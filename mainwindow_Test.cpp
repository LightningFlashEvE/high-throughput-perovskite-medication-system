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
    // ========== 初始化TCP用来收取来自天平的串口信息 ==========
    tcpBalanceCore = new TcpClientCore(this);



    // ========== 按钮连接 ==========
    // 按钮1：连接并发送测试命令
    connect(ui->pushButton, &QPushButton::clicked, this, [=] {

        // 连接到TCP服务器
        tcpCore->connectToTcp("192.168.5.22", "192.168.5.201", 4196, true);
        tcpCore->initializeConnectionsAndTimers();

        tcpBalanceCore->connectToTcp("192.168.5.22", "192.168.5.202", 4196, true);
        tcpBalanceCore->initializeConnectionsForBalance();
        
        initializeAllDevices();

    });
    
    // 按钮2：断开连接
    connect(ui->pushButton_2, &QPushButton::clicked, this, [=] {
        qDebug() << "断开TCP连接";
        tcpCore->disconnectFromTcp();
        tcpBalanceCore->disconnectFromTcp();
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
    // takeEmptyBottle("Box_Transfer_Area_Right");

    // // 取液体，参数为液体名称，液体量
    // getLiquid("DMF", 0.1);

    // // 取固体
    // getSolid("Na2CO3", 0.1);

    // 拧紧瓶子
    tightenBottle();


    });





    
    // // ========== Y轴测试滑块：拖动释放后发送移动命令 ==========
    // // 设置滑块范围：0-57500（十进制）
    // ui->verticalSliderY->setRange(0, 57500);
    // ui->verticalSliderY->setValue(0);  // 初始值为0
    // ui->verticalSliderY->setInvertedAppearance(true); // 反转显示：最大在上，最小在下
    // if (ui->lineEdit_y) ui->lineEdit_y->setText(QString::number(ui->verticalSliderY->value()));

    // // 同步显示：滑块移动时，lineEdit_y 显示当前值（十进制）
    // connect(ui->verticalSliderY, &QSlider::valueChanged, this, [=](int v) {
    //     if (ui->lineEdit_y) ui->lineEdit_y->setText(QString::number(v));
    // });
    
    // // ========== X轴测试滑块：拖动释放后发送移动命令 ==========
    // // 设置滑块范围：0-57500（十进制）
    // ui->horizontalSliderX->setRange(0, 30000);
    // ui->horizontalSliderX->setValue(0);  // 初始值为0
    // ui->horizontalSliderX->setInvertedAppearance(true);
    // if (ui->lineEdit_x) ui->lineEdit_x->setText(QString::number(ui->horizontalSliderX->value()));

    // // 同步显示：滑块移动时，lineEdit_x 显示当前值（十进制）
    // connect(ui->horizontalSliderX, &QSlider::valueChanged, this, [=](int v) {
    //     if (ui->lineEdit_x) ui->lineEdit_x->setText(QString::number(v));
    // });

    // // 连接滑块的sliderReleased信号：只在鼠标释放后才发送命令
    // connect(ui->horizontalSliderX, &QSlider::sliderReleased, this, [=]() {

    // });
    // // 连接滑块的sliderReleased信号：只在鼠标释放后才发送命令
    // connect(ui->verticalSliderY, &QSlider::sliderReleased, this, [=]() {

    // });
}


// 测试配方发送功能（接收JSON对象）
void MainWindow::testRecipeSend(const QJsonObject& recipePacket)
{
    // 打印核心三部分
    qDebug() << "========== 收到配方（三部分） ==========";
    const QString equation = recipePacket.value("化学方程式").toString();
    qDebug() << "化学方程式:" << equation;

    // 溶质
    QJsonArray solutes = recipePacket.value("溶质").toArray();
    qDebug() << "溶质条目数:" << solutes.size();
    for (const auto &v : std::as_const(solutes)) {
        QJsonObject o = v.toObject();
        qDebug() << "溶质-名称:" << o.value("名称").toString()
                 << "用量:" << o.value("用量").toDouble()
                 << o.value("单位").toString();
    }

    // 溶剂
    QJsonArray solvents = recipePacket.value("溶剂").toArray();
    qDebug() << "溶剂条目数:" << solvents.size();
    for (const auto &v : std::as_const(solvents)) {
        QJsonObject o = v.toObject();
        qDebug() << "溶剂-名称:" << o.value("名称").toString()
                 << "用量:" << o.value("用量").toDouble()
                 << o.value("单位").toString();
    }

    // TODO: 在此处开始排打顺序执行（按溶质/溶剂分别处理）
    // 取空瓶（盘名称）
    //takeEmptyBottle("A盘");

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
    QString moveToZ4Command = tcpCore->buildDeviceCommand("08", "D", 4, 8); // 00000004
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

