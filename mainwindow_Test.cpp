#include "mainwindow.h"
#include "qsqlerror.h"
#include "tcpclientcore.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>
#include <QThread>
#include <QSlider>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlDatabase>
#include "qsqldatabase.h"
#include "box.h"
#include "reagentbottle.h"
#include "slot.h"
#include "ui_mainwindow.h"
#include <QNetworkInterface>
#include <QNetworkAddressEntry>
#include <QAbstractSocket>
#include <QHostAddress>
#include <QSettings>

// ========== 网络配置 ==========
// 网络连接类型选择：true=使用无线网络，false=使用有线网络
const bool USE_WIRELESS_NETWORK = false;  // 您可以修改这个值来切换网络类型

void MainWindow::initializeSystemComponents()
{
    // ========== 初始化数据库表==========
    {
        dbm = new AppSqlDatabase(this);
        // 默认表结构与初始数据由 AppSqlDatabase 构造函数自动完成
    }

    // ========== 检查并重置中断的配方（开机时调用）==========
    checkAndResetInterruptedRecipes();



    // ========== 初始化转移区左边区域（15槽位）==========
    // 这里传入this是为了将MainWindow作为Box的父对象，从而利用Qt的对象树管理Box的生命周期
    transferAreaBox = new Box(15, "Box-Transfer-Area-Left", this); // 使用带名称的构造函数，自动从配置文件加载坐标信息

    // ========== 初始化ABC试剂 ==========
    // A试剂
    reagentA = new ReagentBottle();
    reagentA->setName("A试剂");
    reagentA->setInitial(100);
    reagentA->setRemaining(90);
    reagentA->setHeight(95);
    reagentA->setPos(0, 0);
    transferAreaBox->addReagentBottleToSlot(0, reagentA);  // 放在槽位0
    qDebug() << "A试剂初始化完成，放入槽位0";

    // B试剂
    reagentB = new ReagentBottle();
    reagentB->setName("B试剂");
    reagentB->setInitial(100);
    reagentB->setRemaining(191);
    reagentB->setHeight(95);
    reagentB->setPos(1, 0);
    transferAreaBox->addReagentBottleToSlot(1, reagentB);  // 放在槽位1
    qDebug() << "B试剂初始化完成，放入槽位1";

    // C试剂
    reagentC = new ReagentBottle();
    reagentC->setName("C试剂");
    reagentC->setInitial(100);
    reagentC->setRemaining(92);
    reagentC->setHeight(95);
    reagentC->setPos(2, 0);
    transferAreaBox->addReagentBottleToSlot(2, reagentC);  // 放在槽位2
    qDebug() << "C试剂初始化完成，放入槽位2";

    // 读取试剂信息
    if (transferAreaBox->hasBottle(0)) {
        auto *rb = transferAreaBox->bottleAt(0);
        qDebug() << "槽位0:" << rb->getName() << "剩余:" << rb->getRemaining() << "ml";
    }

    // ==========     初始化TCP用来收取485信息     ==========
    tcpCore = new TcpClientCore(this);
    tcpCore->initializeConnectionsAndTimers();
    
    // ========== 初始化TCP用来收取来自天平的串口信息 ==========
    tcpBalanceCore = new TcpClientCore(this);
    tcpBalanceCore->initializeConnectionsForBalance();

    // 连接天平实时重量信号 -> 状态栏实时显示（当前/目标/目的）
    connect(tcpBalanceCore, &TcpClientCore::balanceWeightReceived,
            this, &MainWindow::updateWeightStatusBar);

    // connetct tcpBalanceCore发出weightReached信号时，tcpCore发送停止命令
    connect(tcpBalanceCore, &TcpClientCore::weightReached, this, [=](double weight) {
 
        
        qDebug() << "★★★★★★★★★★★★★★★★★★★★★★★★★打断重量值:" << weight << "g\n\n\n";
        

        // 暂停队列
        tcpCore->pauseQueue();

        // 先发送停止命令
        tcpCore->writeBalanceTareCommand(">01K0EE65", TcpClientCore::AsciiMode);
        
        // 5秒后继续队列
        QTimer::singleShot(10000, this, [=]() {
            qDebug() << "⏰ 10秒暂停结束，恢复队列";
            tcpCore->resumeQueue();
        });
    });
    
    // 连接 tcpCore 的 balancePrintOffRequested 信号，让 tcpBalanceCore 【【【断开接收】】】
    connect(tcpCore, &TcpClientCore::balancePrintOffRequested, this, [=]() {
        qDebug() << "收到AA0命令，关闭天平打印，断开接收";
        tcpBalanceCore->disconnectReceiveForBalance();
    });
    
    // 连接 tcpCore 的 balancePrintOnRequested 信号，让 tcpBalanceCore 【【【连接接收】】】
    connect(tcpCore, &TcpClientCore::balancePrintOnRequested, this, [=]() {
        qDebug() << "收到AA1命令，打开天平打印，连接接收";
        tcpBalanceCore->connectReceiveForBalance();
    });
    


    // 连接 tcpCore 的 setExpectedWeightRequested 信号，让 tcpBalanceCore 设置期望重量
    connect(tcpCore, &TcpClientCore::setExpectedWeightRequested, this, [=](double weight) {
        tcpBalanceCore->setExpectedWeight(weight);
    });
    
    // 连接 tcpCore 的 balanceTareRequested 信号，让 tcpBalanceCore 【【执行去皮命令】】
    connect(tcpCore, &TcpClientCore::balanceTareRequested, this, [=]() {
        qDebug() << "发送去皮命令（十六进制：540D0A = T\\r\\n）";
        tcpBalanceCore->writeBalanceTareCommand("540D0A", TcpClientCore::HexMode);
    });
    
    /*
    * AA0  天平打印关
    * AA1  天平打印开
    * AA2  天平去皮
    * AAcloseShakeBed 关摇床
    * AAopenShakeBed  开摇床
    * AArecordShakeBedTime     记录摇床需要的时间
    * 第三步骤
    */
    // 连接 tcpCore 的 recordShakeBedTimeRequested 信号，执行记录摇床时间
    connect(tcpCore, &TcpClientCore::recordShakeBedTimeRequested, this, [=](int selfLocation, int shakeDurationSeconds) {
        qDebug() << "收到AArecordShakeBedTime命令，selfLocation =" << selfLocation << "shakeDurationSeconds =" << shakeDurationSeconds;
        // 使用传递的参数记录摇床时间
        recordShakeBedTime(selfLocation, shakeDurationSeconds);
    });

    // 连接 tcpCore 的 recipeProcessStateChangeRequested 信号，修改当前配方的 processState
    connect(tcpCore, &TcpClientCore::recipeProcessStateChangeRequested, this, [=](int newState) {
        qDebug() << "收到AAsetRecipeProcessState命令，准备将当前配方状态改为 =" << newState;
        if (newState < RecipeNotProcessed || newState > RecipeFinished) {
            qWarning() << "无效的配方状态值:" << newState << "，忽略";
            return;
        }

        // 找到当前处于"正在执行"状态的配方（同一时刻只有一个配方处于 RecipeProcessing 状态）
        // 因为 TcpClientCore::sendMessage 一次只导入一个配方，并将其状态设为 RecipeProcessing
        bool found = false;
        for (auto &recipe : m_recipeMessageQueues) {
            if (recipe.processState == RecipeProcessing) {
                recipe.processState = static_cast<RecipeProcessState>(newState);
                qDebug() << "已将配方" << recipe.recipeName << "（创建时间:" << recipe.createTime.toString("yyyy-MM-dd hh:mm:ss") << "）的状态从 RecipeProcessing 更新为" << newState;
                found = true;
                break;
            }
        }
        if (!found) {
            qWarning() << "未找到处于 RecipeProcessing 状态的配方，无法更新状态";
        }
    });
    
    // 连接 tcpCore 的 processStateChanged 信号，更新流程状态显示
    connect(tcpCore, &TcpClientCore::processStateChanged, this, &MainWindow::onProcessStateChanged);

    // 为每个步骤勾选框添加右键菜单（用于执行过程中取消步骤）
    auto setupContextMenu = [this](QCheckBox* checkBox, const QString& stepName) {
        if (!checkBox) return;
        checkBox->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(checkBox, &QWidget::customContextMenuRequested, this, [this, checkBox, stepName](const QPoint& pos) {
            QMenu menu;
            QAction* cancelAction = menu.addAction("取消此步骤");
            // 只有在执行过程中才能取消
            cancelAction->setEnabled(tcpCore && tcpCore->m_isProcessingQueue && !m_skippedSteps.contains(stepName));

            if (menu.exec(checkBox->mapToGlobal(pos)) == cancelAction) {
                cancelStep(stepName);
            }
        });
    };

    setupContextMenu(m_processCheckBox_takeEmptyBottle, "takeEmptyBottle");
    setupContextMenu(m_processCheckBox_getSolid, "getSolid");
    setupContextMenu(m_processCheckBox_resetXYZ, "resetXYZ");
    setupContextMenu(m_processCheckBox_getLiquid, "getLiquid");
    setupContextMenu(m_processCheckBox_tightenBottle, "capBottleAndTransferToShaker");

    // 连接 tcpCore 的 stepSkipped 信号，更新UI显示
    connect(tcpCore, &TcpClientCore::stepSkipped, this, &MainWindow::onStepSkipped);

    // 连接 tcpCore 的 openShakeBedRequested 信号，执行启动摇床
    connect(tcpCore, &TcpClientCore::openShakeBedRequested, this, [=]() {
        qDebug() << "收到AAopenShakeBed命令，执行启动摇床";
        controlShakeBed(true, true); // 同步启动摇床
    });
    
    // 连接 tcpCore 的 closeShakeBedRequested 信号，执行关闭摇床
    connect(tcpCore, &TcpClientCore::closeShakeBedRequested, this, [=]() {
        qDebug() << "收到AAcloseShakeBed命令，执行关闭摇床";
        controlShakeBed(false, true); // 同步关闭摇床
    });
    
    // 连接 tcpCore 的 shakeBedForSecondsRequested 信号，启动摇床X秒后自动关闭
    connect(tcpCore, &TcpClientCore::shakeBedForSecondsRequested, this, [=](int seconds) {
        qDebug() << "收到AAshakeBedForSeconds命令，启动摇床" << seconds << "秒后关闭";
        controlShakeBed(true, true); // 同步启动摇床
        QTimer::singleShot(seconds * 1000, this, [=]() {
            qDebug() << "摇床运行" << seconds << "秒完成，正在关闭";
            controlShakeBed(false, true); // 同步关闭摇床
        });
    });
    
    // 连接 tcpCore 的 emptyBottleAreaCurrentIndexPlusOneRequested 信号，执行空瓶区currentIndex加1
    connect(tcpCore, &TcpClientCore::emptyBottleAreaCurrentIndexPlusOneRequested, this, [=]() {
        qDebug() << "收到AAemptyBottleAreaCurrentIndexPlusOne命令，执行空瓶区currentIndex加1";
        incrementDatabaseField("other", "currentIndex", "name = 'emptyBottleArea'");
    });
    
    // 连接 tcpCore 的 tipsHeadAreaCurrentIndexPlusOneRequested 信号，执行tips头区currentIndex加1
    connect(tcpCore, &TcpClientCore::tipsHeadAreaCurrentIndexPlusOneRequested, this, [=](int tipsHeadUsageSelfLocation) {
        qDebug() << "收到AAtipsHeadAreaCurrentIndexPlusOne命令，需要改成被标记的序号是 =" << tipsHeadUsageSelfLocation << "，标记成被标记状态。";

        if (!dbm) {
            qWarning() << "数据库对象未初始化，无法处理tipsHeadArea切换逻辑";
            return;
        }
        // 去表里找到selfLocation为tipsHeadUsageSelfLocation的那一行，把status值改为0（表示已使用）
        QString tipsHeadUsageSql = QString("UPDATE tipsHeadUsage SET status = 0 WHERE selfLocation = %1").arg(tipsHeadUsageSelfLocation);
        QSqlQuery tipsHeadUsageQuery = dbm->query(tipsHeadUsageSql);
        if (tipsHeadUsageQuery.lastError().isValid()) {
            qWarning() << "更新tipsHeadUsage表失败:" << tipsHeadUsageQuery.lastError().text();
        } else {
            qDebug() << QString("已更新tipsHeadUsage位置 %1 的status自增1").arg(tipsHeadUsageSelfLocation);
        }

        // 如果tipsHeadUsageSelfLocation为95，需要切换tipsHeadArea的currentIndex并重置所有tipsHeadUsage的status
        if (tipsHeadUsageSelfLocation == 95) {
            // 查询tipsHeadArea的currentIndex
            QString tipsHeadAreaSql = "SELECT currentIndex FROM other WHERE name = 'tipsHeadArea'";
            QSqlQuery tipsHeadAreaQuery = dbm->query(tipsHeadAreaSql);
            int currentIndex = -1;
            if (tipsHeadAreaQuery.next()) {
                currentIndex = tipsHeadAreaQuery.value("currentIndex").toInt();
            } else {
                qWarning() << "未找到 tipsHeadArea 的数据";
                return;
            }

            // 根据currentIndex的值进行切换
            int newCurrentIndex = -1;
            if (currentIndex == 0) {
                newCurrentIndex = 1;
            } else if (currentIndex == 1) {
                newCurrentIndex = 0;
            } else {
                qWarning() << "tipsHeadArea的currentIndex值异常:" << currentIndex << "，无法执行切换";
                return;
            }

            // 更新tipsHeadArea的currentIndex
            QString updateTipsHeadAreaSql = QString("UPDATE other SET currentIndex = %1 WHERE name = 'tipsHeadArea'").arg(newCurrentIndex);
            QSqlQuery updateTipsHeadAreaQuery = dbm->query(updateTipsHeadAreaSql);
            if (updateTipsHeadAreaQuery.lastError().isValid()) {
                qWarning() << "更新tipsHeadArea的currentIndex失败:" << updateTipsHeadAreaQuery.lastError().text();
            } else {
                qDebug() << QString("已切换tipsHeadArea的currentIndex从 %1 到 %2").arg(currentIndex).arg(newCurrentIndex);
            }

            // 重置tipsHeadUsage表所有记录的status为1
            QString resetTipsHeadUsageSql = "UPDATE tipsHeadUsage SET status = 1";
            QSqlQuery resetTipsHeadUsageQuery = dbm->query(resetTipsHeadUsageSql);
            if (resetTipsHeadUsageQuery.lastError().isValid()) {
                qWarning() << "重置tipsHeadUsage表所有status失败:" << resetTipsHeadUsageQuery.lastError().text();
            } else {
                qDebug() << "已重置tipsHeadUsage表所有记录的status为1";
            }
        }

    });

    // 连接 tcpCore 的 allDevicesInitializedRequested 信号，在初始化队列真正执行完成时标记全局状态
    connect(tcpCore, &TcpClientCore::allDevicesInitializedRequested, this, [=]() {
        if (!m_allDevicesInitialized) {
            m_allDevicesInitialized = true;
            qDebug() << "收到AAallDevicesInitialized命令，所有设备初始化队列已执行完成，m_allDevicesInitialized = true";
        }
    });

    // 连接 tcpCore 的 leaveTheShakerRequested 信号，执行摇床离开逻辑
    connect(tcpCore, &TcpClientCore::leaveTheShakerRequested, this, [=](int selfLocation) {
        qDebug() << "收到AAleaveTheShaker命令，selfLocation =" << selfLocation << "，执行摇床离开逻辑";

        if (!dbm) {
            qWarning() << "数据库对象未初始化，无法处理摇床离开逻辑";
            return;
        }

        if (!tcpCore) {
            qWarning() << "TCP核心对象未初始化，无法处理摇床离开逻辑";
            return;
        }

        // 可选：更新数据库，将isEmpty设置为1（表示摇床已停止），并清理开始时间和结束时间
        QString updateSql = QString("UPDATE shakeBedArea SET isEmpty = 1, startTime = '', endTime = '' WHERE selfLocation = %1")
                .arg(selfLocation);
        QSqlQuery updateQuery = dbm->query(updateSql);
        if (updateQuery.lastError().isValid()) {
            qWarning() << "更新shakeBedArea表失败:" << updateQuery.lastError().text();
        } else {
            qDebug() << QString("已更新摇床位置 %1 的状态为空").arg(selfLocation);
        }

    });

    // 连接 tcpCore 的 responseTimeoutFailed 信号：响应超时重试失败后触发紧急暂停
    connect(tcpCore, &TcpClientCore::responseTimeoutFailed, this, [=](const QString& command, const QString& expectedResponse) {
        qCritical() << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━";
        qCritical() << "收到响应超时失败信号，触发紧急暂停";
        qCritical() << "失败命令:" << command;
        qCritical() << "期望响应:" << expectedResponse;
        qCritical() << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━";

        // 触发紧急暂停按钮
        if (ui && ui->pushButton_Stop) {
            QMetaObject::invokeMethod(ui->pushButton_Stop, "click", Qt::QueuedConnection);
        } else {
            qWarning() << "无法触发紧急暂停按钮：UI对象未初始化";
        }
    });

    // 连接 tcpCore 的 messageQueueEmpty 信号：当前配方消息执行完毕，自动从数据库加载下一个配方
    connect(tcpCore, &TcpClientCore::messageQueueEmpty, this, [=]() {
        qDebug() << "TcpClientCore 队列已空，当前配方执行完毕";
        
        // 1. 将数据库中当前正在执行的配方标记为"执行完毕"
        if (dbm) {
            QString updateSql = QString("UPDATE recipeQueue SET processState = %1 WHERE processState = %2")
                .arg(RecipeFinished).arg(RecipeProcessing);
            QSqlQuery updateQuery = dbm->query(updateSql);
            if (updateQuery.lastError().isValid()) {
                qWarning() << "更新配方状态为执行完毕失败:" << updateQuery.lastError().text();
            } else {
                int rowsAffected = updateQuery.numRowsAffected();
                if (rowsAffected > 0) {
                    qDebug() << "已将" << rowsAffected << "个配方标记为执行完毕";
                }
            }
        }
        
        // 2. 从数据库加载并执行下一个未执行的配方
        qDebug() << "尝试从数据库加载下一个未执行的配方";
        if (!loadAndExecuteNextRecipeFromDatabase()) {
            qDebug() << "没有更多待执行的配方";
        }
    });



    // ========== 按钮连接 ==========
    // ========== 按钮连接 ==========
    // ========== 按钮连接 ==========
    // ========== 按钮连接 ==========
    // ========== 按钮连接 ==========
    // ========== 按钮连接 ==========
    // ========== 按钮连接 ==========
    // ========== 按钮连接 ==========
    // ========== 按钮连接 ==========
    // ========== 按钮连接 ==========
    // ========== 按钮连接 ==========
    // ========== 按钮连接 ==========
    // 按钮1：连接并发送测试命令
    connect(ui->pushButton, &QPushButton::clicked, this, [=] {
        // 从 BoxData.ini 读取 TCP 连接参数（与网络设置对话框共用同一数据源）
        QSettings tcpIni("BoxData.ini", QSettings::IniFormat);
        tcpIni.beginGroup("TCP");
        QString savedLocalIP          = tcpIni.value("localIP",              "").toString();
        QString tcpCoreRemoteIP       = tcpIni.value("tcpCoreRemoteIP",      "192.168.5.201").toString();
        quint16 tcpCoreRemotePort     = static_cast<quint16>(tcpIni.value("tcpCoreRemotePort",    4196).toUInt());
        QString tcpBalanceRemoteIP    = tcpIni.value("tcpBalanceRemoteIP",   "192.168.5.201").toString();
        quint16 tcpBalanceRemotePort  = static_cast<quint16>(tcpIni.value("tcpBalanceRemotePort", 4197).toUInt());
        tcpIni.endGroup();

        // 本机 IP：优先用 ini 中保存的值，否则自动查找有线 IP
        QString localIP = savedLocalIP;
        if (localIP.isEmpty()) {
            if (USE_WIRELESS_NETWORK) {
                localIP = getLocalWirelessIP();
                if (localIP.isEmpty()) {
                    localIP = "192.168.5.78";
                    qDebug() << "未找到无线网口IP，使用默认IP地址:" << localIP;
                } else {
                    qDebug() << "使用自动获取的无线网口IP地址:" << localIP;
                }
            } else {
                localIP = getLocalWiredIP();
                if (localIP.isEmpty()) {
                    localIP = "192.168.5.78";
                    qDebug() << "未找到有线网口IP，使用默认IP地址:" << localIP;
                } else {
                    qDebug() << "使用自动获取的有线网口IP地址:" << localIP;
                }
            }
        }

        // 将上次意外中断的配方（processState=1 正在执行）标记为已完成（processState=2）
        if (dbm) {
            dbm->query(QString("UPDATE recipeQueue SET processState = %1 WHERE processState = %2")
                       .arg(RecipeFinished).arg(RecipeProcessing));
            qDebug() << "连接前清理：将遗留的 processState=1 记录标记为 processState=2";
        }

        // 连接到TCP服务器
        bool okMain    = tcpCore->connectToTcp(localIP, tcpCoreRemoteIP,    tcpCoreRemotePort,    true);
        bool okBalance = tcpBalanceCore->connectToTcp(localIP, tcpBalanceRemoteIP, tcpBalanceRemotePort, true);

        // 任意一个连接失败，都不继续后续初始化
        if (!okMain || !okBalance) {
            qWarning() << "TCP 连接失败，停止后续初始化。主机连接结果:" << okMain
                       << "天平连接结果:" << okBalance;
            return;
        }


        RecipeQueueItem newRecipe;
        newRecipe.recipeName = "开机初始化";
        newRecipe.createTime = QDateTime::currentDateTime();
        newRecipe.processState = RecipeNotProcessed;   // 未处理

        // 调用设备初始化函数，将初始化相关命令写入队列
        initializeAllDevices(newRecipe.messageQueue);
        newRecipe.messageQueue.enqueue(MessageQueueItem("AAallDevicesInitialized", true));

        // 保存到数据库并执行
        saveAndExecuteRecipe(newRecipe);

        // 启动摇床检查定时器
        if (shakeBedCheckTimer && !shakeBedCheckTimer->isActive()) {
            shakeBedCheckTimer->start(1000);  // 每隔1秒检查一次
            qDebug() << "摇床检查定时器已启动";
        }

    });
    
    // 按钮2：断开连接
    connect(ui->pushButton_2, &QPushButton::clicked, this, [=] {

        qDebug() << "断开TCP连接";
        tcpCore->disconnectFromTcp();
        tcpBalanceCore->disconnectFromTcp();

        // 停止摇床检查定时器
        if (shakeBedCheckTimer && shakeBedCheckTimer->isActive()) {
            shakeBedCheckTimer->stop();
            qDebug() << "摇床检查定时器已停止";
        }
        // ========== 检查并重置中断的配方（开机时调用）==========
        checkAndResetInterruptedRecipes();
        tcpCore->clearMessageQueue();

    });


    // 按钮4：复位
    connect(ui->pushButton_4, &QPushButton::clicked, this, [=] {

        QTimer::singleShot(0, this, [=]() {
            // 恢复复位时的清理：停止轮询/清空队列/复位等待标志
            if (!tcpCore) {
                qWarning() << "tcpCore 未初始化";
                return;
            }
            
            tcpCore->resetState();
            qDebug() << "复位";

            // 先张开移动夹爪
            QString openMoveGripCommand = tcpCore->buildDeviceCommand("05", "06", "0105", 0, 4);
            tcpCore->sendMessageAsync(openMoveGripCommand.toUtf8(), false);
            QString waitOpenMoveGripCommand = tcpCore->buildDeviceCommand("05", "03", "0202", 1, 4);
            tcpCore->sendMessageAsync(waitOpenMoveGripCommand.toUtf8(), false, "0503020001");


            RecipeQueueItem newRecipe;
            newRecipe.recipeName = "复位初始化";
            newRecipe.createTime = QDateTime::currentDateTime();
            newRecipe.processState = RecipeNotProcessed;   // 未处理
            initializeAllDevices(newRecipe.messageQueue);
            
            // 保存到数据库并执行
            saveAndExecuteRecipe(newRecipe);
        });
    });

}















































