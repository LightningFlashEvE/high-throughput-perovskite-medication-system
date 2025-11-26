#include "mainwindow.h"
#include "qsqlerror.h"
#include "tcpclientcore.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>
#include <QThread>
#include <QSlider>
#include <QtSql/QSqlQuery>
#include "qsqldatabase.h"
#include "box.h"
#include "reagentbottle.h"
#include "slot.h"
#include "ui_mainwindow.h"


void MainWindow::initializeSystemComponents()
{
    // ========== 初始化数据库表==========
    {
        dbm = new AppSqlDatabase("liquid.db", this);
        // 默认表结构与初始数据由 AppSqlDatabase 构造函数自动完成
    }



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

    // connetct tcpBalanceCore发出weightReached信号时，tcpCore发送停止命令
    connect(tcpBalanceCore, &TcpClientCore::weightReached, this, [=](double weight) {
        qDebug() << "connect获取到重量值:" << weight << "g";
        tcpCore->writeBalanceTareCommand(">01K0EE65", TcpClientCore::AsciiMode);
        tcpBalanceCore->clearExpectedWeight();
        tcpBalanceCore->disconnectReceiveForBalance();

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
    


    // 连接 tcpCore 的 balanceTareRequested 信号，让 tcpBalanceCore 【【执行去皮命令】】
    connect(tcpCore, &TcpClientCore::balanceTareRequested, this, [=]() {
        qDebug() << "发送去皮命令（十六进制：540D0A = T\\r\\n）";
        tcpBalanceCore->writeBalanceTareCommand("540D0A", TcpClientCore::HexMode);
        tcpBalanceCore->setExpectedWeight(tcpCore->getExpectedWeight());
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
    connect(tcpCore, &TcpClientCore::recordShakeBedTimeRequested, this, [=]() {
        qDebug() << "收到AArecordShakeBedTime命令，执行记录摇床时间";
        // 查询shakeBedArea表中isEmpty为1的记录，获取selfLocation
        QString shakeBedAreaSql = "SELECT selfLocation FROM shakeBedArea WHERE isEmpty = 1 LIMIT 1";
        QSqlQuery shakeBedAreaQuery = dbm->query(shakeBedAreaSql);
        if (shakeBedAreaQuery.next()) {
            int selfLocation = shakeBedAreaQuery.value("selfLocation").toInt();
            // 记录摇床时间（默认300秒，即5分钟）
            recordShakeBedTime(selfLocation, 30);
        } else {
            qWarning() << "未找到 isEmpty = 1 的记录，无法记录摇床时间";
        }
    });
    
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
    
    // 连接 tcpCore 的 emptyBottleAreaCurrentIndexPlusOneRequested 信号，执行空瓶区currentIndex加1
    connect(tcpCore, &TcpClientCore::emptyBottleAreaCurrentIndexPlusOneRequested, this, [=]() {
        qDebug() << "收到AAemptyBottleAreaCurrentIndexPlusOne命令，执行空瓶区currentIndex加1";
        incrementDatabaseField("other", "currentIndex", "name = 'emptyBottleArea'");
    });
    
    // 连接 tcpCore 的 tipsHeadAreaCurrentIndexPlusOneRequested 信号，执行tips头区currentIndex加1
    connect(tcpCore, &TcpClientCore::tipsHeadAreaCurrentIndexPlusOneRequested, this, [=]() {
        qDebug() << "收到AAtipsHeadAreaCurrentIndexPlusOne命令，执行tips头区currentIndex加1";
        incrementDatabaseField("other", "currentIndex", "name = 'tipsHeadArea'");
    });



    // ========== 按钮连接 ==========
    // 按钮1：连接并发送测试命令
    connect(ui->pushButton, &QPushButton::clicked, this, [=] {

        // 连接到TCP服务器
        tcpCore->connectToTcp("192.168.5.22", "192.168.5.201", 4196, true);
        tcpBalanceCore->connectToTcp("192.168.5.22", "192.168.5.202", 4196, true);
        
        initializeAllDevices();

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

    });

    // 按钮3：获取坐标
    connect(ui->pushButton_3, &QPushButton::clicked, this, [=] {

        qDebug() << "获取坐标";
        QString dataMsg = tcpCore->buildDeviceCommand("0A", "E", 0, 0);
        tcpCore->sendMessageAsync(dataMsg.toUtf8(), true);  // ✅ 改为 ASCII模式（true）

        // 使用 TcpClientCore 的函数构建设备命令（0A号设备，功能码G，无命令数据）
        QString dataMsg2 = tcpCore->buildDeviceCommand("09", "E", 0, 0);
        tcpCore->sendMessageAsync(dataMsg2.toUtf8(), true);

        // 使用 TcpClientCore 的函数构建设备命令（0A号设备，功能码G，无命令数据）
        QString dataMsg3 = tcpCore->buildDeviceCommand("06", "E", 0, 0);
        tcpCore->sendMessageAsync(dataMsg3.toUtf8(), true);

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


            initializeAllDevices();

        });

    });

    // 按钮5：移动
    connect(ui->pushButton_5, &QPushButton::clicked, this, [=] {
    /**
     * =====================
     * 获取A盘1号位置的h2o bool getLiquid(const QString& name, double ml);
     * =====================
     *
     * 1.拿空瓶到天平：
     * 移动到空盘坐标，下降，夹，上升。
     * 移动到夹持器坐标，下降，夹持器夹住，旋盖，上升。
     * 移动到放盖子坐标，下降，释放盖子，上升。
     *
     * 2.打开液体瓶盖：
     * 移动到A盘1号坐标，下降，夹（确认夹好后），上升。
     * 移动到夹持器坐标，下降，夹持器夹住，旋盖（盖子流上面），上升。
     *
     * 3.取液到天平：
     * 移液器到tips的1号坐标，下降，拿，上升。
     * 移动到夹持器坐标，下降（自动判断深度），取液，上升。
     * 移动到天平坐标，下降，释放液体，上升。
     * 移动到废品区，下降，退头，上升。
     *
     * 4.用完液体瓶归位：
     * 移动到夹持器，下降，旋盖，上升。
     * 移动回A盘1号坐标，下降，释放盖子，上升。
     */

    // 打开空瓶
    //takeEmptyBottle("Box_Transfer_Area_Right");

    // 取液体，参数为液体名称，液体量
    //getLiquid("DMF", 0.01);

    // 取固体
    // getSolid("Na2CO3", 2);

    // // 拧紧瓶子放置去摇床
    //tightenBottle();


    // 摇床启动
    //shakeBed(10);


    });
}















































// 测试配方发送功能（接收JSON对象）
void MainWindow::testRecipeSend(const QJsonObject& recipePacket)
{
    // 打印核心三部分
    qDebug() << "========== 收到配方（三部分） ==========";
    const QString equation = recipePacket.value("化学方程式").toString();
    qDebug() << "化学方程式:" << equation;

    // 解决tips头使用情况的问题。
    int tipsNum = 0;

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


    // TODO: 在此处开始排打顺序执行（按溶质/溶剂分别处理）
    // // 打开空瓶
    takeEmptyBottle("Box_Transfer_Area_Right");

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
        getLiquid(name, volume, tipsNum);
    }

    // 取固体：遍历溶质，传入名称与质量（g）
    for (const auto &v : std::as_const(solutes)) {
        const QJsonObject o = v.toObject();
        const QString name = o.value("名称").toString();
        const double mass = o.value("用量").toDouble();
        if (name.isEmpty() || qFuzzyIsNull(mass)) {
            qWarning() << "溶质参数不完整，跳过：" << o;
            continue;
        }
        qDebug() << "准备取固体:" << name << "目标质量(g):" << mass;
        getSolid(name, mass);
    }

    // // 拧紧瓶子放置去摇床
    tightenBottle();



}



























// 测试配方发送功能（接收JSON对象和JSON字符串）
void MainWindow::testRecipeSendWithString(const QJsonObject& recipePacket, const QString& jsonString)
{
    qDebug() << "========== 收到配方 ==========";
    qDebug() << "JSON对象:" << recipePacket;
    qDebug().noquote() << "JSON字符串:" << jsonString;
    qDebug() << "字符串长度:" << jsonString.length();
    qDebug() << "================================";
    
    // 这里可以直接使用 jsonString 进行后续操作
    // 例如：发送到设备、保存到文件、网络传输等
}

void MainWindow::initializeAllDevices()
{
    QString stopCommand = tcpCore->buildMessageWithCrc(QStringLiteral(">0Cxi30100000000"));
    tcpCore->writeBalanceTareCommand(stopCommand, TcpClientCore::StringMode);

    // 电机速度恢复 已知的有5号电机的旋转和6号电机的Z轴
    setMotor5Speed(100);
    setMotor6ZSpeed(1000);

    qDebug() << "消磁开始";
    QString closeElectromagnetCommand = tcpCore->buildDeviceCommand("0D", "05", "00000000", 0, 0);
    tcpCore->sendMessageAsync(closeElectromagnetCommand.toUtf8(), false);

    qDebug() << "\n\n固体电机初始化"; // 1号电机(跟A电机逻辑相似)
    QString solidMotorInitializeCommand = tcpCore->buildDeviceCommand("01", "f", 0, 0); // 归零
    tcpCore->sendMessageAsync(solidMotorInitializeCommand.toUtf8(), true);
    QString waitSolidMotorInitializeCommand = tcpCore->buildDeviceCommand("01", "g", 0, 0);
    tcpCore->sendMessageAsync(waitSolidMotorInitializeCommand.toUtf8(), true, "01g01");

    qDebug() << "\n\nz固体电机初始化"; // 2号电机
    QString zMotorInitializeCommand = tcpCore->buildDeviceCommand("02", "G", 0, 0); // 归零
    tcpCore->sendMessageAsync(zMotorInitializeCommand.toUtf8(), true);
    QString waitZMotorInitializeCommand = tcpCore->buildDeviceCommand("02", "d", 0, 0);
    tcpCore->sendMessageAsync(waitZMotorInitializeCommand.toUtf8(), true, "02d01");

    qDebug() << "\n\ny固体电机初始化"; // 3号电机
    QString yMotorInitializeCommand = tcpCore->buildDeviceCommand("03", "G", 0, 0); // 归零
    tcpCore->sendMessageAsync(yMotorInitializeCommand.toUtf8(), true);
    QString waitYMotorInitializeCommand = tcpCore->buildDeviceCommand("03", "d", 0, 0);
    tcpCore->sendMessageAsync(waitYMotorInitializeCommand.toUtf8(), true, "03d01");
    
    qDebug() << "\n\nx固体电机初始化"; // 4号电机
    QString xMotorInitializeCommand = tcpCore->buildDeviceCommand("04", "G", 0, 0); // 归零
    tcpCore->sendMessageAsync(xMotorInitializeCommand.toUtf8(), true);
    QString waitXMotorInitializeCommand = tcpCore->buildDeviceCommand("04", "d", 0, 0);
    tcpCore->sendMessageAsync(waitXMotorInitializeCommand.toUtf8(), true, "04d01");

    qDebug() << "\n\n泵初始化"; // 7号电机
    QString initializePumpCommand = tcpCore->buildDeviceCommand("07", "G", 0, 0);
    tcpCore->sendMessageAsync(initializePumpCommand.toUtf8(), true);
    QString waitPumpInitializedCommand = tcpCore->buildDeviceCommand("07", "d", 0, 0);
    tcpCore->sendMessageAsync(waitPumpInitializedCommand.toUtf8(), true);

    qDebug() << "\n\nz泵初始化"; // 8号电机
    QString initializeZPumpCommand = tcpCore->buildDeviceCommand("08", "G", 0, 0);
    tcpCore->sendMessageAsync(initializeZPumpCommand.toUtf8(), true);
    QString waitZPumpInitializedCommand = tcpCore->buildDeviceCommand("08", "d", 0, 0);
    tcpCore->sendMessageAsync(waitZPumpInitializedCommand.toUtf8(), true);
    // 08移动到4的位置
    QString moveToZ4Command = tcpCore->buildDeviceCommand("08", "D", 100, 8); // 00000004
    tcpCore->sendMessageAsync(moveToZ4Command.toUtf8(), true);
    QString waitZ4Command = tcpCore->buildDeviceCommand("08", "d", 0, 0);
    tcpCore->sendMessageAsync(waitZ4Command.toUtf8(), true);
    
    qDebug() << "\n\n移动电爪初始化"; // 5号电机
    QString initializeGripperCommand = tcpCore->buildDeviceCommand("05", "06", "0100", 1, 4);
    tcpCore->sendMessageAsync(initializeGripperCommand.toUtf8(), false);
    QString waitGripperInitializedCommand = tcpCore->buildDeviceCommand("05", "03", "0200", 1, 4);
    tcpCore->sendMessageAsync(waitGripperInitializedCommand.toUtf8(), false);
    QString configureGripperModeCommand = tcpCore->buildDeviceCommand("05", "06", "0101", 1, 4);
    tcpCore->sendMessageAsync(configureGripperModeCommand.toUtf8(), false);
    QString waitGripperModeConfiguredCommand = tcpCore->buildDeviceCommand("05", "03", "0201", 1, 4);
    tcpCore->sendMessageAsync(waitGripperModeConfiguredCommand.toUtf8(), false);

    qDebug() << "\n\nz移动电爪初始化"; // 6号电机
    QString initializeZAxisCommand = tcpCore->buildDeviceCommand("06", "G", 0, 0);
    tcpCore->sendMessageAsync(initializeZAxisCommand.toUtf8(), true);
    QString waitZAxisInitializedCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    tcpCore->sendMessageAsync(waitZAxisInitializedCommand.toUtf8(), true);

    // 摇床摇3s后停止，作为已经初始化。
    initializeShakeBed();

    qDebug() << "\n\n初始化X"; // 10号电机
    QString initializeXAxisCommand = tcpCore->buildDeviceCommand("0A", "G", 0, 0);
    tcpCore->sendMessageAsync(initializeXAxisCommand.toUtf8(), true);
    QString waitXAxisInitializedCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    tcpCore->sendMessageAsync(waitXAxisInitializedCommand.toUtf8(), true);

    qDebug() << "\n\n初始化Y"; // 9号电机
    QString initializeYAxisCommand = tcpCore->buildDeviceCommand("09", "G", 0, 0);
    tcpCore->sendMessageAsync(initializeYAxisCommand.toUtf8(), true);
    QString waitYAxisInitializedCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    tcpCore->sendMessageAsync(waitYAxisInitializedCommand.toUtf8(), true);

    qDebug() << "\n\n夹持区初始化"; // 11号电机
    QString initializeGripAreaCommand = tcpCore->buildDeviceCommand("0B", "06", "0100", 1, 4);
    tcpCore->sendMessageAsync(initializeGripAreaCommand.toUtf8(), false);
    QString waitGripAreaInitializedCommand = tcpCore->buildDeviceCommand("0B", "03", "0200", 1, 4);
    tcpCore->sendMessageAsync(waitGripAreaInitializedCommand.toUtf8(), false);
}


// 用于tcpBalanceCore，发送T\r\n（十六进制：54 0D 0A），这是天平去皮命令。单纯发送就行->write(dataToSend); 
void MainWindow::on_pushButton_6_clicked()
{
    // 使用十六进制模式发送：54 0D 0A (T\r\n)
    // tcpBalanceCore->writeBalanceTareCommand("540D0A", TcpClientCore::HexMode);
    // tcpCore->writeBalanceTareCommand(">01K0EE65", TcpClientCore::AsciiMode);
    tcpBalanceCore->disconnectReceiveForBalance();
}



// 摇床初始化（摇3秒后停止）
void MainWindow::initializeShakeBed()
{
    // 摇床是C，12号机器
    // 初始化：启动摇床，摇3秒后停止

    // 1. 初始化摇匀设备
    QString initCommand = tcpCore->buildMessageWithCrc(QStringLiteral(">0CG"));
    tcpCore->sendMessageAsync(initCommand.toUtf8(), true, "0CG");

    // 2. 查询初始化状态
    QString queryInitCommand = tcpCore->buildMessageWithCrc(QStringLiteral(">0cg"));
    tcpCore->sendMessageAsync(queryInitCommand.toUtf8(), true, "0Cg01");

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
    tcpCore->sendMessageAsync(startCommand.toUtf8(), true, "0Cxi300");
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
void MainWindow::resetXYZMotorsToZero()
{
    // 复位时停止天平打印
    if (tcpBalanceCore) {
        tcpBalanceCore->disconnectReceiveForBalance();
    }
    tcpCore->sendMessageAsync("AA0", true); // 关闭天平打印

    // 06号电机恢复到零点
    QString zeroMotorCommand = tcpCore->buildDeviceCommand("06", "D", 0, 8);
    tcpCore->sendMessageAsync(zeroMotorCommand.toUtf8(), true);
    QString waitZeroMotorCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    tcpCore->sendMessageAsync(waitZeroMotorCommand.toUtf8(), true, "06d01");

    // 08号电机恢复到零点
    zeroMotorCommand = tcpCore->buildDeviceCommand("08", "D", 3, 8);
    tcpCore->sendMessageAsync(zeroMotorCommand.toUtf8(), true);
    waitZeroMotorCommand = tcpCore->buildDeviceCommand("08", "d", 0, 0);
    tcpCore->sendMessageAsync(waitZeroMotorCommand.toUtf8(), true, "08d01");

    // 09号电机恢复到零点
    zeroMotorCommand = tcpCore->buildDeviceCommand("09", "D", 0, 8);
    tcpCore->sendMessageAsync(zeroMotorCommand.toUtf8(), true);
    waitZeroMotorCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    tcpCore->sendMessageAsync(waitZeroMotorCommand.toUtf8(), true, "09d01");

    // 0A号电机恢复到零点
    zeroMotorCommand = tcpCore->buildDeviceCommand("0A", "D", 0, 8);
    tcpCore->sendMessageAsync(zeroMotorCommand.toUtf8(), true);
    waitZeroMotorCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    tcpCore->sendMessageAsync(waitZeroMotorCommand.toUtf8(), true, "0Ad01");
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
    QString shakeBedAreaSql = "SELECT originX, originY, gripperZ, rightSpacing, bottomSpacing, cols, rows, currentIndex FROM other WHERE name = 'shakeBedArea'";
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
     * 数据库获取 transferRightArea 的xyz
     * 0A, 09, 06电机移动到transferRightArea的xyz
     *
     */
    QString transferRightAreaSql = "SELECT originX, originY, gripperZ FROM other WHERE name = 'transferRightArea'";
    QSqlQuery transferRightAreaQuery = dbm->query(transferRightAreaSql);
    int transferRightAreaX=0, transferRightAreaY=0, transferRightAreaZ=0, transferRightAreaRightSpacing=0, transferRightAreaBottomSpacing=0, transferRightAreaCols=0, transferRightAreaRows=0, transferRightAreaSlotIndex=0;
    if (transferRightAreaQuery.next()) {
        transferRightAreaX = transferRightAreaQuery.value("originX").toInt();
        transferRightAreaY = transferRightAreaQuery.value("originY").toInt();
        transferRightAreaZ = transferRightAreaQuery.value("gripperZ").toInt();
        transferRightAreaRightSpacing = transferRightAreaQuery.value("rightSpacing").toDouble();
        transferRightAreaBottomSpacing = transferRightAreaQuery.value("bottomSpacing").toDouble();
        transferRightAreaCols = transferRightAreaQuery.value("cols").toInt();
        transferRightAreaRows = transferRightAreaQuery.value("rows").toInt();
        transferRightAreaSlotIndex = transferRightAreaQuery.value("currentIndex").toInt();
    }
    // 计算xy
    SlotPositionConfig transferRightAreaConfig(transferRightAreaX, transferRightAreaY, transferRightAreaCols, transferRightAreaRows, transferRightAreaRightSpacing, transferRightAreaBottomSpacing);
    QPoint transferRightAreaTargetPos = calculateSlotPosition(transferRightAreaConfig, transferRightAreaSlotIndex);
    int transferRightAreaTargetX = transferRightAreaTargetPos.x();
    int transferRightAreaTargetY = transferRightAreaTargetPos.y();

    QString moveToTransferRightAreaXCommand = tcpCore->buildDeviceCommand("0A", "D", transferRightAreaTargetX, 8);
    tcpCore->sendMessageAsync(moveToTransferRightAreaXCommand.toUtf8(), true);
    QString waitTransferRightAreaXCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    tcpCore->sendMessageAsync(waitTransferRightAreaXCommand.toUtf8(), true, "0Ad01");

    QString moveToTransferRightAreaYCommand = tcpCore->buildDeviceCommand("09", "D", transferRightAreaTargetY, 8);
    tcpCore->sendMessageAsync(moveToTransferRightAreaYCommand.toUtf8(), true);
    QString waitTransferRightAreaYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    tcpCore->sendMessageAsync(waitTransferRightAreaYCommand.toUtf8(), true, "09d01");

    QString moveToTransferRightAreaZCommand = tcpCore->buildDeviceCommand("06", "D", transferRightAreaZ, 8);
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

    // 去other表把currentIndex值+1，并更新到数据库
    incrementDatabaseField("other", "currentIndex", "name = 'transferRightArea'");

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
    // X = 原点X - 列号 * 横向间距（往左移动，所以是减法）
    // Y = 原点Y + 行号 * 纵向间距（往下移动，所以是加法）
    double targetX = config.sourceX - colIndex * config.spacingX;
    double targetY = config.sourceY + rowIndex * config.spacingY;

    // 抹除小数点后面的部分（直接截断，不四舍五入）
    int resultX = static_cast<int>(targetX);
    int resultY = static_cast<int>(targetY);

    return QPoint(resultX, resultY);
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

    qDebug() << logMessage << command;

    if (sendImmediately) {
        // 立即发送（同步）
        tcpCore->sendMessage(command.toUtf8(), true);
    } else {
        // 异步发送
        tcpCore->sendMessageAsync(command.toUtf8(), true, expectedSignature);
    }
}






// 设置5号电机速度
void MainWindow::setMotor5Speed(int speed)
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
    tcpCore->sendMessageAsync(speed5Command.toUtf8(), false, expectedSignature);

    qDebug() << "设置5号电机速度:" << speed;
}

// 设置6号电机Z轴速度
void MainWindow::setMotor6ZSpeed(int speed)
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
    tcpCore->sendMessageAsync(speed6zCommand.toUtf8(), true, "06B");

    qDebug() << "设置6号电机Z轴速度:" << speed << "rpm";
}

