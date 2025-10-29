#include "mainwindow.h"
#include "tcpclientcore.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>
#include <iterator>

#include "box.h"
#include "reagentbottle.h"
#include "slot.h"
#include "ui_mainwindow.h"

void MainWindow::ForTempTest()
{
    // ========== 初始化转移区左边区域（15槽位）==========
    // 这里传入this是为了将MainWindow作为Box的父对象，从而利用Qt的对象树管理Box的生命周期
    transferAreaBox = new Box(15, this);
    qDebug() << "转移区左边区域初始化完成（15槽位）";

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

    // ========== 初始化TCP客户端核心 ==========
    tcpCore = new TcpClientCore(this);


    // ========== 按钮连接 ==========
    // 按钮1：连接并发送测试命令
    connect(ui->pushButton, &QPushButton::clicked, this, [=] {
        qDebug() << "=== 配方发送测试 ===";
        
        // 连接到TCP服务器
        tcpCore->connectToTcp("192.168.5.22", "192.168.5.201", 4196, true);

        qDebug() << "初始化Z";
        QString dataMsg = tcpCore->buildDeviceCommand("06", "G", "");
        tcpCore->sendMessage(dataMsg.toUtf8(), true);

        QTimer::singleShot(2000, this, [=] {
            qDebug() << "发送初始X";
            QString dataMsg = tcpCore->buildDeviceCommand("0A", "G", "");
            tcpCore->sendMessage(dataMsg.toUtf8(), true);
        });

        QTimer::singleShot(2060, this, [=] {
            qDebug() << "发送电爪初始化Y";
            QString dataMsg = tcpCore->buildDeviceCommand("09", "G", "");
            tcpCore->sendMessage(dataMsg.toUtf8(), true);
        });

        QTimer::singleShot(2120, this, [=] {
            qDebug() << "发送电爪初始化Y";
            QString dataMsg = tcpCore->buildDeviceCommand("05", "G", "");
            tcpCore->sendMessage(dataMsg.toUtf8(), true);
        });
    });
    
    // 按钮2：断开连接
    connect(ui->pushButton_2, &QPushButton::clicked, this, [=] {
        qDebug() << "断开TCP连接";
        tcpCore->disconnectFromTcp();
    });

    // 按钮3：获取坐标
    connect(ui->pushButton_3, &QPushButton::clicked, this, [=] {

        qDebug() << "获取坐标";
        QString dataMsg = tcpCore->buildDeviceCommand("0A", "E", "");
        tcpCore->sendMessage(dataMsg.toUtf8(), true);  // ✅ 改为 ASCII模式（true）

        QTimer::singleShot(60, this, [=] {
            // 使用 TcpClientCore 的函数构建设备命令（0A号设备，功能码G，无命令数据）
            QString dataMsg2 = tcpCore->buildDeviceCommand("09", "E", "");
            tcpCore->sendMessage(dataMsg2.toUtf8(), true);
        });

        QTimer::singleShot(60*2, this, [=] {
            // 使用 TcpClientCore 的函数构建设备命令（0A号设备，功能码G，无命令数据）
            QString dataMsg3 = tcpCore->buildDeviceCommand("06", "E", "");
            tcpCore->sendMessage(dataMsg3.toUtf8(), true);
        });
    });

    // 按钮4：复位
    connect(ui->pushButton_4, &QPushButton::clicked, this, [=] {

        qDebug() << "复位";
        QString dataMsg = tcpCore->buildDeviceCommand("06", "D", "00000000");
        tcpCore->sendMessage(dataMsg.toUtf8(), true);  // ✅ 改为 ASCII模式（true）


        QTimer::singleShot(60, this, [=] {
            // 使用新的5参数版本：十进制100，4位宽度（自动转为"0064"）
            QString dataMsg3 = tcpCore->buildDeviceCommand("05", "06", "0105", 0, 4); // 夹紧
            qDebug() << "夹紧：";
            tcpCore->sendMessage(dataMsg3.toUtf8(), false);
        });


        QTimer::singleShot(1000, this, [=] {
            // 使用 TcpClientCore 的函数构建设备命令（0A号设备，功能码G，无命令数据）
            QString dataMsg2 = tcpCore->buildDeviceCommand("09", "D", "00000000");
            tcpCore->sendMessage(dataMsg2.toUtf8(), true);
        });



        QTimer::singleShot(2000+60, this, [=] {
            // 使用 TcpClientCore 的函数构建设备命令（0A号设备，功能码G，无命令数据）
            QString dataMsg3 = tcpCore->buildDeviceCommand("0A", "D", "00000000");
            tcpCore->sendMessage(dataMsg3.toUtf8(), true);
        });

        QTimer::singleShot(2000+60*2, this, [=] {
            qDebug() << "发送电爪初始化Y";
            QString dataMsg = tcpCore->buildDeviceCommand("05", "G", "");
            tcpCore->sendMessage(dataMsg.toUtf8(), true);
        });
    });

    // 按钮5：移动
    connect(ui->pushButton_5, &QPushButton::clicked, this, [=] {
        qDebug() << "移动";
        QString dataMsg = tcpCore->buildDeviceCommand("0A", "D", "00003A99");
        tcpCore->sendMessage(dataMsg.toUtf8(), true);  // ✅ 改为 ASCII模式（true）

        QTimer::singleShot(60, this, [=] {
            QString dataMsg2 = tcpCore->buildDeviceCommand("09", "D", "00005B53");
            tcpCore->sendMessage(dataMsg2.toUtf8(), true);
        });

        QTimer::singleShot(1300, this, [=] {
            QString dataMsg3 = tcpCore->buildDeviceCommand("06", "D", "00041AC7");
            tcpCore->sendMessage(dataMsg3.toUtf8(), true);
        });

        QTimer::singleShot(4000, this, [=] {
            // 使用新的5参数版本：十进制100，4位宽度（自动转为"0064"）
            QString dataMsg3 = tcpCore->buildDeviceCommand("05", "06", "0105", ui->lineEdit->text().toInt(), 4); // 夹紧
            qDebug() << "夹紧：";
            tcpCore->sendMessage(dataMsg3.toUtf8(), false);
        });



    });
}

// 测试配方发送功能
void MainWindow::testRecipeSend(const QJsonObject& recipePacket)
{
    /**
     * X轴0A电机: 复位
     * X轴0A电机：移动n
     *
     */

    /**
     * Z轴0A电机: 复位
     * Z轴0A电机：移动n
     *
     */





}