// 测试配方发送功能（接收JSON对象）
void MainWindow::testRecipeSend(const QJsonObject& recipePacket)
{
    // ============ 步骤1：创建新的配方队列项 ============
    RecipeQueueItem newRecipe;

    // 打印核心三部分
    qDebug() << "========== 收到配方（三部分） ==========";
    const QString equation = recipePacket.value("化学方程式").toString();
    qDebug() << "化学方程式:" << equation;

    // ============ 步骤2：设置配方信息 ============
    newRecipe.recipeName = equation;                     // 使用化学方程式作为配方名称
    newRecipe.createTime = QDateTime::currentDateTime(); // 记录创建时间
    newRecipe.processState = RecipeNotProcessed;         // 配方初始为“未处理”

    // 获取当前配方的索引（最后一个）
    int currentRecipeIndex = m_recipeMessageQueues.size() - 1;
    qDebug() << "创建新配方队列:";
    qDebug() << "  - 配方名称:" << newRecipe.recipeName;
    qDebug() << "  - 创建时间:" << newRecipe.createTime.toString("yyyy-MM-dd hh:mm:ss");
    qDebug() << "  - 队列索引:" << currentRecipeIndex;
    qDebug() << "  - 当前总配方数:" << m_recipeMessageQueues.size();
    // 溶质
    QJsonArray solutes = recipePacket.value("溶质").toArray();
    qDebug() << "溶质条目数:" << solutes.size();
    for (const auto &v : std::as_const(solutes)) {
        const QJsonObject o = v.toObject();
        const QString name = o.value("名称").toString();
        const double amount = o.value("用量").toDouble();
        const QString unit = o.value("单位").toString();
        qDebug() << "溶质-名称:" << name
                 << "用量:" << amount
                 << unit;
    }
    // 溶剂
    QJsonArray solvents = recipePacket.value("溶剂").toArray();
    qDebug() << "溶剂条目数:" << solvents.size();
    for (const auto &v : std::as_const(solvents)) {
        const QJsonObject o = v.toObject();
        const QString name = o.value("名称").toString();
        const double amount = o.value("用量").toDouble();
        const QString unit = o.value("单位").toString();
        qDebug() << "溶剂-名称:" << name
                 << "用量:" << amount
                 << unit;
    }


    // 这里实现类似AA那种发出信号去改processState状态：
    // 在配方队列的适当位置插入一个标记命令
    {
        int desiredState = RecipeProcessing;
        QString stateCmd = QString("AAsetRecipeProcessState:%1").arg(desiredState);
        newRecipe.messageQueue.enqueue(MessageQueueItem(stateCmd.toUtf8(), true));
    }

    // 打开空瓶
    takeEmptyBottle("Box_Transfer_Area_Right", newRecipe.messageQueue);


    // 取固体：遍历溶质，传入名称与质量（g）
    for (const auto &v : std::as_const(solutes)) {
        const QJsonObject o = v.toObject();
        const QString name = o.value("名称").toString();
        const double mass = o.value("用量").toDouble();
        if (name.isEmpty() || qFuzzyIsNull(mass)) {
            qWarning() << "溶质参数不完整，跳过：" << o;
            continue;
        }
        qDebug() << "准备取固体:" << name << "目标质量(g):" << mass/1000;
        getSolid(name, mass/1000, newRecipe.messageQueue);
    }

    resetXYZMotorsToZero(newRecipe.messageQueue);

    // 取液体：遍历溶剂，传入名称与体积（ml）
    for (const auto &v : std::as_const(solvents)) {
        const QJsonObject o = v.toObject();
        const QString name = o.value("名称").toString();
        const double volume = o.value("用量").toDouble();
        if (name.isEmpty() || qFuzzyIsNull(volume)) {
            qWarning() << "溶剂参数不完整，跳过：" << o;
            continue;
        }
        qDebug() << "准备取液体:" << name << "目标体积(ml):" << volume;
        getLiquid(name, volume, newRecipe.messageQueue);
    }

    // 拧盖并送入摇床
    capBottleAndTransferToShaker(newRecipe.messageQueue);

    {
        int desiredState = RecipeFinished;
        QString stateCmd = QString("AAsetRecipeProcessState:%1").arg(desiredState);
        newRecipe.messageQueue.enqueue(MessageQueueItem(stateCmd.toUtf8(), true));
    }

    // 保存到数据库并执行
    saveAndExecuteRecipe(newRecipe);
}





