// 5号电机旋转圈数
// 设置5号设备拧紧力度
void MainWindow::setMotor5TighteningForce(int force)
{
    // TODO: 在此处编写设置5号设备拧紧力度的具体实现
    // 参数 force 范围：10-100      01 06 01 060032E9E2
    // 发送： 05 03 0106 0001 65F7
    // 返回： 05 0302 0032 3991
    Q_UNUSED(force);

    QString command = tcpCore->buildDeviceCommand("05", "06", "0106", force, 4);
    tcpCore->sendMessageAsync(command.toUtf8(), false, command);

    QString waitCommand = tcpCore->buildDeviceCommand("05", "03", "0106", 1, 4);
    QString responseData = QString("050302") + QString::number(force, 16).toUpper().rightJustified(4, '0');
    tcpCore->sendMessageAsync(waitCommand.toUtf8(), false, responseData);

    //qDebug() << "设置5号电机拧紧力度:" << force << " 响应数据:" << responseData << " 命令:" << command;

}

void MainWindow::rotateMotor5ByCircles(double circles)
{
    // 将圈数转换为角度值（圈数 * 360度）
    int angleValue = static_cast<int>(circles * 360);

    // 构建5号电机旋转命令（寄存器0108用于旋转）
    QString rotateCommand = tcpCore->buildDeviceCommand("05", "06", "0108", angleValue, 4);

    // 发送命令（Hex模式）
    tcpCore->sendMessageAsync(rotateCommand.toUtf8(), false);

    qDebug() << "5号电机旋转:" << circles << "圈（角度值:" << angleValue << "度）";
}