// ========== 初始化 ==========
// ========== 初始化 ==========
// ========== 初始化 ==========
// ========== 初始化 ==========
// ========== 初始化 ==========
// ========== 初始化 ==========
// ========== 初始化 ==========
// ========== 初始化 ==========
// ========== 初始化 ==========
// ========== 初始化 ==========
void MainWindow::initializeAllDevices(QQueue<MessageQueueItem>& messageQueue)
{

    Q_UNUSED(messageQueue);

    // 电机速度恢复 已知的有5号电机的旋转和6号电机的Z轴
    setMotor5Speed(100, messageQueue);
    setMotor6ZSpeed(1000, messageQueue);


    qDebug() << "消磁开始";
    QString closeElectromagnetCommand = tcpCore->buildDeviceCommand("0D", "05", "00000000", 0, 0);
    messageQueue.enqueue(MessageQueueItem(closeElectromagnetCommand.toUtf8(), false, "0D0500000000"));


    qDebug() << "\n\n固体电机初始化"; // 1号电机(跟A电机逻辑相似)
    QString solidMotorInitializeCommand = tcpCore->buildDeviceCommand("01", "f", 0, 0); // 归零
    messageQueue.enqueue(MessageQueueItem(solidMotorInitializeCommand.toUtf8(), true, "01f"));
    QString waitSolidMotorInitializeCommand = tcpCore->buildDeviceCommand("01", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitSolidMotorInitializeCommand.toUtf8(), true, "01d01"));

    qDebug() << "\n\nz固体电机初始化"; // 2号电机
    QString zMotorInitializeCommand = tcpCore->buildDeviceCommand("02", "G", 0, 0); // 归零
    messageQueue.enqueue(MessageQueueItem(zMotorInitializeCommand.toUtf8(), true, "02G"));
    QString waitZMotorInitializeCommand = tcpCore->buildDeviceCommand("02", "g", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitZMotorInitializeCommand.toUtf8(), true, "02g01"));

    qDebug() << "\n\ny固体电机初始化"; // 3号电机
    QString yMotorInitializeCommand = tcpCore->buildDeviceCommand("03", "G", 0, 0); // 归零
    messageQueue.enqueue(MessageQueueItem(yMotorInitializeCommand.toUtf8(), true, "03G"));
    QString waitYMotorInitializeCommand = tcpCore->buildDeviceCommand("03", "g", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitYMotorInitializeCommand.toUtf8(), true, "03g01"));
    
    qDebug() << "\n\nx固体电机初始化"; // 4号电机
    QString xMotorInitializeCommand = tcpCore->buildDeviceCommand("04", "G", 0, 0); // 归零
    messageQueue.enqueue(MessageQueueItem(xMotorInitializeCommand.toUtf8(), true, "04G"));
    QString waitXMotorInitializeCommand = tcpCore->buildDeviceCommand("04", "g", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitXMotorInitializeCommand.toUtf8(), true, "04g01"));


    qDebug() << "\n\n泵初始化"; // 7号电机
    QString initializePumpCommand = tcpCore->buildDeviceCommand("07", "G", 0, 0);
    messageQueue.enqueue(MessageQueueItem(initializePumpCommand.toUtf8(), true, "07G"));
    QString waitPumpInitializedCommand = tcpCore->buildDeviceCommand("07", "g", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitPumpInitializedCommand.toUtf8(), true, "07g01"));

    qDebug() << "\n\nz泵初始化"; // 8号电机
    QString initializeZPumpCommand = tcpCore->buildDeviceCommand("08", "G", 0, 0);
    messageQueue.enqueue(MessageQueueItem(initializeZPumpCommand.toUtf8(), true, "08G"));
    QString waitZPumpInitializedCommand = tcpCore->buildDeviceCommand("08", "g", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitZPumpInitializedCommand.toUtf8(), true, "08g01"));
    // 08移动到4的位置
    QString moveToZ4Command = tcpCore->buildDeviceCommand("08", "D", 100, 8); // 00000004
    messageQueue.enqueue(MessageQueueItem(moveToZ4Command.toUtf8(), true, "08D"));
    QString waitZ4Command = tcpCore->buildDeviceCommand("08", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitZ4Command.toUtf8(), true, "08d01"));
    
    qDebug() << "\n\n移动电爪初始化"; // 5号电机
    QString initializeGripperCommand = tcpCore->buildDeviceCommand("05", "06", "0100", 1, 4);
    messageQueue.enqueue(MessageQueueItem(initializeGripperCommand.toUtf8(), false, "050601000001"));
    QString waitGripperInitializedCommand = tcpCore->buildDeviceCommand("05", "03", "0200", 1, 4);
    messageQueue.enqueue(MessageQueueItem(waitGripperInitializedCommand.toUtf8(), false, "0503020001"));   //  05 03 0200 0001
    QString configureGripperModeCommand = tcpCore->buildDeviceCommand("05", "06", "0101", 1, 4);
    messageQueue.enqueue(MessageQueueItem(configureGripperModeCommand.toUtf8(), false, "050601010001"));
    QString waitGripperModeConfiguredCommand = tcpCore->buildDeviceCommand("05", "03", "0201", 1, 4);
    messageQueue.enqueue(MessageQueueItem(waitGripperModeConfiguredCommand.toUtf8(), false, "0503020001"));  // 0503020  10001

    qDebug() << "\n\nz移动电爪初始化"; // 6号电机
    QString initializeZAxisCommand = tcpCore->buildDeviceCommand("06", "G", 0, 0);
    messageQueue.enqueue(MessageQueueItem(initializeZAxisCommand.toUtf8(), true, "06G"));
    QString waitZAxisInitializedCommand = tcpCore->buildDeviceCommand("06", "g", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitZAxisInitializedCommand.toUtf8(), true, "06g01"));

    // 启动摇床，3秒后自动关闭（作为初始化）
    messageQueue.enqueue(MessageQueueItem("AAshakeBedForSeconds:3", true));

    qDebug() << "\n\n初始化X"; // 10号电机
    QString initializeXAxisCommand = tcpCore->buildDeviceCommand("0A", "G", 0, 0);
    messageQueue.enqueue(MessageQueueItem(initializeXAxisCommand.toUtf8(), true, "0AG"));
    QString waitXAxisInitializedCommand = tcpCore->buildDeviceCommand("0A", "g", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitXAxisInitializedCommand.toUtf8(), true, "0Ag01"));

    qDebug() << "\n\n初始化Y"; // 9号电机
    QString initializeYAxisCommand = tcpCore->buildDeviceCommand("09", "G", 0, 0);
    messageQueue.enqueue(MessageQueueItem(initializeYAxisCommand.toUtf8(), true, "09G"));
    QString waitYAxisInitializedCommand = tcpCore->buildDeviceCommand("09", "g", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitYAxisInitializedCommand.toUtf8(), true, "09g01"));

    qDebug() << "\n\n夹持区初始化"; // 11号电机
    QString initializeGripAreaCommand = tcpCore->buildDeviceCommand("0B", "06", "0100", 1, 4);
    messageQueue.enqueue(MessageQueueItem(initializeGripAreaCommand.toUtf8(), false, "0B0601000001"));
    QString waitGripAreaInitializedCommand = tcpCore->buildDeviceCommand("0B", "03", "0200", 1, 4);
    messageQueue.enqueue(MessageQueueItem(waitGripAreaInitializedCommand.toUtf8(), false, "0B03020001"));
}



// 摇床初始化（摇3秒后停止）
void MainWindow::initializeShakeBed(QQueue<MessageQueueItem>& messageQueue)
{
    // 摇床是C，12号机器
    // 初始化：启动摇床，摇3秒后停止

    // 1. 初始化摇匀设备
    QString initCommand = tcpCore->buildMessageWithCrc(QStringLiteral(">0CG"));
    messageQueue.enqueue(MessageQueueItem(initCommand.toUtf8(), true, "0CG"));

    // 2. 查询初始化状态
    QString queryInitCommand = tcpCore->buildMessageWithCrc(QStringLiteral(">0cg"));
    messageQueue.enqueue(MessageQueueItem(queryInitCommand.toUtf8(), true, "0Cg01"));

    // 3. 启动摇匀（速度300rpm，加速度300）
    QString startCommand = tcpCore->buildMessageWithCrc(QStringLiteral(">0Cxi3000000012c0000012c"));
    qDebug() << "摇床初始化启动命令:" << startCommand;

    // 断开之前的连接（如果存在）
    if (m_shakeBedInitConnection) {
        disconnect(m_shakeBedInitConnection);
        m_shakeBedInitConnection = QMetaObject::Connection();
    }

    // 使用一次性连接，等待启动命令的回复后再开始3秒倒计时
    m_shakeBedInitConnection = connect(tcpCore, &TcpClientCore::dataReceived, this, [this](const QByteArray& data) {
        QString dataStr = QString::fromUtf8(data);
        // 检查是否是摇床启动命令的回复（包含 "0Cxi300" 但不包含 "0Cxi301"）
        if (dataStr.contains("0Cxi300") && !dataStr.contains("0Cxi301")) {
            qDebug() << "收到摇床启动回复，开始3秒倒计时";
            // 断开这个一次性连接
            if (m_shakeBedInitConnection) {
                disconnect(m_shakeBedInitConnection);
                m_shakeBedInitConnection = QMetaObject::Connection();
            }

            // 等待3秒后停止（作为初始化完成）
            QTimer::singleShot(3000, this, [this]() {
                QString stopCommand = tcpCore->buildMessageWithCrc(QStringLiteral(">0Cxi30100000000"));
                qDebug() << "摇床初始化停止命令:" << stopCommand;
                // 直接发送，不加入队列（使用writeBalanceTareCommand函数）
                tcpCore->writeBalanceTareCommand(stopCommand, TcpClientCore::StringMode);
                qDebug() << "摇床初始化完成（已摇3秒）";
            });
        }
    }, Qt::QueuedConnection);

    // 发送启动命令
    messageQueue.enqueue(MessageQueueItem(startCommand.toUtf8(), true, "0Cxi300"));
}

// 摇床
void MainWindow::shakeBed(int parameter)
{
    // 摇床是C，12号机器
    // parameter: 摇匀时间（秒），如果为0或负数则只停止

    if (parameter <= 0) {
        // 只停止摇匀
        // 自定义协议格式：>0Cxi30100000000 + CRC
        QString stopCommand = tcpCore->buildMessageWithCrc(QStringLiteral(">0Cxi30100000000"));
        tcpCore->sendMessageAsync(stopCommand.toUtf8(), true, "0Cxi301"); // 等待回复：>0Cxi300

        return;
    }

    // // 1. 初始化摇匀设备
    // QString initCommand = tcpCore->buildMessageWithCrc(QStringLiteral(">0CG"));
    // tcpCore->sendMessageAsync(initCommand.toUtf8(), true, "0CG");

    // // 2. 查询初始化状态
    // QString queryInitCommand = tcpCore->buildMessageWithCrc(QStringLiteral(">0cg"));
    // tcpCore->sendMessageAsync(queryInitCommand.toUtf8(), true, "0Cg01");

    // 3. 启动摇匀（速度300rpm，加速度300）
    // 速度：0000012c = 300 (十进制) = 0x12C
    // 加速度：0000012c = 300 (十进制) = 0x12C
    // 发送：>0Cxi3000000012c0000012c + CRC，回复：>0Cxi300
    QString startCommand = tcpCore->buildMessageWithCrc(QStringLiteral(">0Cxi3000000012c0000012c"));
    qDebug() << "startCommand " << startCommand;
    tcpCore->sendMessageAsync(startCommand.toUtf8(), true, "0Cxi300");

    // // 4. 等待指定时间（parameter秒）
    // QTimer::singleShot(parameter * 1000, this, [this, parameter]() {
    //     // 5. 停止摇匀
    //     // 发送：>0Cxi30100000000 + CRC，回复：>0Cxi300
    //     QString stopCommand = tcpCore->buildMessageWithCrc(QStringLiteral(">0Cxi30100000000"));
    //     qDebug() << "stopCommand" << stopCommand;
    //     tcpCore->sendMessageAsync(stopCommand.toUtf8(), true, "0Cxi301");
    //     qDebug() << "摇床完成，摇匀时间：" << parameter << "秒";

    //     // 摇床完成后放置试剂瓶
    //     placeShakenReagentBottle();

    // });



}

/**
 * xyz轴恢复到零点
 * 将06，08，09，0A号电机恢复到零点
 */
void MainWindow::resetXYZMotorsToZero(QQueue<MessageQueueItem>& messageQueue)
{
    // ★ 插入状态标记：XYZ复位
    messageQueue.enqueue(MessageQueueItem("AAstateChange:resetXYZ", true));

    // 复位时停止天平打印
    if (tcpBalanceCore) {
        tcpBalanceCore->disconnectReceiveForBalance();
    }
    messageQueue.enqueue(MessageQueueItem("AA0", true)); // 关闭天平打印

    // 06号电机恢复到零点
    QString zeroMotorCommand = tcpCore->buildDeviceCommand("06", "D", 0, 8);
    messageQueue.enqueue(MessageQueueItem(zeroMotorCommand.toUtf8(), true, "06D"));
    QString waitZeroMotorCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitZeroMotorCommand.toUtf8(), true, "06d01"));

    // 08号电机恢复到零点
    zeroMotorCommand = tcpCore->buildDeviceCommand("08", "D", 3, 8);
    messageQueue.enqueue(MessageQueueItem(zeroMotorCommand.toUtf8(), true, "08D"));
    waitZeroMotorCommand = tcpCore->buildDeviceCommand("08", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitZeroMotorCommand.toUtf8(), true, "08d01"));

    // 09号电机恢复到零点
    zeroMotorCommand = tcpCore->buildDeviceCommand("09", "D", 0, 8);
    messageQueue.enqueue(MessageQueueItem(zeroMotorCommand.toUtf8(), true));
    waitZeroMotorCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitZeroMotorCommand.toUtf8(), true, "09d01"));

    // 0A号电机恢复到零点
    zeroMotorCommand = tcpCore->buildDeviceCommand("0A", "D", 0, 8);
    messageQueue.enqueue(MessageQueueItem(zeroMotorCommand.toUtf8(), true));
    waitZeroMotorCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitZeroMotorCommand.toUtf8(), true, "0Ad01"));
}

void MainWindow::placeShakenReagentBottle()
{
    // TODO: 实现摇床完成后将试剂瓶放置到指定位置的逻辑
    qDebug() << "摇床完成，准备放置试剂瓶";

    /**
    * 读取摇床区
    *
    * 数据库获取 shakeBedArea 的xyz
    *
    * 数据库获取 shakeBedArea 的rightSpacing, bottomSpacing, cols, rows
    * 计算槽位坐标
    * 0A, 09, 06电机移动到shakeBedArea的xyz  currentIndex
    */
    QString shakeBedAreaSql = "SELECT originX, originY, gripperZ, rightSpacing, bottomSpacing, cols, `rows`, currentIndex FROM other WHERE name = 'shakeBedArea'";
    QSqlQuery shakeBedAreaQuery = dbm->query(shakeBedAreaSql);
    int shakeBedAreaRightSpacing=0, shakeBedAreaBottomSpacing=0, shakeBedAreaCols=0, shakeBedAreaRows=0, shakeBedAreaSlotIndex=0;
    int shakeBedAreaX=0, shakeBedAreaY=0, shakeBedAreaZ=0;
    if (shakeBedAreaQuery.next()) {
        shakeBedAreaX = shakeBedAreaQuery.value("originX").toInt();
        shakeBedAreaY = shakeBedAreaQuery.value("originY").toInt();
        shakeBedAreaZ = shakeBedAreaQuery.value("gripperZ").toInt();
        shakeBedAreaRightSpacing = shakeBedAreaQuery.value("rightSpacing").toDouble();
        shakeBedAreaBottomSpacing = shakeBedAreaQuery.value("bottomSpacing").toDouble();
        shakeBedAreaCols = shakeBedAreaQuery.value("cols").toInt();
        shakeBedAreaRows = shakeBedAreaQuery.value("rows").toInt();
        shakeBedAreaSlotIndex = shakeBedAreaQuery.value("currentIndex").toInt();
    }


    SlotPositionConfig config(shakeBedAreaX, shakeBedAreaY, shakeBedAreaCols, shakeBedAreaRows, shakeBedAreaRightSpacing, shakeBedAreaBottomSpacing);
    QPoint targetPos = calculateSlotPosition(config, shakeBedAreaSlotIndex);
    int shakeBedAreaTargetX = targetPos.x();
    int shakeBedAreaTargetY = targetPos.y();

    QString moveToShakeBedAreaZCommand = tcpCore->buildDeviceCommand("0A", "D", shakeBedAreaTargetX, 8);
    tcpCore->sendMessageAsync(moveToShakeBedAreaZCommand.toUtf8(), true);
    QString waitShakeBedAreaZCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    tcpCore->sendMessageAsync(waitShakeBedAreaZCommand.toUtf8(), true, "0Ad01");

    QString moveToShakeBedAreaYCommand = tcpCore->buildDeviceCommand("09", "D", shakeBedAreaTargetY, 8);
    tcpCore->sendMessageAsync(moveToShakeBedAreaYCommand.toUtf8(), true);
    QString waitShakeBedAreaYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    tcpCore->sendMessageAsync(waitShakeBedAreaYCommand.toUtf8(), true, "09d01");

    QString moveToShakeBedAreaXCommand = tcpCore->buildDeviceCommand("06", "D", shakeBedAreaZ, 8);
    tcpCore->sendMessageAsync(moveToShakeBedAreaXCommand.toUtf8(), true);
    QString waitShakeBedAreaXCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    tcpCore->sendMessageAsync(waitShakeBedAreaXCommand.toUtf8(), true, "06d01");


    /**
     * 夹住瓶子，上移
     *
     * 5号电机夹住瓶子
     * 6号电机上移
     */
    QString enableGripperCommand = tcpCore->buildDeviceCommand("05", "06", "0105", 100, 4);
    tcpCore->sendMessageAsync(enableGripperCommand.toUtf8(), false);
    QString waitGripperEnableCommand = tcpCore->buildDeviceCommand("05", "03", "0202", 1, 4);
    tcpCore->sendMessageAsync(waitGripperEnableCommand.toUtf8(), false, "0503020002");

    QString raiseTransferZCommand = tcpCore->buildDeviceCommand("06", "D", 100, 8);
    tcpCore->sendMessageAsync(raiseTransferZCommand.toUtf8(), true);
    QString waitTransferZRaisedCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    tcpCore->sendMessageAsync(waitTransferZRaisedCommand.toUtf8(), true, "06d01");

    /**
     * 去到成品放置区域
     *
     * 从 pan_init 获取 finashedPositon 的 xyz + 网格参数
     * 从 pan_FinishedProductLocation 找最小空槽 slot_index
     * 0A, 09, 06 电机移动到目标 xyz
     */
    // 盘首与网格参数
    QString finishedPanSql = "SELECT x, y, gripperZ, rightSpacing, bottomSpacing, cols, `rows` FROM pan_init WHERE name = 'finashedPositon'";
    QSqlQuery finishedPanQuery = dbm->query(finishedPanSql);
    int finishedX=0, finishedY=0, finishedZ=0;
    int finishedRightSpacing=0, finishedBottomSpacing=0, finishedCols=1, finishedRows=1;
    if (finishedPanQuery.next()) {
        finishedX             = finishedPanQuery.value("x").toInt();
        finishedY             = finishedPanQuery.value("y").toInt();
        finishedZ             = finishedPanQuery.value("gripperZ").toInt();
        finishedRightSpacing  = finishedPanQuery.value("rightSpacing").toInt();
        finishedBottomSpacing = finishedPanQuery.value("bottomSpacing").toInt();
        finishedCols          = finishedPanQuery.value("cols").toInt();
        finishedRows          = finishedPanQuery.value("rows").toInt();
    } else {
        qCritical() << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━";
        qCritical() << "未找到 pan_init 中 finashedPositon 记录，触发紧急暂停";
        qCritical() << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━";
        if (ui && ui->pushButton_Stop)
            QMetaObject::invokeMethod(ui->pushButton_Stop, "click", Qt::QueuedConnection);
        return;
    }

    // 找 drug_name 为空的最小 slot_index
    QString finishedSlotSql = "SELECT slot_index FROM pan_FinishedProductLocation WHERE (drug_name IS NULL OR drug_name = '') ORDER BY slot_index ASC LIMIT 1";
    QSqlQuery finishedSlotQuery = dbm->query(finishedSlotSql);
    int finishedSlotIndex = -1;
    if (finishedSlotQuery.next()) {
        finishedSlotIndex = finishedSlotQuery.value("slot_index").toInt();
    } else {
        qCritical() << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━";
        qCritical() << "pan_FinishedProductLocation 中无空槽（drug_name 均非空），触发紧急暂停";
        qCritical() << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━";
        if (ui && ui->pushButton_Stop)
            QMetaObject::invokeMethod(ui->pushButton_Stop, "click", Qt::QueuedConnection);
        return;
    }

    // 校验槽位范围
    int finishedMaxSlot = finishedCols * finishedRows - 1;
    if (finishedSlotIndex < 0 || finishedSlotIndex > finishedMaxSlot) {
        qWarning() << "slot_index" << finishedSlotIndex << "超出范围 [0," << finishedMaxSlot << "]，使用 0";
        finishedSlotIndex = 0;
    }

    // 计算目标坐标
    SlotPositionConfig finishedConfig(finishedX, finishedY, finishedCols, finishedRows, finishedRightSpacing, finishedBottomSpacing);
    QPoint finishedTargetPos = calculateSlotPosition(finishedConfig, finishedSlotIndex);
    int finishedTargetX = finishedTargetPos.x();
    int finishedTargetY = finishedTargetPos.y();

    QString moveToTransferRightAreaXCommand = tcpCore->buildDeviceCommand("0A", "D", finishedTargetX, 8);
    tcpCore->sendMessageAsync(moveToTransferRightAreaXCommand.toUtf8(), true);
    QString waitTransferRightAreaXCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    tcpCore->sendMessageAsync(waitTransferRightAreaXCommand.toUtf8(), true, "0Ad01");

    QString moveToTransferRightAreaYCommand = tcpCore->buildDeviceCommand("09", "D", finishedTargetY, 8);
    tcpCore->sendMessageAsync(moveToTransferRightAreaYCommand.toUtf8(), true);
    QString waitTransferRightAreaYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    tcpCore->sendMessageAsync(waitTransferRightAreaYCommand.toUtf8(), true, "09d01");

    QString moveToTransferRightAreaZCommand = tcpCore->buildDeviceCommand("06", "D", finishedZ, 8);
    tcpCore->sendMessageAsync(moveToTransferRightAreaZCommand.toUtf8(), true);
    QString waitTransferRightAreaZCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    tcpCore->sendMessageAsync(waitTransferRightAreaZCommand.toUtf8(), true, "06d01");

    /**
     * 松夹爪
     *
     * 5号电机松夹爪
     */
    QString releaseGripperCommand = tcpCore->buildDeviceCommand("05", "06", "0105", 0, 4);
    tcpCore->sendMessageAsync(releaseGripperCommand.toUtf8(), false);
    QString waitGripperReleaseCommand = tcpCore->buildDeviceCommand("05", "03", "0202", 1, 4);
    tcpCore->sendMessageAsync(waitGripperReleaseCommand.toUtf8(), false, "0503020001");

    /**
     * 上移
     *
     * 6号电机上移
     */
    tcpCore->sendMessageAsync(raiseTransferZCommand.toUtf8(), true);
    tcpCore->sendMessageAsync(waitTransferZRaisedCommand.toUtf8(), true, "06d01");

}

// 计算槽位坐标
QPoint MainWindow::calculateSlotPosition(const SlotPositionConfig& config, int index)
{
    if (config.cols <= 0) {
        qWarning() << "列数必须大于0";
        return QPoint(static_cast<int>(config.sourceX), static_cast<int>(config.sourceY));
    }

    // 计算行号和列号（从0开始）
    int rowIndex = index / config.cols;
    int colIndex = index % config.cols;

    // 计算目标坐标
    // X方向：根据 xDirectionReverse 决定：false=往左(减法，默认), true=往右(加法)
    // Y方向：始终往下(加法)
    double targetX = config.xDirectionReverse 
                     ? (config.sourceX + colIndex * config.spacingX)  // 往右：加法
                     : (config.sourceX - colIndex * config.spacingX); // 往左：减法（默认）
    
    double targetY = config.sourceY + rowIndex * config.spacingY;     // 往下：加法（固定）

    // 抹除小数点后面的部分（直接截断，不四舍五入）
    int resultX = static_cast<int>(targetX);
    int resultY = static_cast<int>(targetY);

    return QPoint(resultX, resultY);
}

bool MainWindow::saveRecipeToDatabase(const RecipeQueueItem& recipe, bool insertBeforeFirstPending)
{
    if (!dbm) {
        qWarning() << "数据库对象未初始化，无法保存配方到数据库";
        return false;
    }

    // 1. 计算 executionOrder
    int executionOrder = 0;
    
    if (insertBeforeFirstPending) {
        // 插队模式：插入到第一个未执行配方之前
        // 查询第一个未执行配方的 executionOrder
        QString findFirstPendingSql = "SELECT MIN(executionOrder) as minOrder FROM recipeQueue WHERE processState = 0";
        QSqlQuery findQuery = dbm->query(findFirstPendingSql);
        
        if (findQuery.next() && !findQuery.value("minOrder").isNull()) {
            int targetOrder = findQuery.value("minOrder").toInt();
            // 将所有 executionOrder >= targetOrder 的配方都 +1
            QString shiftSql = QString("UPDATE recipeQueue SET executionOrder = executionOrder + 1 WHERE executionOrder >= %1").arg(targetOrder);
            dbm->query(shiftSql);
            executionOrder = targetOrder;
            qDebug() << "插队模式：新配方将插入到 executionOrder =" << executionOrder;
        } else {
            // 没有未执行的配方，追加到末尾
            QString maxOrderSql = "SELECT COALESCE(MAX(executionOrder), -1) + 1 as nextOrder FROM recipeQueue";
            QSqlQuery maxQuery = dbm->query(maxOrderSql);
            if (maxQuery.next()) {
                executionOrder = maxQuery.value("nextOrder").toInt();
            }
            qDebug() << "没有未执行配方，追加到末尾，executionOrder =" << executionOrder;
        }
    } else {
        // 默认模式：追加到末尾
        QString maxOrderSql = "SELECT COALESCE(MAX(executionOrder), -1) + 1 as nextOrder FROM recipeQueue";
        QSqlQuery maxQuery = dbm->query(maxOrderSql);
        if (maxQuery.next()) {
            executionOrder = maxQuery.value("nextOrder").toInt();
        }
    }

    // 2. 插入配方主表
    QString insertRecipeSql = QString(
        "INSERT INTO recipeQueue (recipeName, createTime, processState, executionOrder) "
        "VALUES ('%1', '%2', %3, %4)"
    ).arg(recipe.recipeName,
          recipe.createTime.toString("yyyy-MM-dd hh:mm:ss"),
          QString::number(recipe.processState),
          QString::number(executionOrder));
    
    QSqlQuery recipeQuery = dbm->query(insertRecipeSql);
    if (recipeQuery.lastError().isValid()) {
        qWarning() << "插入配方主表失败:" << recipeQuery.lastError().text();
        return false;
    }
    
    // 2. 获取刚插入的配方 ID
    int recipeId = recipeQuery.lastInsertId().toInt();
    qDebug() << "配方已保存到数据库，ID:" << recipeId;
    
    // 3. 插入消息队列
    QSqlDatabase db = QSqlDatabase::database("app_sqlite_conn");
    if (!db.isOpen()) {
        qWarning() << "数据库未打开，无法插入消息队列";
        return false;
    }
    
    int messageOrder = 0;
    for (const auto &message : std::as_const(recipe.messageQueue)) {
        // 使用参数绑定方式插入 BLOB 数据
        QSqlQuery messageQuery(db);
        messageQuery.prepare(
            "INSERT INTO recipeMessageQueue (recipeId, messageOrder, content, asciiOrHex, shouldWaitForResponse, expectedSignature) "
            "VALUES (?, ?, ?, ?, ?, ?)"
        );
        messageQuery.addBindValue(recipeId);
        messageQuery.addBindValue(messageOrder++);
        messageQuery.addBindValue(message.content);
        messageQuery.addBindValue(message.asciiOrHex ? 1 : 0);
        messageQuery.addBindValue(message.shouldWaitForResponse ? 1 : 0);
        // 确保 expectedSignature 不为 NULL，如果为空或未初始化则使用空字符串
        QString expectedSig = message.expectedSignature.isNull() || message.expectedSignature.isEmpty() 
                            ? QString("") 
                            : message.expectedSignature;
        messageQuery.addBindValue(expectedSig);
        
        if (!messageQuery.exec()) {
            qWarning() << "插入消息失败:" << messageQuery.lastError().text() << "messageOrder:" << (messageOrder - 1);
            return false;
        }
    }
    qDebug() << "配方消息队列已保存，共" << messageOrder << "条消息";
    return true;
}

void MainWindow::saveAndExecuteRecipe(const RecipeQueueItem& recipe, bool insertBeforeFirstPending)
{
    // 保存到数据库并执行
    if (!saveRecipeToDatabase(recipe, insertBeforeFirstPending)) {
        qWarning() << "保存配方到数据库失败";
        return;
    }

    if (!loadAndExecuteNextRecipeFromDatabase()) {
        qDebug() << "没有可执行的配方或当前有配方正在执行";
    }
}

void MainWindow::checkAndResetInterruptedRecipes()
{
    if (!dbm) {
        qWarning() << "数据库对象未初始化，无法检查中断的配方";
        return;
    }

    // ========== 1. 处理 recipeQueue 表中正在执行的配方 ==========
    QString selectSql = "SELECT id, recipeName, createTime FROM recipeQueue WHERE processState = 1";
    QSqlQuery selectQuery = dbm->query(selectSql);
    
    if (selectQuery.lastError().isValid()) {
        qWarning() << "查询正在执行的配方失败:" << selectQuery.lastError().text();
        return;
    }

    int recipeCount = 0;
    while (selectQuery.next()) {
        int recipeId = selectQuery.value("id").toInt();
        QString recipeName = selectQuery.value("recipeName").toString();
        QString createTime = selectQuery.value("createTime").toString();
        
        // 更新状态为 9（开机中断）
        QString updateSql = QString("UPDATE recipeQueue SET processState = 9 WHERE id = %1").arg(recipeId);
        QSqlQuery updateQuery = dbm->query(updateSql);
        
        if (updateQuery.lastError().isValid()) {
            qWarning() << "更新配方状态失败，ID:" << recipeId << "错误:" << updateQuery.lastError().text();
        } else {
            qDebug() << "发现开机中断的配方，已标记为状态9 - ID:" << recipeId 
                     << "名称:" << recipeName << "创建时间:" << createTime;
            recipeCount++;
        }
    }

    if (recipeCount > 0) {
        qDebug() << "开机检查完成，共发现" << recipeCount << "个中断的配方（已标记为状态9）";
    } else {
        qDebug() << "开机检查完成，没有发现中断的配方";
    }

    // ========== 2. 处理 shakeBedArea 表中占位但没用上的记录（isEmpty = 2）==========
    QString selectShakeBedSql = "SELECT selfLocation, startTime, endTime FROM shakeBedArea WHERE isEmpty = 2";
    QSqlQuery selectShakeBedQuery = dbm->query(selectShakeBedSql);
    
    if (selectShakeBedQuery.lastError().isValid()) {
        qWarning() << "查询shakeBedArea表失败:" << selectShakeBedQuery.lastError().text();
        return;
    }

    int shakeBedCount = 0;
    while (selectShakeBedQuery.next()) {
        int selfLocation = selectShakeBedQuery.value("selfLocation").toInt();
        QString startTime = selectShakeBedQuery.value("startTime").toString();
        QString endTime = selectShakeBedQuery.value("endTime").toString();
        
        // 恢复为未使用状态（isEmpty = 1），并清空开始时间和结束时间
        QString updateShakeBedSql = QString("UPDATE shakeBedArea SET isEmpty = 1 WHERE selfLocation = %1").arg(selfLocation);
        QSqlQuery updateShakeBedQuery = dbm->query(updateShakeBedSql);
        
        if (updateShakeBedQuery.lastError().isValid()) {
            qWarning() << "更新shakeBedArea状态失败，位置:" << selfLocation << "错误:" << updateShakeBedQuery.lastError().text();
        } else {
            qDebug() << "发现占位但没用上的摇床位置，已恢复为未使用状态 - 位置:" << selfLocation 
                     << "开始时间:" << startTime << "结束时间:" << endTime;
            shakeBedCount++;
        }
    }

    if (shakeBedCount > 0) {
        qDebug() << "开机检查完成，共释放" << shakeBedCount << "个占位但没用上的摇床位置（已恢复为未使用状态）";
    } else {
        qDebug() << "开机检查完成，没有发现占位但没用上的摇床位置";
    }
}

bool MainWindow::loadAndExecuteNextRecipeFromDatabase()
{
    if (!dbm) {
        qWarning() << "数据库对象未初始化，无法从数据库加载配方";
        return false;
    }

    if (!tcpCore) {
        qWarning() << "TCP核心对象未初始化，无法执行配方";
        return false;
    }

    // 0. 先检查是否有配方正在执行（processState = 1）
    QString checkProcessingSql = "SELECT COUNT(*) FROM recipeQueue WHERE processState = 1";
    QSqlQuery checkQuery = dbm->query(checkProcessingSql);
    if (checkQuery.next() && checkQuery.value(0).toInt() > 0) {
        qDebug() << "当前有配方正在执行，不加载新配方";
        return false;
    }

    // 1. 查询下一个未执行的配方（processState = 0，按执行顺序）
    QString selectRecipeSql = "SELECT id, recipeName, createTime, processState FROM recipeQueue WHERE processState = 0 ORDER BY executionOrder LIMIT 1";
    QSqlQuery recipeQuery = dbm->query(selectRecipeSql);
    
    if (recipeQuery.lastError().isValid()) {
        qWarning() << "查询配方失败:" << recipeQuery.lastError().text();
        return false;
    }

    if (!recipeQuery.next()) {
        qDebug() << "数据库中没有未执行的配方";
        return false;
    }

    // 2. 读取配方基本信息
    int recipeId = recipeQuery.value("id").toInt();
    QString recipeName = recipeQuery.value("recipeName").toString();
    QString createTimeStr = recipeQuery.value("createTime").toString();
    
    qDebug() << "从数据库加载配方: ID=" << recipeId << "名称=" << recipeName << "创建时间=" << createTimeStr;

    // 3. 重置流程状态显示（清除上一次的绿色效果）
    resetProcessStateDisplay();
    qDebug() << "已重置流程状态显示，准备执行新配方";

    // 4. 更新配方状态为"正在执行"
    QString updateStateSql = QString("UPDATE recipeQueue SET processState = %1 WHERE id = %2")
        .arg(RecipeProcessing).arg(recipeId);
    QSqlQuery updateQuery = dbm->query(updateStateSql);
    if (updateQuery.lastError().isValid()) {
        qWarning() << "更新配方状态失败:" << updateQuery.lastError().text();
    }

    // 5. 查询该配方的所有消息（按 messageOrder 排序）
    QString selectMessagesSql = QString(
        "SELECT messageOrder, content, asciiOrHex, shouldWaitForResponse, expectedSignature "
        "FROM recipeMessageQueue WHERE recipeId = %1 ORDER BY messageOrder"
    ).arg(recipeId);
    
    QSqlQuery messageQuery = dbm->query(selectMessagesSql);
    if (messageQuery.lastError().isValid()) {
        qWarning() << "查询配方消息失败:" << messageQuery.lastError().text();
        return false;
    }

    // 6. 清空旧队列，确保数据干净
    tcpCore->clearMessageQueue();

    // 7. 将消息添加到 tcpCore 执行
    int messageCount = 0;
    qDebug() << "════════════════════════════════════════";
    qDebug() << "开始批量添加消息到队列（同步执行）";
    qDebug() << "════════════════════════════════════════";
    while (messageQuery.next()) {
        QByteArray content = messageQuery.value("content").toByteArray();
        bool asciiOrHex = messageQuery.value("asciiOrHex").toInt() == 1;
        QString expectedSignature = messageQuery.value("expectedSignature").toString();
        
        // 调用 sendMessageAsync 将消息加入队列
        tcpCore->sendMessageAsync(content, asciiOrHex, expectedSignature);
         messageCount++;
    }
    qDebug() << "════════════════════════════════════════";
    qDebug() << "批量添加完成！配方" << recipeName << "共添加" << messageCount << "条消息";
    qDebug() << "注意：虽然 start(0) 被调用了" << messageCount << "次，";
    qDebug() << "      但 processMessageQueue() 还一次都没执行！";
    qDebug() << "      现在函数即将返回，Qt事件循环将接管...";
    qDebug() << "════════════════════════════════════════";
    return true;
}

bool MainWindow::incrementDatabaseField(const QString& tableName, const QString& fieldName, const QString& whereClause)
{
    if (!dbm) {
        qWarning() << "数据库对象未初始化";
        return false;
    }

    // 构建UPDATE SQL语句
    QString updateSql;
    if (whereClause.isEmpty()) {
        // 如果没有WHERE条件，更新所有记录（通常不推荐，但保留此功能）
        updateSql = QString("UPDATE %1 SET %2 = %2 + 1").arg(tableName, fieldName);
    } else {
        // 有WHERE条件，只更新匹配的记录
        updateSql = QString("UPDATE %1 SET %2 = %2 + 1 WHERE %3").arg(tableName, fieldName, whereClause);
    }

    QSqlQuery updateQuery = dbm->query(updateSql);

    if (updateQuery.lastError().isValid()) {
        qWarning() << "更新数据库字段失败:" << updateQuery.lastError().text() << "SQL:" << updateSql;
        return false;
    }

    // 检查是否有记录被更新
    int rowsAffected = updateQuery.numRowsAffected();
    if (rowsAffected > 0) {
        qDebug() << QString("成功更新表 %1 的字段 %2，影响 %3 条记录").arg(tableName, fieldName).arg(rowsAffected);
        return true;
    } else {
        qWarning() << QString("更新表 %1 的字段 %2 时没有记录被影响，可能WHERE条件不匹配").arg(tableName, fieldName);
        return false;
    }
}

// 将数据库表中指定字段的值自动减1
bool MainWindow::decrementDatabaseField(const QString& tableName, const QString& fieldName, const QString& whereClause)
{
    if (!dbm) {
        qWarning() << "数据库对象未初始化";
        return false;
    }

    // 构建UPDATE SQL语句
    QString updateSql;
    if (whereClause.isEmpty()) {
        // 如果没有WHERE条件，更新所有记录（通常不推荐，但保留此功能）
        updateSql = QString("UPDATE %1 SET %2 = %2 - 1").arg(tableName, fieldName);
    } else {
        // 有WHERE条件，只更新匹配的记录
        updateSql = QString("UPDATE %1 SET %2 = %2 - 1 WHERE %3").arg(tableName, fieldName, whereClause);
    }

    QSqlQuery updateQuery = dbm->query(updateSql);

    if (updateQuery.lastError().isValid()) {
        qWarning() << "更新数据库字段失败:" << updateQuery.lastError().text() << "SQL:" << updateSql;
        return false;
    }

    // 检查是否有记录被更新
    int rowsAffected = updateQuery.numRowsAffected();
    if (rowsAffected > 0) {
        qDebug() << QString("成功更新表 %1 的字段 %2，影响 %3 条记录").arg(tableName, fieldName).arg(rowsAffected);
        return true;
    } else {
        qWarning() << QString("更新表 %1 的字段 %2 时没有记录被影响，可能WHERE条件不匹配").arg(tableName, fieldName);
        return false;
    }
}







// 控制摇床开关
void MainWindow::controlShakeBed(bool isOn, bool sendImmediately)
{
    if (!tcpCore) {
        qWarning() << "TCP核心对象未初始化，无法控制摇床";
        return;
    }

    QString command;
    QString expectedSignature;
    QString logMessage;

    if (isOn) {
        // 启动摇床
        command = tcpCore->buildMessageWithCrc(QStringLiteral(">0Cxi3000000012c0000012c"));
        expectedSignature = "0Cxi300";
        logMessage = "启动摇床";
    } else {
        // 关闭摇床
        command = tcpCore->buildMessageWithCrc(QStringLiteral(">0Cxi30100000000"));
        expectedSignature = "0Cxi301";
        logMessage = "关闭摇床";
    }

    // qDebug() << logMessage << command;

    if (sendImmediately) {
        // 立即发送（同步）
        tcpCore->sendMessage(command.toUtf8(), true);
    } else {
        // 异步发送
        tcpCore->sendMessageAsync(command.toUtf8(), true, expectedSignature);
    }
}
void MainWindow::controlShakeBed(bool isOn, QQueue<MessageQueueItem>& messageQueue, bool sendImmediately)
{
    if (!tcpCore) {
        qWarning() << "TCP核心对象未初始化，无法控制摇床";
        return;
    }

    QString command;
    QString expectedSignature;
    QString logMessage;

    if (isOn) {
        // 启动摇床
        command = tcpCore->buildMessageWithCrc(QStringLiteral(">0Cxi3000000012c0000012c"));
        expectedSignature = "0Cxi300";
        logMessage = "启动摇床";
    } else {
        // 关闭摇床
        command = tcpCore->buildMessageWithCrc(QStringLiteral(">0Cxi30100000000"));
        expectedSignature = "0Cxi301";
        logMessage = "关闭摇床";
    }

    qDebug() << logMessage << command;

    if (sendImmediately) {
        // 立即发送（同步）
        tcpCore->sendMessage(command.toUtf8(), true);
    } else {
        // 异步发送
        //tcpCore->sendMessageAsync(command.toUtf8(), true, expectedSignature);
        messageQueue.enqueue(MessageQueueItem(command.toUtf8(), true, expectedSignature));
    }
}





// 设置5号电机速度
void MainWindow::setMotor5Speed(int speed, QQueue<MessageQueueItem>& messageQueue)
{
    // 验证速度范围（10-100）
    if (speed < 10 || speed > 100) {
        qWarning() << "5号电机速度超出范围！速度:" << speed << "，有效范围: 10-100";
        return;
    }

    // 构建5号电机速度设置命令
    QString speed5Command = tcpCore->buildDeviceCommand("05", "06", "0107", speed, 4);

    // 构建期望签名：05060107 + 速度的4位16进制字符串
    QString expectedSignature = "05060107" + QString::number(speed, 16).toUpper().rightJustified(4, '0');

    // 发送命令
    //tcpCore->sendMessageAsync(speed5Command.toUtf8(), false, expectedSignature);
    messageQueue.enqueue(MessageQueueItem(speed5Command.toUtf8(), false, expectedSignature));

    qDebug() << "设置5号电机速度:" << speed;
}

// 设置6号电机Z轴速度
void MainWindow::setMotor6ZSpeed(int speed, QQueue<MessageQueueItem>& messageQueue)
{
    // 验证速度范围（建议范围：1-10000 rpm，但不强制限制）
    if (speed <= 0) {
        qWarning() << "6号电机Z轴速度无效！速度:" << speed << "，必须大于0";
        return;
    }

    // 计算速度值：(速度 * 6400) / 60
    int speedValue = (speed * 6400) / 60;

    // 转换为8位16进制字符串并添加"0A0A"后缀
    QString speedValueString = QString::number(speedValue, 16).toUpper().rightJustified(8, '0') + "0A0A";

    // 构建命令：">06B00000000" + 速度值字符串
    QString speed6zCommand = tcpCore->buildMessageWithCrc(">06B00000000" + speedValueString);

    // 发送命令（ASCII模式，期望签名"06B"）
    //tcpCore->sendMessageAsync(speed6zCommand.toUtf8(), true, "06B");
    messageQueue.enqueue(MessageQueueItem(speed6zCommand.toUtf8(), true, "06B"));

    qDebug() << "设置6号电机Z轴速度:" << speed << "rpm";
}

// 5号电机旋转圈数
// 设置5号设备拧紧力度
void MainWindow::setMotor5TighteningForce(int force, QQueue<MessageQueueItem>& messageQueue)
{
    // TODO: 在此处编写设置5号设备拧紧力度的具体实现
    // 参数 force 范围：10-100      01 06 01 060032E9E2
    // 发送： 05 03 0106 0001 65F7
    // 返回： 05 0302 0032 3991
    Q_UNUSED(force);

    QString command = tcpCore->buildDeviceCommand("05", "06", "0106", force, 4);
    messageQueue.enqueue(MessageQueueItem(command.toUtf8(), false, command));

    QString waitCommand = tcpCore->buildDeviceCommand("05", "03", "0106", 1, 4);
    QString responseData = QString("050302") + QString::number(force, 16).toUpper().rightJustified(4, '0');
    messageQueue.enqueue(MessageQueueItem(waitCommand.toUtf8(), false, responseData));

}

void MainWindow::rotateMotor5ByCircles(double circles, QQueue<MessageQueueItem>& messageQueue)
{
    // 将圈数转换为角度值（圈数 * 360度）
    int angleValue = static_cast<int>(circles * 360);

    // 构建5号电机旋转命令（寄存器0108用于旋转）
    QString rotateCommand = tcpCore->buildDeviceCommand("05", "06", "0108", angleValue, 4);

    // 构建完整的期望响应值（ModBus写命令会回显完整的请求）
    QString expectedResponse = QString("05060108%1").arg(angleValue, 4, 16, QChar('0')).toUpper();

    // 发送命令（Hex模式）
    messageQueue.enqueue(MessageQueueItem(rotateCommand.toUtf8(), false, expectedResponse));

    qDebug() << "5号电机旋转:" << circles << "圈（角度值:" << angleValue << "度）期望响应:" << expectedResponse;
}

// 获取本地无线网口的IP地址
QString MainWindow::getLocalWirelessIP()
{
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    
    // 无线网卡的关键词（Windows和Linux通用）
    QStringList wirelessKeywords;
    wirelessKeywords << "Wi-Fi" << "WiFi" << "WLAN" << "Wireless" 
                    << "wlan" << "wifi" << "wireless" << "802.11";
    
    for (const QNetworkInterface &interface : std::as_const(interfaces)) {
        // 检查接口是否启用且不是回环接口
        if (!interface.flags().testFlag(QNetworkInterface::IsUp) ||
            interface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }
        
        // 检查接口名称是否包含无线网卡关键词
        QString interfaceName = interface.name();
        bool isWireless = false;
        for (const QString &keyword : std::as_const(wirelessKeywords)) {
            if (interfaceName.contains(keyword, Qt::CaseInsensitive)) {
                isWireless = true;
                break;
            }
        }
        
        // 如果找到无线网卡，获取其IPv4地址
        if (isWireless) {
            QList<QNetworkAddressEntry> entries = interface.addressEntries();
            for (const QNetworkAddressEntry &entry : std::as_const(entries)) {
                QHostAddress address = entry.ip();
                // 只返回IPv4地址，排除IPv6
                if (address.protocol() == QAbstractSocket::IPv4Protocol) {
                    QString ip = address.toString();
                    // qDebug() << "找到无线网口:" << interfaceName << "，IP地址:" << ip;
                    return ip;
                }
            }
        }
    }
    
    // 如果没找到无线网卡，尝试返回第一个非回环的IPv4地址（作为备选）
    for (const QNetworkInterface &interface : std::as_const(interfaces)) {
        if (interface.flags().testFlag(QNetworkInterface::IsUp) &&
            !interface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            QList<QNetworkAddressEntry> entries = interface.addressEntries();
            for (const QNetworkAddressEntry &entry : std::as_const(entries)) {
                QHostAddress address = entry.ip();
                if (address.protocol() == QAbstractSocket::IPv4Protocol) {
                    QString ip = address.toString();
                    qDebug() << "未找到无线网口，使用备选网口:" << interface.name() << "，IP地址:" << ip;
                    return ip;
                }
            }
        }
    }
    
    qWarning() << "未找到可用的网络接口IP地址";
    return QString();  // 返回空字符串表示未找到
}

// 获取本地有线网口的IP地址
QString MainWindow::getLocalWiredIP()
{
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    
    // 有线网卡的关键词（Windows和Linux通用）
    QStringList wiredKeywords;
    wiredKeywords << "Ethernet" << "ethernet" << "eth" << "以太网" 
                  << "Local Area Connection" << "本地连接";
    
    for (const QNetworkInterface &interface : std::as_const(interfaces)) {
        // 检查接口是否启用且不是回环接口
        if (!interface.flags().testFlag(QNetworkInterface::IsUp) ||
            interface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }
        
        // 检查接口名称是否包含有线网卡关键词
        QString interfaceName = interface.name();
        bool isWired = false;
        for (const QString &keyword : std::as_const(wiredKeywords)) {
            if (interfaceName.contains(keyword, Qt::CaseInsensitive)) {
                isWired = true;
                break;
            }
        }
        
        // 如果找到有线网卡，获取其IPv4地址
        if (isWired) {
            QList<QNetworkAddressEntry> entries = interface.addressEntries();
            for (const QNetworkAddressEntry &entry : std::as_const(entries)) {
                QHostAddress address = entry.ip();
                // 只返回IPv4地址，排除IPv6
                if (address.protocol() == QAbstractSocket::IPv4Protocol) {
                    QString ip = address.toString();
                    // qDebug() << "找到有线网口:" << interfaceName << "，IP地址:" << ip;
                    return ip;
                }
            }
        }
    }
    
    // 如果没找到有线网卡，尝试返回第一个非无线的IPv4地址（作为备选）
    QStringList wirelessKeywords;
    wirelessKeywords << "Wi-Fi" << "WiFi" << "WLAN" << "Wireless" 
                    << "wlan" << "wifi" << "wireless" << "802.11";
    
    for (const QNetworkInterface &interface : std::as_const(interfaces)) {
        if (interface.flags().testFlag(QNetworkInterface::IsUp) &&
            !interface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            
            QString interfaceName = interface.name();
            bool isWireless = false;
            for (const QString &keyword : std::as_const(wirelessKeywords)) {
                if (interfaceName.contains(keyword, Qt::CaseInsensitive)) {
                    isWireless = true;
                    break;
                }
            }
            
            // 跳过无线网卡
            if (isWireless) continue;
            
            QList<QNetworkAddressEntry> entries = interface.addressEntries();
            for (const QNetworkAddressEntry &entry : std::as_const(entries)) {
                QHostAddress address = entry.ip();
                if (address.protocol() == QAbstractSocket::IPv4Protocol) {
                    QString ip = address.toString();
                    qDebug() << "未找到明确的有线网口，使用备选网口:" << interface.name() << "，IP地址:" << ip;
                    return ip;
                }
            }
        }
    }
    
    qWarning() << "未找到可用的有线网络接口IP地址";
    return QString();  // 返回空字符串表示未找到
}



// 测试配方发送功能（接收JSON对象和JSON字符串）
void MainWindow::testRecipeSendWithString(const QJsonObject& recipePacket, const QString& jsonString)
{
    qDebug() << "========== 收到配方 ==========";
    qDebug() << "JSON对象:" << recipePacket;
    qDebug().noquote() << "JSON字符串:" << jsonString;
    qDebug() << "字符串长度:" << jsonString.length();
    qDebug() << "================================";


}






















