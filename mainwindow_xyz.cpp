#include "mainwindow.h"
#include "qsqlerror.h"
#include "tcpclientcore.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>
#include <QThread>
#include <QTimer>
#include <QEventLoop>
#include <QSlider>
#include <QDateTime>
#include <QtSql/QSqlQuery>
#include "qsqldatabase.h"
// #include "box.h"
// #include "reagentbottle.h"
// #include "slot.h"
#include "ui_mainwindow.h"



void MainWindow::on_pushButton_7_clicked()
{


    closeBottleCap();
}

void MainWindow::on_pushButton_8_clicked()
{
    // 1、先释放5号夹爪
    QString openGripperCommand = tcpCore->buildDeviceCommand("05", "06", "0105", 0, 4);
    tcpCore->sendMessageAsync(openGripperCommand.toUtf8(), false, openGripperCommand);
    QString waitOpenGripperCommand = tcpCore->buildDeviceCommand("05", "03", "0202", 1, 4); // 05  03  0202  0001  25F6
    tcpCore->sendMessageAsync(waitOpenGripperCommand.toUtf8(), false, "0503020001");
}



































// 取空瓶（盘名称）函数定义框架
bool MainWindow::takeEmptyBottle(const QString& trayName)
{

    Q_UNUSED(trayName);

    // TODO: 在此处编写取空瓶的具体实现
    // 查询数据库表 Box_Transfer_Area_Right 获取 x y z 和 currentIndex四个信息
    //QString transferAreaSql2 = "SELECT originX, originY, gripperZ, currentIndex FROM Box_Transfer_Area_Right";
    QString transferAreaSql = "SELECT originX, originY, gripperZ, currentIndex, rightSpacing, bottomSpacing ,cols, rows FROM other WHERE name = 'emptyBottleArea'";
    QSqlQuery transferAreaQuery = dbm->query(transferAreaSql);

    // 改用int类型
    int transferOriginX, transferOriginY, transferGripperZ, transferSlotIndex, transferRightSpacing, transferBottomSpacing, transferCols, transferRows;

    if (transferAreaQuery.next()) {
        // 在赋值时直接转换为8位十六进制字符串（大写）
        transferOriginX = transferAreaQuery.value("originX").toInt();
        transferOriginY = transferAreaQuery.value("originY").toInt();
        transferGripperZ = transferAreaQuery.value("gripperZ").toInt();
        transferSlotIndex = transferAreaQuery.value("currentIndex").toInt();
        transferRightSpacing = transferAreaQuery.value("rightSpacing").toDouble();
        transferBottomSpacing = transferAreaQuery.value("bottomSpacing").toDouble();
        transferCols = transferAreaQuery.value("cols").toInt();
        transferRows = transferAreaQuery.value("rows").toInt();
        qDebug() << "Transfer area coordinates:" << transferOriginX << transferOriginY << transferGripperZ << "slot index:" << transferSlotIndex << "rightSpacing:" << transferRightSpacing << "bottomSpacing:" << transferBottomSpacing << "cols:" << transferCols << "rows:" << transferRows;
        Q_UNUSED(transferSlotIndex);
    } else {
        qWarning() << "未找到 Box_Transfer_Area_Right 表的数据";
        return false;
    }

    // 这里获取的是盘的首坐标，位于盘的左上角，以它为原点。使用的时候从原点开始往右边取。
    // 使用 calculateSlotPosition 函数计算目标槽位坐标
    // 配置槽位参数（实际应从数据库或配置中获取）
    SlotPositionConfig config(transferOriginX, transferOriginY, transferCols, transferRows, transferRightSpacing, transferBottomSpacing); // 示例：2列3行，间距3和4

    QPoint targetPos = calculateSlotPosition(config, transferSlotIndex);
    int emptyBottleAreaTargetX = targetPos.x();
    int emptyBottleAreaTargetY = targetPos.y();

    qDebug() << "计算槽位坐标: 原点(" << transferOriginX << "," << transferOriginY
             << ") 索引:" << transferSlotIndex
             << " -> 目标(" << emptyBottleAreaTargetX << "," << emptyBottleAreaTargetY << ")";

    QString moveToTransferXCommand = tcpCore->buildDeviceCommand("0A", "D", emptyBottleAreaTargetX, 8);
    tcpCore->sendMessageAsync(moveToTransferXCommand.toUtf8(), true);
    QString waitTransferXCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    tcpCore->sendMessageAsync(waitTransferXCommand.toUtf8(), true, "0Ad01");
    QString moveToTransferYCommand = tcpCore->buildDeviceCommand("09", "D", emptyBottleAreaTargetY, 8);
    tcpCore->sendMessageAsync(moveToTransferYCommand.toUtf8(), true);
    QString waitTransferYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    tcpCore->sendMessageAsync(waitTransferYCommand.toUtf8(), true, "09d01");
    QString moveToTransferZCommand = tcpCore->buildDeviceCommand("06", "D", transferGripperZ, 8);
    tcpCore->sendMessageAsync(moveToTransferZCommand.toUtf8(), true);
    QString waitTransferZCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    tcpCore->sendMessageAsync(waitTransferZCommand.toUtf8(), true, "06d01");

    // 先释放5号夹爪，确保状态重置（解决第二次执行时夹爪不夹的问题）
    QString releaseGripperBeforeCatchCommand = tcpCore->buildDeviceCommand("05", "06", "0105", 0, 4);
    tcpCore->sendMessageAsync(releaseGripperBeforeCatchCommand.toUtf8(), false);
    QString waitReleaseGripperBeforeCatchCommand = tcpCore->buildDeviceCommand("05", "03", "0202", 1, 4);
    tcpCore->sendMessageAsync(waitReleaseGripperBeforeCatchCommand.toUtf8(), false, "0503020001");

    // Catch it - 夹紧5号夹爪
    QString enableGripperCommand = tcpCore->buildDeviceCommand("05", "06", "0105", 100, 4);
    tcpCore->sendMessageAsync(enableGripperCommand.toUtf8(), false);
    QString waitGripperEnableCommand = tcpCore->buildDeviceCommand("05", "03", "0202", 1, 4);
    tcpCore->sendMessageAsync(waitGripperEnableCommand.toUtf8(), false, "0503020002"); // 目前是01

    // 去数据库把emptyBottleArea里面的currentIndex值+1，并更新到数据库
    tcpCore->sendMessageAsync("AAemptyBottleAreaCurrentIndexPlusOne");  

    // Rise
    QString raiseTransferZCommand = tcpCore->buildDeviceCommand("06", "D", 0, 8);
    tcpCore->sendMessageAsync(raiseTransferZCommand.toUtf8(), true);
    QString waitTransferZRaisedCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    tcpCore->sendMessageAsync(waitTransferZRaisedCommand.toUtf8(), true, "06d01");// 目前是01

    // Open the gripper and wait for the empty bottle to come
    // 夹持区open
    QString openGripCommand = tcpCore->buildDeviceCommand("0B", "06", "0105", 0, 4);
    tcpCore->sendMessageAsync(openGripCommand.toUtf8(), false, "0B0601050000");
    QString waitGripOpenFeedbackCommand = tcpCore->buildDeviceCommand("0B", "03", "0202", 1, 4);
    tcpCore->sendMessageAsync(waitGripOpenFeedbackCommand.toUtf8(), false, "0B03020001");
    // 去other表读取gripArea的x y z值，然后移动到gripArea
    QString gripAreaSql = "SELECT originX, originY, gripperZ FROM other WHERE name = 'gripArea'";
    QSqlQuery gripAreaQuery = dbm->query(gripAreaSql);
    int gripAreaX, gripAreaY, gripAreaZ;
    if (gripAreaQuery.next()) {
        // 在赋值时直接转换为8位十六进制字符串（大写）
        gripAreaX = gripAreaQuery.value("originX").toInt();
        gripAreaY = gripAreaQuery.value("originY").toInt();
        gripAreaZ = gripAreaQuery.value("gripperZ").toInt();
    } else {
        qWarning() << "未找到 gripArea 的数据";
        return false;
    }
    QString moveToGripAreaXCommand = tcpCore->buildDeviceCommand("0A", "D", gripAreaX, 8);
    tcpCore->sendMessageAsync(moveToGripAreaXCommand.toUtf8(), true);
    QString waitGripAreaXCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    tcpCore->sendMessageAsync(waitGripAreaXCommand.toUtf8(), true, "0Ad01");
    QString moveToGripAreaYCommand = tcpCore->buildDeviceCommand("09", "D", gripAreaY, 8);
    tcpCore->sendMessageAsync(moveToGripAreaYCommand.toUtf8(), true);
    QString waitGripAreaYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    tcpCore->sendMessageAsync(waitGripAreaYCommand.toUtf8(), true, "09d01");
    QString moveToGripAreaZCommand = tcpCore->buildDeviceCommand("06", "D", gripAreaZ, 8);
    tcpCore->sendMessageAsync(moveToGripAreaZCommand.toUtf8(), true);
    QString waitGripAreaZCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    tcpCore->sendMessageAsync(waitGripAreaZCommand.toUtf8(), true, "06d01");


    // 调整电爪的夹紧与松开的力矩
    QString setGripTorqueCommand = tcpCore->buildDeviceCommand("0B", "06", "0103", 100, 4); // 0B0601030064
    tcpCore->sendMessageAsync(setGripTorqueCommand.toUtf8(), false);

    // 电爪夹住玻璃瓶
    QString closeGripCommand = tcpCore->buildDeviceCommand("0B", "06", "0105", 100, 4); // 0B0601030064
    tcpCore->sendMessageAsync(closeGripCommand.toUtf8(), false, "0B0601050064");
    QString waitGripClosedCommand = tcpCore->buildDeviceCommand("0B", "03", "0202", 1, 4);
    tcpCore->sendMessageAsync(waitGripClosedCommand.toUtf8(), false, "0B03020002");  // 目前是01

    // 开盖
#if 0
    // Rotate to open the cover
    // 电爪旋转
    QString rotateGripCommand = tcpCore->buildDeviceCommand("05", "06", "0108", 64996, 4); // 旋转1.5圈
    tcpCore->sendMessageAsync(rotateGripCommand.toUtf8(), false);
    QString readGripperRotationCommand = tcpCore->buildDeviceCommand("05", "03", "0203", 1, 4); // -> 050302030001 + CRC
    tcpCore->sendMessageAsync(readGripperRotationCommand.toUtf8(), false, "0503020001");

#else
    openBottleCap();

#endif



    // Z轴上移一点
    QString liftZAfterRotationCommand = tcpCore->buildDeviceCommand("06", "D", 163687, 8);
    tcpCore->sendMessageAsync(liftZAfterRotationCommand.toUtf8(), true);
    QString waitZAfterRotationCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    tcpCore->sendMessageAsync(waitZAfterRotationCommand.toUtf8(), true, "06d01");

    // // 电爪旋转后归零，开盖里面带了归零
    // QString rotateGripToZeroCommand = tcpCore->buildDeviceCommand("05", "06", "0108", 0, 4); // 归零
    // tcpCore->sendMessageAsync(rotateGripToZeroCommand.toUtf8(), false);
    // QString waitRotateGripToZeroCommand = tcpCore->buildDeviceCommand("05", "03", "0203", 1, 4); // 旋转1.5圈
    // tcpCore->sendMessageAsync(waitRotateGripToZeroCommand.toUtf8(), false, "0503020001");

    // 夹持区力矩/速度设置：写0103=0x003C（60）
    QString setGripAreaTorqueCommand = tcpCore->buildDeviceCommand("0B", "06", "0103", "003C");
    tcpCore->sendMessageAsync(setGripAreaTorqueCommand.toUtf8(), false, "0B060103003C");
    // 释放使能电爪
    // QString disableGripperCommand = tcpCore->buildDeviceCommand("0B", "06", "0304", 1, 4);
    // tcpCore->sendMessageAsync(disableGripperCommand.toUtf8(), false, "0B0303040001");

    // 移动到放瓶盖区域放瓶盖，运动xyz
    QString moveToCapDropXCommand = tcpCore->buildDeviceCommand("0A", "D", 17464, 8);  // 17464
    tcpCore->sendMessageAsync(moveToCapDropXCommand.toUtf8(), true);
    QString waitCapDropXCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    tcpCore->sendMessageAsync(waitCapDropXCommand.toUtf8(), true, "0Ad01");

    QString moveToCapDropYCommand = tcpCore->buildDeviceCommand("09", "D", 35572, 8); // 35572
    tcpCore->sendMessageAsync(moveToCapDropYCommand.toUtf8(), true);
    QString waitCapDropYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    tcpCore->sendMessageAsync(waitCapDropYCommand.toUtf8(), true, "09d01");

    QString moveToCapDropZCommand = tcpCore->buildDeviceCommand("06", "D", 257246, 8); // 257246
    tcpCore->sendMessageAsync(moveToCapDropZCommand.toUtf8(), true);
    QString waitCapDropZCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    tcpCore->sendMessageAsync(waitCapDropZCommand.toUtf8(), true, "06d01");

    // 松爪并归位
    QString homeGripReleaseCommand = tcpCore->buildDeviceCommand("05", "06", "0105", 0, 4); // 0B0601030064
    tcpCore->sendMessageAsync(homeGripReleaseCommand.toUtf8(), false);
    QString waitHomeGripReleaseCommand = tcpCore->buildDeviceCommand("05", "03", "0202", 1, 4);
    tcpCore->sendMessageAsync(waitHomeGripReleaseCommand.toUtf8(), false, "0503020001");  // 这里看情况修改

    // Z轴上移一点
    tcpCore->sendMessageAsync(liftZAfterRotationCommand.toUtf8(), true);
    QString waitZeroZCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    tcpCore->sendMessageAsync(waitZeroZCommand.toUtf8(), true, "06d01");

    // 回到gripArea
    tcpCore->sendMessageAsync(moveToGripAreaXCommand.toUtf8(), true); // x
    tcpCore->sendMessageAsync(waitGripAreaXCommand.toUtf8(), true, "0Ad01");
    tcpCore->sendMessageAsync(moveToGripAreaYCommand.toUtf8(), true); // y
    tcpCore->sendMessageAsync(waitGripAreaYCommand.toUtf8(), true, "09d01");
    QString back2gripAreaZCommand = tcpCore->buildDeviceCommand("06", "D", 269680, 8); //000415F0
    tcpCore->sendMessageAsync(back2gripAreaZCommand.toUtf8(), true); // z
    QString waitback2gripAreaZCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    tcpCore->sendMessageAsync(waitback2gripAreaZCommand.toUtf8(), true, "06d01");


    // 夹住玻璃瓶，松爪，z轴回到零点
    tcpCore->sendMessageAsync(enableGripperCommand.toUtf8(), false);
    tcpCore->sendMessageAsync(waitGripperEnableCommand.toUtf8(), false, "0503020002"); // 目前是01

    QString oprnGripCommand = tcpCore->buildDeviceCommand("0B", "06", "0105", 1, 4);
    tcpCore->sendMessageAsync(oprnGripCommand.toUtf8(), false);
    tcpCore->sendMessageAsync(oprnGripCommand.toUtf8(), false);
    QString waitGripOpendCommand = tcpCore->buildDeviceCommand("0B", "03", "0202", 1, 4);
    tcpCore->sendMessageAsync(waitGripOpendCommand.toUtf8(), false, "0B03020001");  // 目前是01

    tcpCore->sendMessageAsync(raiseTransferZCommand.toUtf8(), true);
    tcpCore->sendMessageAsync(waitTransferZRaisedCommand.toUtf8(), true, "06d01");










    // 移动到天平   x21451  y8556  z271692
    /**
     * 移动到天平
     *
     * 数据库获取 balanceArea的xyz
     * 6，9，10电机移动到balanceArea的xyz
     */
    QString balanceAreaSql = "SELECT originX, originY, gripperZ FROM other WHERE name = 'balanceArea'";
    QSqlQuery balanceAreaQuery = dbm->query(balanceAreaSql);
    int balanceAreaX=0, balanceAreaY=0, balanceAreaZ=0;
    if (balanceAreaQuery.next()) {
        balanceAreaX = balanceAreaQuery.value("originX").toInt();
        balanceAreaY = balanceAreaQuery.value("originY").toInt();
        balanceAreaZ = balanceAreaQuery.value("gripperZ").toInt();
        qDebug() << "-----------------------" << balanceAreaX << balanceAreaY << balanceAreaZ;
    } else {
        qWarning() << "未找到 balanceArea 的数据";
        return false;
    }
    QString moveToBalanceAreaXCommand = tcpCore->buildDeviceCommand("0A", "D", balanceAreaX, 8);
    tcpCore->sendMessageAsync(moveToBalanceAreaXCommand.toUtf8(), true);
    QString waitBalanceAreaXCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    tcpCore->sendMessageAsync(waitBalanceAreaXCommand.toUtf8(), true, "0Ad01");
    QString moveToBalanceAreaYCommand = tcpCore->buildDeviceCommand("09", "D", balanceAreaY, 8);
    tcpCore->sendMessageAsync(moveToBalanceAreaYCommand.toUtf8(), true);
    QString waitBalanceAreaYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    tcpCore->sendMessageAsync(waitBalanceAreaYCommand.toUtf8(), true, "09d01");
    QString moveToBalanceAreaZCommand = tcpCore->buildDeviceCommand("06", "D", balanceAreaZ, 8);
    tcpCore->sendMessageAsync(moveToBalanceAreaZCommand.toUtf8(), true);
    QString waitBalanceAreaZCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    tcpCore->sendMessageAsync(waitBalanceAreaZCommand.toUtf8(), true, "06d01");



    // 释放，z轴回到零点
    tcpCore->sendMessageAsync(homeGripReleaseCommand.toUtf8(), false);
    tcpCore->sendMessageAsync(waitHomeGripReleaseCommand.toUtf8(), false, "0503020001");  // 这里看情况修改
    tcpCore->sendMessageAsync(raiseTransferZCommand.toUtf8(), true);
    tcpCore->sendMessageAsync(waitTransferZRaisedCommand.toUtf8(), true, "06d01");


    return false;
}


// 取液体
bool MainWindow::getLiquid(const QString& liquidName, double volumeMl, int& tipsNum)
{
    Q_UNUSED(liquidName);
    Q_UNUSED(volumeMl);

    /**
     * 移动到liquidName的xyz坐标：去数据库找表LiquidMaterialArea，取字段name的liquidName的那一行，获取originX，originY，gripperZ
     * 根据源坐标和currentIndex计算xy，改成用selfLocation计算
     */
    QString liquidNameSql = "SELECT originX, originY, gripperZ, rightSpacing, bottomSpacing, cols, rows, selfLocation FROM LiquidMaterialArea WHERE liquidName = '" + liquidName + "'";
    QSqlQuery liquidNameQuery = dbm->query(liquidNameSql);
    int liquidNameX=0, liquidNameY=0, liquidNameZ=0, liquidNameRightSpacing=0, liquidNameBottomSpacing=0, liquidNameCols=0, liquidNameRows=0, liquidNameSelfLocation=0;
    if (liquidNameQuery.next()) {
        liquidNameX = liquidNameQuery.value("originX").toInt();
        liquidNameY = liquidNameQuery.value("originY").toInt();
        liquidNameZ = liquidNameQuery.value("gripperZ").toInt();
        liquidNameRightSpacing = liquidNameQuery.value("rightSpacing").toDouble();
        liquidNameBottomSpacing = liquidNameQuery.value("bottomSpacing").toDouble();
        liquidNameCols = liquidNameQuery.value("cols").toInt();
        liquidNameRows = liquidNameQuery.value("rows").toInt();
        liquidNameSelfLocation = liquidNameQuery.value("selfLocation").toInt();
    } else {
        qWarning() << "未找到 liquidName = " << liquidName << " 的数据";
        return false;
    }


    SlotPositionConfig liquidNameConfig(liquidNameX, liquidNameY, liquidNameCols, liquidNameRows, liquidNameRightSpacing, liquidNameBottomSpacing);
    QPoint targetPos = calculateSlotPosition(liquidNameConfig, liquidNameSelfLocation);
    int liquidNameTargetX = targetPos.x();
    int liquidNameTargetY = targetPos.y();

    QString moveToLiquidNameXCommand = tcpCore->buildDeviceCommand("0A", "D", liquidNameTargetX, 8);
    tcpCore->sendMessageAsync(moveToLiquidNameXCommand.toUtf8(), true);
    QString waitLiquidNameXCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    tcpCore->sendMessageAsync(waitLiquidNameXCommand.toUtf8(), true, "0Ad01");

    QString moveToLiquidNameYCommand = tcpCore->buildDeviceCommand("09", "D", liquidNameTargetY, 8);
    tcpCore->sendMessageAsync(moveToLiquidNameYCommand.toUtf8(), true);
    QString waitLiquidNameYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    tcpCore->sendMessageAsync(waitLiquidNameYCommand.toUtf8(), true, "09d01");

    QString moveToLiquidNameZCommand = tcpCore->buildDeviceCommand("06", "D", liquidNameZ, 8);
    tcpCore->sendMessageAsync(moveToLiquidNameZCommand.toUtf8(), true);
    QString waitLiquidNameZCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    tcpCore->sendMessageAsync(waitLiquidNameZCommand.toUtf8(), true, "06d01");



    /**
     * 夹住，上移
     *
     */
    QString enableGripperCommand = tcpCore->buildDeviceCommand("05", "06", "0105", 100, 4);
    tcpCore->sendMessageAsync(enableGripperCommand.toUtf8(), false);
    QString waitGripperEnableCommand = tcpCore->buildDeviceCommand("05", "03", "0202", 1, 4);
    tcpCore->sendMessageAsync(waitGripperEnableCommand.toUtf8(), false, "0503020002"); // 目前是01
    QString raiseTransferZCommand = tcpCore->buildDeviceCommand("06", "D", 0, 8);
    tcpCore->sendMessageAsync(raiseTransferZCommand.toUtf8(), true);
    QString waitTransferZRaisedCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    tcpCore->sendMessageAsync(waitTransferZRaisedCommand.toUtf8(), true, "06d01");// 目前是01


    /**
     * 打开固定夹爪    移动到夹持区
     *
     *
     */
    QString openGripCommand = tcpCore->buildDeviceCommand("0B", "06", "0105", 0, 4);
    tcpCore->sendMessageAsync(openGripCommand.toUtf8(), false, "0B0601050000");
    QString waitGripOpenFeedbackCommand = tcpCore->buildDeviceCommand("0B", "03", "0202", 1, 4);
    tcpCore->sendMessageAsync(waitGripOpenFeedbackCommand.toUtf8(), false, "0B03020001");

    // 去other表读取gripArea的x y z值，然后移动到gripArea
    QString gripAreaSql = "SELECT originX, originY, gripperZ FROM other WHERE name = 'gripArea'";
    QSqlQuery gripAreaQuery = dbm->query(gripAreaSql);
    int gripAreaX, gripAreaY, gripAreaZ;
    if (gripAreaQuery.next()) {
        // 在赋值时直接转换为8位十六进制字符串（大写）
        gripAreaX = gripAreaQuery.value("originX").toInt();
        gripAreaY = gripAreaQuery.value("originY").toInt();
        gripAreaZ = gripAreaQuery.value("gripperZ").toInt();
    } else {
        qWarning() << "未找到 gripArea 的数据";
        return false;
    }
    QString moveToGripAreaXCommand = tcpCore->buildDeviceCommand("0A", "D", gripAreaX, 8);
    tcpCore->sendMessageAsync(moveToGripAreaXCommand.toUtf8(), true);
    QString waitGripAreaXCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    tcpCore->sendMessageAsync(waitGripAreaXCommand.toUtf8(), true, "0Ad01");

    QString moveToGripAreaYCommand = tcpCore->buildDeviceCommand("09", "D", gripAreaY, 8);
    tcpCore->sendMessageAsync(moveToGripAreaYCommand.toUtf8(), true);
    QString waitGripAreaYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    tcpCore->sendMessageAsync(waitGripAreaYCommand.toUtf8(), true, "09d01");

    QString moveToGripAreaZCommand = tcpCore->buildDeviceCommand("06", "D", gripAreaZ, 8);
    tcpCore->sendMessageAsync(moveToGripAreaZCommand.toUtf8(), true);
    QString waitGripAreaZCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    tcpCore->sendMessageAsync(waitGripAreaZCommand.toUtf8(), true, "06d01");


    /**
     * 夹住玻璃瓶,旋转开盖,上移到零点。
     *
     * 问题点，不会旋转。已经解决
     */
    QString closeGripCommand = tcpCore->buildDeviceCommand("0B", "06", "0105", 100, 4); // 0B0601030064
    tcpCore->sendMessageAsync(closeGripCommand.toUtf8(), false);
    QString waitGripClosedCommand = tcpCore->buildDeviceCommand("0B", "03", "0202", 1, 4);
    tcpCore->sendMessageAsync(waitGripClosedCommand.toUtf8(), false, "0B03020002");  // 目前是01

    openBottleCap(); // 开盖自带归零

    tcpCore->sendMessageAsync(raiseTransferZCommand.toUtf8(), true);
    tcpCore->sendMessageAsync(waitTransferZRaisedCommand.toUtf8(), true, "06d01");// 目前是01


    // // 电爪旋转归零
    QString rotateGripToZeroCommand = tcpCore->buildDeviceCommand("05", "06", "0108", 0, 4); // 归零
    QString waitRotateGripToZeroCommand = tcpCore->buildDeviceCommand("05", "03", "0203", 1, 4); // 旋转1.5圈





    /**
     * 移动到tips头区域，下移取tips头，上移（当前进度）
     *
     * 数据库获取 tips头的xy currentIndex

    // TODO: 在此处编写取空瓶的具体实现
    // 查询数据库表 other 获取 tipsHeadArea 的xy 和 currentIndex
    // 计算xy
     */
    QString tipsHeadSql = "SELECT originX, originY, currentIndex, cols, rows, rightSpacing, bottomSpacing FROM other WHERE name = 'tipsHeadArea'";
    QSqlQuery tipsHeadQuery = dbm->query(tipsHeadSql);
    int tipsHeadX=0, tipsHeadY=0, tipsHeadSlotIndex=0, tipsHeadCols=0, tipsHeadRows=0, tipsHeadRightSpacing=0, tipsHeadBottomSpacing=0;
    if (tipsHeadQuery.next()) {
        tipsHeadX = tipsHeadQuery.value("originX").toInt();
        tipsHeadY = tipsHeadQuery.value("originY").toInt();
        tipsHeadSlotIndex = tipsHeadQuery.value("currentIndex").toInt();
        tipsHeadCols = tipsHeadQuery.value("cols").toInt();
        tipsHeadRows = tipsHeadQuery.value("rows").toInt();
        tipsHeadRightSpacing = tipsHeadQuery.value("rightSpacing").toDouble();
        tipsHeadBottomSpacing = tipsHeadQuery.value("bottomSpacing").toDouble();
        // 将获取到的tips索引赋值给外部变量
    }
    SlotPositionConfig config(tipsHeadX, tipsHeadY, tipsHeadCols, tipsHeadRows, tipsHeadRightSpacing, tipsHeadBottomSpacing);
    QPoint tipstargetPos = calculateSlotPosition(config, tipsHeadSlotIndex+tipsNum);
    tipsNum++; // 解决同步过程使用异步导致tips头索引不同步的问题
    int tipsHeadTargetX = tipstargetPos.x();
    int tipsHeadTargetY = tipstargetPos.y();


    QString moveToTipsHeadXCommand = tcpCore->buildDeviceCommand("0A", "D", tipsHeadTargetX, 8);
    tcpCore->sendMessageAsync(moveToTipsHeadXCommand.toUtf8(), true);
    QString waitTipsHeadXCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    tcpCore->sendMessageAsync(waitTipsHeadXCommand.toUtf8(), true, "0Ad01");

    QString moveToTipsHeadYCommand = tcpCore->buildDeviceCommand("09", "D", tipsHeadTargetY, 8);
    tcpCore->sendMessageAsync(moveToTipsHeadYCommand.toUtf8(), true);
    QString waitTipsHeadYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    tcpCore->sendMessageAsync(waitTipsHeadYCommand.toUtf8(), true, "09d01");

    // 下移去获取tip头命令：08i000249F0C959
    QString moveToGetTipZCommand = tcpCore->buildDeviceCommand("08", "i", 150000, 8); // 000249F0
    tcpCore->sendMessageAsync(moveToGetTipZCommand.toUtf8(), true);
    QString waitMoveToGetTipZCommand = tcpCore->buildDeviceCommand("08", "d", 0, 0);
    tcpCore->sendMessageAsync(waitMoveToGetTipZCommand.toUtf8(), true, "08d01");

    // tips头序号加一
    tcpCore->sendMessageAsync("AAtipsHeadAreaCurrentIndexPlusOne");



    // 上移 tip头命令： 08D000000004
    QString raiseTipZCommand = tcpCore->buildDeviceCommand("08", "D", 100, 8); // 00000004
    tcpCore->sendMessageAsync(raiseTipZCommand.toUtf8(), true);
    QString waitRaiseTipZCommand = tcpCore->buildDeviceCommand("08", "d", 0, 0);
    tcpCore->sendMessageAsync(waitRaiseTipZCommand.toUtf8(), true, "08d01");



    // 07n00C8 吸液volumeMl微升（volumeMl是ml，要转成微升，再转成4位十六进制，前面带0）每次最多吸1000微升，如果超过1000微升，则分多次吸液
    const int volumeUl = static_cast<int>(volumeMl * 1000.0 + 0.5);
    // 1.移动到移动到夹持区下移
    QString gripLiquidAreaSql = "SELECT originX, originY FROM other WHERE name = 'gripLiquidArea'";
    QSqlQuery gripLiquidAreaQuery = dbm->query(gripLiquidAreaSql);
    int gripLiquidAreaX=0, gripLiquidAreaY=0;
    if (gripLiquidAreaQuery.next()) {
        gripLiquidAreaX = gripLiquidAreaQuery.value("originX").toInt();
        gripLiquidAreaY = gripLiquidAreaQuery.value("originY").toInt();
    } else {
        qWarning() << "未找到 gripArea 的数据";
        return false;
    }
    QString moveToGripLiquidAreaXCommand = tcpCore->buildDeviceCommand("0A", "D", gripLiquidAreaX, 8);
    tcpCore->sendMessageAsync(moveToGripLiquidAreaXCommand.toUtf8(), true);
    QString waitGripLiquidAreaXCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    tcpCore->sendMessageAsync(waitGripLiquidAreaXCommand.toUtf8(), true, "0Ad01");
    QString moveToGripLiquidAreaYCommand = tcpCore->buildDeviceCommand("09", "D", gripLiquidAreaY, 8);
    tcpCore->sendMessageAsync(moveToGripLiquidAreaYCommand.toUtf8(), true);
    QString waitGripLiquidAreaYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    tcpCore->sendMessageAsync(waitGripLiquidAreaYCommand.toUtf8(), true, "09d01");

    // 2.吸液
    // 2.1  07N01 N01气压探测
    QString enablePressureDetectionCommand = tcpCore->buildDeviceCommand("07", "N", 1, 2);
    tcpCore->sendMessageAsync(enablePressureDetectionCommand.toUtf8(), true, "07N");
    // 2.2  08j000001F400004D73 高速下移，再慢速下移，在低速段内检测到液位信号，电机立即停止
    QString liquidProbeComboCommand = tcpCore->buildDeviceCommand("08", "j", 2147483667827, 16); // 000001F400004D73
    tcpCore->sendMessageAsync(liquidProbeComboCommand.toUtf8(), true, "08j");
    QString waitEnablePressureDetectionCommand = tcpCore->buildDeviceCommand("07", "d", 0, 0);
    tcpCore->sendMessageAsync(waitEnablePressureDetectionCommand.toUtf8(), true, "07d03"); // d03 为探测到液面状态
    QString waitLiquidProbeComboCommand = tcpCore->buildDeviceCommand("08", "d", 0, 0);
    tcpCore->sendMessageAsync(waitLiquidProbeComboCommand.toUtf8(), true, "08d03");
    // 2.3  07N00 00是关闭气压探测
    QString disablePressureDetectionCommand = tcpCore->buildDeviceCommand("07", "N", 0, 2);
    tcpCore->sendMessageAsync(disablePressureDetectionCommand.toUtf8(), true);
    // 2.4  吸
    QString aspirateVolumeCommand = tcpCore->buildDeviceCommand("07", "n", volumeUl, 4);
    tcpCore->sendMessageAsync(aspirateVolumeCommand.toUtf8(), true);
    QString waitAspirateVolumeCommand = tcpCore->buildDeviceCommand("07", "d", 0, 0);
    tcpCore->sendMessageAsync(waitAspirateVolumeCommand.toUtf8(), true, "07d01");

    // 3.吸完液后上移
    tcpCore->sendMessageAsync(raiseTipZCommand.toUtf8(), true);
    tcpCore->sendMessageAsync(waitRaiseTipZCommand.toUtf8(), true, "08d01"); // d01 为上移到位

    // 4.移动到天平下移
    QString balanceAreaSql = "SELECT originX, originY, tipsZ FROM other WHERE name = 'balanceAreaForTipsArea'";
    QSqlQuery balanceAreaQuery = dbm->query(balanceAreaSql);
    int balanceAreaForTipsAreaX=0, balanceAreaForTipsAreaY=0,  balanceAreaForTipsAreaZ=0;
    if (balanceAreaQuery.next()) {
        balanceAreaForTipsAreaX = balanceAreaQuery.value("originX").toInt();
        balanceAreaForTipsAreaY = balanceAreaQuery.value("originY").toInt();
        balanceAreaForTipsAreaZ = balanceAreaQuery.value("tipsZ").toInt();
    }
    else {
        qWarning() << "未找到 gripArea 的数据";
        return false;
    }
    QString moveToBalanceAreaXCommand = tcpCore->buildDeviceCommand("0A", "D", balanceAreaForTipsAreaX, 8);
    tcpCore->sendMessageAsync(moveToBalanceAreaXCommand.toUtf8(), true);
    QString waitBalanceAreaXCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    tcpCore->sendMessageAsync(waitBalanceAreaXCommand.toUtf8(), true, "0Ad01");
    QString moveToBalanceAreaYCommand = tcpCore->buildDeviceCommand("09", "D", balanceAreaForTipsAreaY, 8);
    tcpCore->sendMessageAsync(moveToBalanceAreaYCommand.toUtf8(), true);
    QString waitBalanceAreaYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    tcpCore->sendMessageAsync(waitBalanceAreaYCommand.toUtf8(), true, "09d01");
    QString moveToBalanceAreaZCommand = tcpCore->buildDeviceCommand("08", "D", balanceAreaForTipsAreaZ, 8);
    tcpCore->sendMessageAsync(moveToBalanceAreaZCommand.toUtf8(), true);
    QString waitBalanceAreaZCommand = tcpCore->buildDeviceCommand("08", "d", 0, 0);
    tcpCore->sendMessageAsync(waitBalanceAreaZCommand.toUtf8(), true, "08d01");

    // 5.吐液
    QString dispenseVolumeCommand = tcpCore->buildDeviceCommand("07", "p", 0, 4);
    tcpCore->sendMessageAsync(dispenseVolumeCommand.toUtf8(), true);
    QString waitDispenseVolumeCommand = tcpCore->buildDeviceCommand("07", "d", 0, 0);
    tcpCore->sendMessageAsync(waitDispenseVolumeCommand.toUtf8(), true, "07d01");

    // 6.上移
    tcpCore->sendMessageAsync(raiseTipZCommand.toUtf8(), true);
    tcpCore->sendMessageAsync(waitRaiseTipZCommand.toUtf8(), true, "08d01");




    /**
     * 移动到废弃区域，丢弃tips头
     *
     * 数据库获取 wasteArea（垃圾区域）的xyz
     */
    QString wasteAreaSql = "SELECT originX, originY, tipsZ FROM other WHERE name = 'wasteArea'";
    QSqlQuery wasteAreaQuery = dbm->query(wasteAreaSql);
    int wasteAreaX, wasteAreaY, wastetipsZ;
    if (wasteAreaQuery.next()) {
        wasteAreaX = wasteAreaQuery.value("originX").toInt();
        wasteAreaY = wasteAreaQuery.value("originY").toInt();
        wastetipsZ = wasteAreaQuery.value("tipsZ").toInt();
        qDebug() << "wasteAreaX: " << wasteAreaX << "wasteAreaY: " << wasteAreaY << "wastetipsZ: " << wastetipsZ;
    } else {
        qWarning() << "未找到 wasteArea 的数据";
        return false;
    }
    QString moveToWasteAreaXCommand = tcpCore->buildDeviceCommand("0A", "D", wasteAreaX, 8);
    tcpCore->sendMessageAsync(moveToWasteAreaXCommand.toUtf8(), true);
    QString waitWasteAreaXCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    tcpCore->sendMessageAsync(waitWasteAreaXCommand.toUtf8(), true, "0Ad01");
    QString moveToWasteAreaYCommand = tcpCore->buildDeviceCommand("09", "D", wasteAreaY, 8);
    tcpCore->sendMessageAsync(moveToWasteAreaYCommand.toUtf8(), true);
    QString waitWasteAreaYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    tcpCore->sendMessageAsync(waitWasteAreaYCommand.toUtf8(), true, "09d01");
    QString moveToWasteAreaZCommand = tcpCore->buildDeviceCommand("08", "D", wastetipsZ, 8);
    tcpCore->sendMessageAsync(moveToWasteAreaZCommand.toUtf8(), true);
    QString waitWasteAreaZCommand = tcpCore->buildDeviceCommand("08", "d", 0, 0);
    tcpCore->sendMessageAsync(waitWasteAreaZCommand.toUtf8(), true, "08d01");
    // 丢弃tips头命令：07Q
    QString discardTipHeadCommand = tcpCore->buildDeviceCommand("07", "Q", 0, 0);
    tcpCore->sendMessageAsync(discardTipHeadCommand.toUtf8(), true);
    QString waitDiscardTipHeadCommand = tcpCore->buildDeviceCommand("07", "d", 0, 0);
    tcpCore->sendMessageAsync(waitDiscardTipHeadCommand.toUtf8(), true, "07d01");
    // 检查tips头是否成功丢弃：07q
    QString checkDiscardTipHeadCommand = tcpCore->buildDeviceCommand("07", "q", 0, 0);
    tcpCore->sendMessageAsync(checkDiscardTipHeadCommand.toUtf8(), true, "07q02");
    // 上升08
    tcpCore->sendMessageAsync(raiseTipZCommand.toUtf8(), true);
    tcpCore->sendMessageAsync(waitRaiseTipZCommand.toUtf8(), true, "08d01");

    /**
     * 回到夹持区域下移，盖盖子松夹爪，上移
     *
     *
     */
    tcpCore->sendMessageAsync(moveToGripAreaXCommand.toUtf8(), true);
    tcpCore->sendMessageAsync(waitGripAreaXCommand.toUtf8(), true, "0Ad01");
    tcpCore->sendMessageAsync(moveToGripAreaYCommand.toUtf8(), true);
    tcpCore->sendMessageAsync(waitGripAreaYCommand.toUtf8(), true, "09d01");
    QString moveToGripAreaZCommand_reduction= tcpCore->buildDeviceCommand("06", "D", gripAreaZ-24994, 8);
    tcpCore->sendMessageAsync(moveToGripAreaZCommand_reduction.toUtf8(), true);
    tcpCore->sendMessageAsync(waitGripAreaZCommand.toUtf8(), true, "06d01");

    // // 盖盖子（旋转关盖）
    // QString rotateGripToCloseCommand = tcpCore->buildDeviceCommand("05", "06", "0108", 600, 4); // 顺时针1.5圈
    // tcpCore->sendMessageAsync(rotateGripToCloseCommand.toUtf8(), false);
    // QString readGripperRotationToCloseCommand = tcpCore->buildDeviceCommand("05", "03", "0203", 1, 4); // -> 050302030001 + CRC
    // tcpCore->sendMessageAsync(readGripperRotationToCloseCommand.toUtf8(), false, "0503020001");

    closeBottleCap(); // 关盖自带归零

    // 松固定夹爪0B
    QString releaseGripCommand = tcpCore->buildDeviceCommand("0B", "06", "0105", 0, 4);
    tcpCore->sendMessageAsync(releaseGripCommand.toUtf8(), false);
    QString waitReleaseGripCommand = tcpCore->buildDeviceCommand("0B", "03", "0202", 1, 4);
    tcpCore->sendMessageAsync(waitReleaseGripCommand.toUtf8(), false, "0B03020001");

    // 上移06D000000000
    QString raiseMoveGripCommand = tcpCore->buildDeviceCommand("06", "D", 0, 8);
    tcpCore->sendMessageAsync(raiseMoveGripCommand.toUtf8(), true);
    QString waitRaiseMoveGripCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    tcpCore->sendMessageAsync(waitRaiseMoveGripCommand.toUtf8(), true, "06d01");

    // // 电爪旋转归零
    // tcpCore->sendMessageAsync(rotateGripToZeroCommand.toUtf8(), false);
    // tcpCore->sendMessageAsync(waitRotateGripToZeroCommand.toUtf8(), false, "0503020001");


    /**
     * 回到原位置下移动，释放，再上移
     *
     * 动到liquidName的xyz坐标
     */
     tcpCore->sendMessageAsync(moveToLiquidNameXCommand.toUtf8(), true);
     tcpCore->sendMessageAsync(waitLiquidNameXCommand.toUtf8(), true, "0Ad01");
     tcpCore->sendMessageAsync(moveToLiquidNameYCommand.toUtf8(), true);
     tcpCore->sendMessageAsync(waitLiquidNameYCommand.toUtf8(), true, "09d01");
     tcpCore->sendMessageAsync(moveToLiquidNameZCommand.toUtf8(), true);
     tcpCore->sendMessageAsync(waitLiquidNameZCommand.toUtf8(), true, "06d01");
     // 释放移动夹爪
     QString releaseMoveGripCommand = tcpCore->buildDeviceCommand("05", "06", "0105", 0, 4);
     tcpCore->sendMessageAsync(releaseMoveGripCommand.toUtf8(), false);
     QString waitReleaseMoveGripCommand = tcpCore->buildDeviceCommand("05", "03", "0202", 1, 4);
     tcpCore->sendMessageAsync(waitReleaseMoveGripCommand.toUtf8(), false, "0503020001");
     // 上移06D000000000
     tcpCore->sendMessageAsync(raiseMoveGripCommand.toUtf8(), true);
     tcpCore->sendMessageAsync(waitRaiseMoveGripCommand.toUtf8(), true, "06d01");


    // xyz轴恢复到零点
    resetXYZMotorsToZero();

    return true;
}


// 取固体
bool MainWindow::getSolid(const QString& solidName, double mass)
{
    Q_UNUSED(solidName);
    Q_UNUSED(mass);

    qDebug() << "嘻嘻哈哈";
    tcpCore->sendMessageAsync("AA1", true); // 打开天平打印
    tcpCore->setExpectedWeight(mass); // tcpCore设置期望重量值， tcpBalanceCore使用
    tcpCore->sendMessageAsync("AA2", true); // 去皮

    /**
     * 移动到固体盘
     * 读取solidArea的xyz坐标 x对于4号，y对于3号，z对于2号
     */
    QString solidAreaSql = "SELECT originX, originY, solidZ FROM other WHERE name = 'solidArea'";
    QSqlQuery solidAreaQuery = dbm->query(solidAreaSql);
    int solidAreaX = 0, solidAreaY = 0, solidAreaZ = 0;
    if (solidAreaQuery.next()) {
        solidAreaX = solidAreaQuery.value("originX").toInt();
        solidAreaY = solidAreaQuery.value("originY").toInt();
        solidAreaZ = solidAreaQuery.value("solidZ").toInt();
    }

    QString moveToSolidAreaXCommand = tcpCore->buildDeviceCommand("04", "D", solidAreaX, 8);
    tcpCore->sendMessageAsync(moveToSolidAreaXCommand.toUtf8(), true);
    QString waitSolidAreaXCommand = tcpCore->buildDeviceCommand("04", "d", 0, 0);
    tcpCore->sendMessageAsync(waitSolidAreaXCommand.toUtf8(), true, "04d01");

    QString moveToSolidAreaYCommand = tcpCore->buildDeviceCommand("03", "D", solidAreaY, 8);
    tcpCore->sendMessageAsync(moveToSolidAreaYCommand.toUtf8(), true);
    QString waitSolidAreaYCommand = tcpCore->buildDeviceCommand("03", "d", 0, 0);
    tcpCore->sendMessageAsync(waitSolidAreaYCommand.toUtf8(), true, "03d01");

    QString moveToSolidAreaZCommand = tcpCore->buildDeviceCommand("02", "D", solidAreaZ, 8);
    tcpCore->sendMessageAsync(moveToSolidAreaZCommand.toUtf8(), true);
    QString waitSolidAreaZCommand = tcpCore->buildDeviceCommand("02", "d", 0, 0);
    tcpCore->sendMessageAsync(waitSolidAreaZCommand.toUtf8(), true, "02d01");


    /**
     * 吸住
     *
     * 0D 05 00 00 FF 00 <CRC>  电磁铁打开
     */
    QString openElectromagnetCommand = tcpCore->buildDeviceCommand("0D", "05", "0000FF00", 0, 0);
    tcpCore->sendMessageAsync(openElectromagnetCommand.toUtf8(), false);


    /**
     * 上移，去天平的xyz
     *
     * 数据库获取 balanceAreaForSolid的xyz
     */
    QString zeroMotorCommand = tcpCore->buildDeviceCommand("02", "D", 0, 8);
    tcpCore->sendMessageAsync(zeroMotorCommand.toUtf8(), true);
    QString waitZeroMotorCommand = tcpCore->buildDeviceCommand("02", "d", 0, 0);
    tcpCore->sendMessageAsync(waitZeroMotorCommand.toUtf8(), true, "02d01");


    // tcpCore->sendMessageAsync("AA1", true); // 打开天平打印
    QString balanceAreaSql = "SELECT originX, originY, solidZ FROM other WHERE name = 'balanceAreaForSolid'";
    QSqlQuery balanceAreaQuery = dbm->query(balanceAreaSql);
    int balanceAreaX=0, balanceAreaY=0, balanceAreasolidZ=0;
    if (balanceAreaQuery.next()) {
        balanceAreaX = balanceAreaQuery.value("originX").toInt();
        balanceAreaY = balanceAreaQuery.value("originY").toInt();
        balanceAreasolidZ = balanceAreaQuery.value("solidZ").toInt();
    } else {
        qWarning() << "未找到 balanceArea 的数据";
        return false;
    }
    QString moveToBalanceAreaXCommand = tcpCore->buildDeviceCommand("04", "D", balanceAreaX, 8);
    tcpCore->sendMessageAsync(moveToBalanceAreaXCommand.toUtf8(), true);
    QString waitBalanceAreaXCommand = tcpCore->buildDeviceCommand("04", "d", 0, 0);
    tcpCore->sendMessageAsync(waitBalanceAreaXCommand.toUtf8(), true, "04d01");
    QString moveToBalanceAreaYCommand = tcpCore->buildDeviceCommand("03", "D", balanceAreaY, 8);
    tcpCore->sendMessageAsync(moveToBalanceAreaYCommand.toUtf8(), true);
    QString waitBalanceAreaYCommand = tcpCore->buildDeviceCommand("03", "d", 0, 0);
    tcpCore->sendMessageAsync(waitBalanceAreaYCommand.toUtf8(), true, "03d01");
    QString moveToBalanceAreaZCommand = tcpCore->buildDeviceCommand("02", "D", balanceAreasolidZ, 8);
    tcpCore->sendMessageAsync(moveToBalanceAreaZCommand.toUtf8(), true);
    QString waitBalanceAreaZCommand = tcpCore->buildDeviceCommand("02", "d", 0, 0);
    tcpCore->sendMessageAsync(waitBalanceAreaZCommand.toUtf8(), true, "02d01");











    /**
     * 旋转出料
     *
     *
     */
    // 开始旋，用1号电机实现
    // 获取lineEdit_x的数字再转成8位16进制前面补0
    // 获取lineEdit_y的数字再转成4位16进制前面补0
    // 两者拼接一起组成新的16进制，再转成数字。作为K命令的值
    // 获取lineEdit_x和lineEdit_y的数字，再转成16进制，前面补0
    QString x = ui->lineEdit_x->text();
    QString y = ui->lineEdit_y->text();
    // 检查文本是否为空
    if (x.isEmpty() || y.isEmpty()) {
        qWarning() << "lineEdit_x 或 lineEdit_y 为空，无法执行 getSolid";
        return false;
    }
    // 转换成整数，然后再转为16进制字符串，补充前导零
    bool xOk, yOk;
    int xInt = x.toInt(&xOk, 10);
    int yInt = y.toInt(&yOk, 10);
    if (!xOk || !yOk) {
        qWarning() << "lineEdit_x 或 lineEdit_y 转换失败，xOk:" << xOk << "yOk:" << yOk;
        return false;
    }
    QString xHex = QString::number(xInt, 16).toUpper().rightJustified(8, '0');
    QString yHex = QString::number(yInt, 16).toUpper().rightJustified(4, '0');
    // 拼接成新的16进制字符串，再转为数字
    QString k = xHex + yHex;
    bool kOk;
    // 使用 qint64 而不是 int，因为12位十六进制字符串的值可能超过 int 范围
    qint64 kValue = k.toLongLong(&kOk, 16);
    qDebug() << "xInt:" << xInt << "yInt:" << yInt << "xHex:" << xHex << "yHex:" << yHex << "k:" << k << "kValue:" << kValue;

    if (!kOk) {
        qWarning() << "k 值转换失败";
        return false;
    }

    QString rotateGripToOpenCommand = tcpCore->buildDeviceCommand("01", "h", kValue, 12);
    tcpCore->sendMessageAsync(rotateGripToOpenCommand.toUtf8(), true);
    QString waitRotateGripToOpenCommand = tcpCore->buildDeviceCommand("01", "d", 0, 0);
    tcpCore->sendMessageAsync(waitRotateGripToOpenCommand.toUtf8(), true, "01d01");


    tcpCore->sendMessageAsync("AA0", true); // 关闭天平打印






    /**
     * 3电机回到零点
     *
     *
     */
    tcpCore->sendMessageAsync(zeroMotorCommand.toUtf8(), true);
    tcpCore->sendMessageAsync(waitZeroMotorCommand.toUtf8(), true, "02d01");


    /**
     * 回到原固体盘位置
     *
     *
     */
    tcpCore->sendMessageAsync(moveToSolidAreaXCommand.toUtf8(), true);
    tcpCore->sendMessageAsync(waitSolidAreaXCommand.toUtf8(), true, "04d01");
    tcpCore->sendMessageAsync(moveToSolidAreaYCommand.toUtf8(), true);
    tcpCore->sendMessageAsync(waitSolidAreaYCommand.toUtf8(), true, "03d01");
    tcpCore->sendMessageAsync(moveToSolidAreaZCommand.toUtf8(), true);
    tcpCore->sendMessageAsync(waitSolidAreaZCommand.toUtf8(), true, "02d01");


    /**
     * 释放
     *
     * 0D 05 00 00 00 00 <CRC>  电磁铁关闭
     */
    QString closeElectromagnetCommand = tcpCore->buildDeviceCommand("0D", "05", "00000000", 0, 0);
    tcpCore->sendMessageAsync(closeElectromagnetCommand.toUtf8(), false);



    /**
     * 3电机回到零点
     *
     *
     */
    tcpCore->sendMessageAsync(zeroMotorCommand.toUtf8(), true);
    tcpCore->sendMessageAsync(waitZeroMotorCommand.toUtf8(), true, "02d01");





    return false;
}


// 打包放摇床
void MainWindow::tightenBottle()
{
    /**
     * xyz移动到天平
     *
     * 数据库获取 balanceArea 的xyz
     * 6，9，10电机移动到balanceArea的xyz
     */

    QString balanceAreaSql = "SELECT originX, originY, gripperZ FROM other WHERE name = 'balanceArea'";
    QSqlQuery balanceAreaQuery = dbm->query(balanceAreaSql);
    int balanceAreaX=0, balanceAreaY=0, balanceAreaZ=0;
    if (balanceAreaQuery.next()) {
        balanceAreaX = balanceAreaQuery.value("originX").toInt();
        balanceAreaY = balanceAreaQuery.value("originY").toInt();
        balanceAreaZ = balanceAreaQuery.value("gripperZ").toInt();
    }
    QString moveToBalanceAreaZCommand = tcpCore->buildDeviceCommand("0A", "D", balanceAreaX, 8);
    tcpCore->sendMessageAsync(moveToBalanceAreaZCommand.toUtf8(), true);
    QString waitBalanceAreaZCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    tcpCore->sendMessageAsync(waitBalanceAreaZCommand.toUtf8(), true, "0Ad01");

    QString moveToBalanceAreaYCommand = tcpCore->buildDeviceCommand("09", "D", balanceAreaY, 8);
    tcpCore->sendMessageAsync(moveToBalanceAreaYCommand.toUtf8(), true);
    QString waitBalanceAreaYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    tcpCore->sendMessageAsync(waitBalanceAreaYCommand.toUtf8(), true, "09d01");

    QString moveToBalanceAreaXCommand = tcpCore->buildDeviceCommand("06", "D", balanceAreaZ, 8);
    tcpCore->sendMessageAsync(moveToBalanceAreaXCommand.toUtf8(), true);
    QString waitBalanceAreaXCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    tcpCore->sendMessageAsync(waitBalanceAreaXCommand.toUtf8(), true, "06d01");

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

     QString raiseTransferZCommand = tcpCore->buildDeviceCommand("06", "D", 0, 8);
     tcpCore->sendMessageAsync(raiseTransferZCommand.toUtf8(), true);
     QString waitTransferZRaisedCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
     tcpCore->sendMessageAsync(waitTransferZRaisedCommand.toUtf8(), true, "06d01");


     /**
     * 移动到固定夹爪
     *
     * 数据库获取 gripArea 的xyz
     */
    QString gripAreaSql = "SELECT originX, originY, gripperZ FROM other WHERE name = 'gripArea'";
    QSqlQuery gripAreaQuery = dbm->query(gripAreaSql);
    int gripAreaX=0, gripAreaY=0, gripAreaZ=0;
    if (gripAreaQuery.next()) {
        gripAreaX = gripAreaQuery.value("originX").toInt();
        gripAreaY = gripAreaQuery.value("originY").toInt();
        gripAreaZ = gripAreaQuery.value("gripperZ").toInt();
    }
    QString moveToGripAreaXCommand = tcpCore->buildDeviceCommand("0A", "D", gripAreaX, 8);
    tcpCore->sendMessageAsync(moveToGripAreaXCommand.toUtf8(), true);
    QString waitGripAreaXCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    tcpCore->sendMessageAsync(waitGripAreaXCommand.toUtf8(), true, "0Ad01");
    QString moveToGripAreaYCommand = tcpCore->buildDeviceCommand("09", "D", gripAreaY, 8);
    tcpCore->sendMessageAsync(moveToGripAreaYCommand.toUtf8(), true);
    QString waitGripAreaYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    tcpCore->sendMessageAsync(waitGripAreaYCommand.toUtf8(), true, "09d01");
    QString moveToGripAreaZCommand = tcpCore->buildDeviceCommand("06", "D", gripAreaZ, 8);
    tcpCore->sendMessageAsync(moveToGripAreaZCommand.toUtf8(), true);
    QString waitGripAreaZCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    tcpCore->sendMessageAsync(waitGripAreaZCommand.toUtf8(), true, "06d01");

    /**
     * 挂顶夹爪夹住瓶子
     *
     * 11号固定夹爪电机夹住瓶子
     * 5号电机松爪
     * 6号电机上移
     */
    QString enableFixedGripperCommand = tcpCore->buildDeviceCommand("0B", "06", "0105", 100, 4);
    tcpCore->sendMessageAsync(enableFixedGripperCommand.toUtf8(), false);
    QString waitFixedGripperEnableCommand = tcpCore->buildDeviceCommand("0B", "03", "0202", 1, 4);
    tcpCore->sendMessageAsync(waitFixedGripperEnableCommand.toUtf8(), false, "0B03020002");

    QString releaseGripperCommand = tcpCore->buildDeviceCommand("05", "06", "0105", 0, 4);
    tcpCore->sendMessageAsync(releaseGripperCommand.toUtf8(), false);
    QString waitGripperReleaseCommand = tcpCore->buildDeviceCommand("05", "03", "0202", 1, 4);
    tcpCore->sendMessageAsync(waitGripperReleaseCommand.toUtf8(), false, "0503020001");

    tcpCore->sendMessageAsync(raiseTransferZCommand.toUtf8(), true);
    tcpCore->sendMessageAsync(waitTransferZRaisedCommand.toUtf8(), true, "06d01");


    /**
     * 移动到帽子区域
     *
     * 数据库获取 hatArea 的xyz
     * 6，9，10电机移动到hatArea的xyz
     */
    QString hatAreaSql = "SELECT originX, originY, gripperZ FROM other WHERE name = 'hatArea'";
    QSqlQuery hatAreaQuery = dbm->query(hatAreaSql);
    int hatAreaX=0, hatAreaY=0, hatAreaZ=0;
    if (hatAreaQuery.next()) {
        hatAreaX = hatAreaQuery.value("originX").toInt();
        hatAreaY = hatAreaQuery.value("originY").toInt();
        hatAreaZ = hatAreaQuery.value("gripperZ").toInt();
    }
    QString moveToHatAreaZCommand = tcpCore->buildDeviceCommand("0A", "D", hatAreaX, 8);
    tcpCore->sendMessageAsync(moveToHatAreaZCommand.toUtf8(), true);
    QString waitHatAreaZCommand = tcpCore->buildDeviceCommand("10", "d", 0, 0);
    tcpCore->sendMessageAsync(waitHatAreaZCommand.toUtf8(), true, "0Ad01");

    QString moveToHatAreaYCommand = tcpCore->buildDeviceCommand("09", "D", hatAreaY, 8);
    tcpCore->sendMessageAsync(moveToHatAreaYCommand.toUtf8(), true);
    QString waitHatAreaYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    tcpCore->sendMessageAsync(waitHatAreaYCommand.toUtf8(), true, "09d01");

    QString moveToHatAreaXCommand = tcpCore->buildDeviceCommand("06", "D", hatAreaZ, 8);
    tcpCore->sendMessageAsync(moveToHatAreaXCommand.toUtf8(), true);
    QString waitHatAreaXCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    tcpCore->sendMessageAsync(waitHatAreaXCommand.toUtf8(), true, "06d01");


    /**
     * 夹住瓶盖
     *
     * 5号电机加紧
     * 6号电机上移
     */
    tcpCore->sendMessageAsync(enableGripperCommand.toUtf8(), false);
    tcpCore->sendMessageAsync(waitGripperEnableCommand.toUtf8(), false, "0503020002");

    tcpCore->sendMessageAsync(raiseTransferZCommand.toUtf8(), true);
    tcpCore->sendMessageAsync(waitTransferZRaisedCommand.toUtf8(), true, "06d01");


    /**
     * 移动到放盖子区域
     *
     *
     */
    tcpCore->sendMessageAsync(moveToGripAreaXCommand.toUtf8(), true);
    tcpCore->sendMessageAsync(waitGripAreaXCommand.toUtf8(), true, "0Ad01");
    tcpCore->sendMessageAsync(moveToGripAreaYCommand.toUtf8(), true);
    tcpCore->sendMessageAsync(waitGripAreaYCommand.toUtf8(), true, "09d01");
    QString moveToGripAreaZCommand_reduction= tcpCore->buildDeviceCommand("06", "D", gripAreaZ-24994, 8); // 下移动的时候，不能太多
    tcpCore->sendMessageAsync(moveToGripAreaZCommand_reduction.toUtf8(), true);
    tcpCore->sendMessageAsync(waitGripAreaZCommand.toUtf8(), true, "06d01");


    /**
     * 05号电机拧紧瓶盖
     * 0B号固定夹爪电机松开瓶子
     * 6号电机上移
     * 05号电机移动夹爪电机复位
     *
     */
    // QString tightenGripperCommand = tcpCore->buildDeviceCommand("05", "06", "0108", 600, 4);
    // tcpCore->sendMessageAsync(tightenGripperCommand.toUtf8(), false);
    // QString waitTightenGripperCommand = tcpCore->buildDeviceCommand("05", "03", "0203", 1, 4);
    // tcpCore->sendMessageAsync(waitTightenGripperCommand.toUtf8(), false, "0503020001");

    closeBottleCap();


    QString releaseFixedGripperCommand = tcpCore->buildDeviceCommand("0B", "06", "0105", 0, 4);
    tcpCore->sendMessageAsync(releaseFixedGripperCommand.toUtf8(), false);
    QString waitReleaseFixedGripperCommand = tcpCore->buildDeviceCommand("0B", "03", "0202", 1, 4);
    tcpCore->sendMessageAsync(waitReleaseFixedGripperCommand.toUtf8(), false, "0B03020001");

    tcpCore->sendMessageAsync(raiseTransferZCommand.toUtf8(), true);
    tcpCore->sendMessageAsync(waitTransferZRaisedCommand.toUtf8(), true, "06d01");

    // QString resetGripperCommand = tcpCore->buildDeviceCommand("05", "06", "0108", 0, 4);
    // tcpCore->sendMessageAsync(resetGripperCommand.toUtf8(), false);
    // QString waitResetGripperCommand = tcpCore->buildDeviceCommand("05", "03", "0203", 1, 4);
    // tcpCore->sendMessageAsync(waitResetGripperCommand.toUtf8(), false, "0503020001");


    // 关闭摇床
    tcpCore->sendMessageAsync("AAcloseShakeBed");


    // 数据库去表shakeBedArea找字段isEmpty的值为1的记录，然后取字段selfLocation的值出来待用
    QString shakeBedAreaSql = "SELECT selfLocation FROM shakeBedArea WHERE isEmpty = 1";
    QSqlQuery shakeBedAreaQuery = dbm->query(shakeBedAreaSql);
    int shakeBedAreaSelfLocation=0;
    if (shakeBedAreaQuery.next()) {
        shakeBedAreaSelfLocation = shakeBedAreaQuery.value("selfLocation").toInt();
    }   else {
        qWarning() << "未找到 isEmpty = 1 的记录";
        return;
    }
    // 数据库去表other找字段name的值为shakeBedArea的那一行，获取originX, originY， rightSpacing, bottomSpacing, cols, rows ，gripperZ
    QString otherSql = "SELECT originX, originY, rightSpacing, bottomSpacing, cols, rows, gripperZ FROM other WHERE name = 'shakeBedArea'";
    QSqlQuery otherQuery = dbm->query(otherSql);
    int otherOriginX=0, otherOriginY=0, otherRightSpacing=0, otherBottomSpacing=0, otherCols=0, otherRows=0, otherGripperZ=0;
    if (otherQuery.next()) {
        otherOriginX = otherQuery.value("originX").toInt();
        otherOriginY = otherQuery.value("originY").toInt();
        otherRightSpacing = otherQuery.value("rightSpacing").toDouble();
        otherBottomSpacing = otherQuery.value("bottomSpacing").toDouble();
        otherCols = otherQuery.value("cols").toInt();
        otherRows = otherQuery.value("rows").toInt();
        otherGripperZ = otherQuery.value("gripperZ").toInt();
    }
    else {
        qWarning() << "未找到 name = 'shakeBedArea' 的记录";
        return;
    }
    // 通过originX, originY， rightSpacing, bottomSpacing, cols, rows   和 前面获取到的selfLocation的值计算出摇床的空位坐标。
    SlotPositionConfig shakeBedAreaConfig(otherOriginX, otherOriginY, otherCols, otherRows, otherRightSpacing, otherBottomSpacing);
    QPoint targetPos = calculateSlotPosition(shakeBedAreaConfig, shakeBedAreaSelfLocation);
    int shakeBedAreaTargetX = targetPos.x();
    int shakeBedAreaTargetY = targetPos.y();


    //  0A, 09, 06电机移动到指定位置
    QString moveToShakeBedAreaXCommand = tcpCore->buildDeviceCommand("0A", "D", shakeBedAreaTargetX, 8);
    tcpCore->sendMessageAsync(moveToShakeBedAreaXCommand.toUtf8(), true);
    QString waitShakeBedAreaXCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    tcpCore->sendMessageAsync(waitShakeBedAreaXCommand.toUtf8(), true, "0Ad01");
    QString moveToShakeBedAreaYCommand = tcpCore->buildDeviceCommand("09", "D", shakeBedAreaTargetY, 8);
    tcpCore->sendMessageAsync(moveToShakeBedAreaYCommand.toUtf8(), true);
    QString waitShakeBedAreaYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    tcpCore->sendMessageAsync(waitShakeBedAreaYCommand.toUtf8(), true, "09d01");
    QString moveToShakeBedAreaZCommand = tcpCore->buildDeviceCommand("06", "D", otherGripperZ, 8);
    tcpCore->sendMessageAsync(moveToShakeBedAreaZCommand.toUtf8(), true);
    QString waitShakeBedAreaZCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    tcpCore->sendMessageAsync(waitShakeBedAreaZCommand.toUtf8(), true, "06d01");


    /**
    * 松移动夹爪， 6号电机上移零点
    *
    */
    tcpCore->sendMessageAsync(releaseGripperCommand.toUtf8(), false);
    tcpCore->sendMessageAsync(waitGripperReleaseCommand.toUtf8(), false, "0503020001");
    tcpCore->sendMessageAsync(raiseTransferZCommand.toUtf8(), true);
    tcpCore->sendMessageAsync(waitTransferZRaisedCommand.toUtf8(), true, "06d01");



    /*
     * AA0  天平打印关
     * AA1  天平打印开
     * AA2  天平去皮
     * AAcloseShakeBed 关摇床
     * AAopenShakeBed  开摇床
     * AArecordShakeBedTime     记录摇床需要的时间
     * 步骤四:使用
     tcpCore->sendMessageAsync("AA0", true); // 关闭天平打印
     */


    // 启动摇床
    tcpCore->sendMessageAsync("AAopenShakeBed");

    // 记录摇床时间信息  去执行   recordShakeBedTime(shakeBedAreaSelfLocation, 60*5);
    tcpCore->sendMessageAsync("AArecordShakeBedTime"); // 记录摇床时间信息
 


    
}




















// 记录摇床区域的时间信息
bool MainWindow::recordShakeBedTime(int selfLocation, int shakeDurationSeconds)
{
    if (!dbm) {
        qWarning() << "数据库对象未初始化";
        return false;
    }

    // 1. 更新other表的currentIndex
    if (!incrementDatabaseField("other", "currentIndex", "name = 'shakeBedArea'")) {
        qWarning() << "更新shakeBedArea的currentIndex失败";
        return false;
    }

    // 2. 获取当前时间并格式化（yyyy-MM-dd hh:mm:ss格式）
    QDateTime currentDateTime = QDateTime::currentDateTime();
    QDateTime endDateTime = currentDateTime.addSecs(shakeDurationSeconds);
    QString startTimeStr = currentDateTime.toString("yyyy-MM-dd hh:mm:ss");
    QString endTimeStr = endDateTime.toString("yyyy-MM-dd hh:mm:ss");

    // 3. 更新数据库：设置startTime、endTime和isEmpty（使用字符串格式）
    QString updateSql = QString("UPDATE shakeBedArea SET startTime = '%1', endTime = '%2', isEmpty = 0 WHERE selfLocation = %3")
        .arg(startTimeStr, endTimeStr, QString::number(selfLocation));

    QSqlQuery updateQuery = dbm->query(updateSql);
    if (updateQuery.lastError().isValid()) {
        qWarning() << "更新shakeBedArea表失败:" << updateQuery.lastError().text() << "SQL:" << updateSql;
        return false;
    } else {
        qDebug() << QString("已更新shakeBedArea表，位置%1：开始时间=%2，结束时间=%3")
            .arg(QString::number(selfLocation), startTimeStr, endTimeStr);
        return true;
    }


}

// 开盖函数（打开瓶盖）
void MainWindow::openBottleCap()
{
    /**
     * 一、准备动作
     */
    QString rotateGripToZeroCommand = tcpCore->buildDeviceCommand("05", "06", "0108", 0, 4);// 5号电机归零
    tcpCore->sendMessageAsync(rotateGripToZeroCommand.toUtf8(), false);
    QString waitrotateGripToZeroCommand = tcpCore->buildDeviceCommand("05", "03", "0203", 1, 4); // -> 050302030001 + CRC
    tcpCore->sendMessageAsync(waitrotateGripToZeroCommand.toUtf8(), false);


    /**
     * 二、变量设置
     */
    int raiseHeight = 24994;
    // int currentZ6Coordinate = 0; //当前z坐标


    /**
     * 三、【5号电机速度】、【5号电机旋转力矩】和【6号电机移动Z】设置
     */
    setMotor5Speed(27);
    setMotor6ZSpeed(35);
    setMotor5TighteningForce(100);



    /**
     * 四、上开盖
     */
    // 去数据库获取夹住区域的z坐标
    QString gripAreaSql = "SELECT gripperZ FROM other WHERE name = 'gripArea'";
    QSqlQuery gripAreaQuery = dbm->query(gripAreaSql);
    int gripAreaZ;
    if (gripAreaQuery.next()) {
        gripAreaZ = gripAreaQuery.value("gripperZ").toInt();
    } else {
        qWarning() << "未找到 gripArea 的数据";
        return;
    }
    // 上开盖
    qDebug() << "========================" << gripAreaZ << " - " << raiseHeight << " = " << gripAreaZ-raiseHeight;
    QString moveToGripUpZCommand = tcpCore->buildDeviceCommand("06", "D", gripAreaZ-raiseHeight, 8);// 6号缓慢上移到指定距离
    tcpCore->sendMessageAsync(moveToGripUpZCommand.toUtf8(), true);
    rotateMotor5ByCircles(-5.4);
    QString waitmoveToGripUpZCommand = tcpCore->buildDeviceCommand("05", "03", "0203", 1, 4); // -> 050302030001 + CRC
    tcpCore->sendMessageAsync(waitmoveToGripUpZCommand.toUtf8(), false, "0503020001");
    QString waitrotateGripLeftCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    tcpCore->sendMessageAsync(waitrotateGripLeftCommand.toUtf8(), true, "06d01");


    /**
     * 五、收尾
     */
    // 1、电机速度恢复 已知的有5号电机的旋转和6号电机的Z轴
    setMotor5Speed(100);
    setMotor6ZSpeed(1000);
    // 2、旋转归零恢复
    QString rotateInitCommand = tcpCore->buildDeviceCommand("05", "06", "0101", 1, 4);
    tcpCore->sendMessageAsync(rotateInitCommand.toUtf8(), false);
    QString waitRotateInitCommand = tcpCore->buildDeviceCommand("05", "03", "0201", 1, 4);
    tcpCore->sendMessageAsync(waitRotateInitCommand.toUtf8(), false);


}

// 关盖函数（关闭瓶盖）
void MainWindow::closeBottleCap()
{

    /**
     * 一、准备动作：5号电机旋转归零
     */
    QString rotateInitCommand = tcpCore->buildDeviceCommand("05", "06", "0101", 1, 4);
    tcpCore->sendMessageAsync(rotateInitCommand.toUtf8(), false);
    QString waitRotateInitCommand = tcpCore->buildDeviceCommand("05", "03", "0201", 1, 4);
    tcpCore->sendMessageAsync(waitRotateInitCommand.toUtf8(), false);
    // QString rotateGripToZeroCommand = tcpCore->buildDeviceCommand("05", "06", "0108", 0, 4);
    // tcpCore->sendMessageAsync(rotateGripToZeroCommand.toUtf8(), false);
    // QString waitrotateGripToZeroCommand = tcpCore->buildDeviceCommand("05", "03", "0203", 1, 4); // -> 050302030001 + CRC
    // tcpCore->sendMessageAsync(waitrotateGripToZeroCommand.toUtf8(), false);

    /**
     * 二、5号电机转速、5号旋转力矩设置、6号电机上移速度
     */
    setMotor5Speed(27);
    setMotor6ZSpeed(35);
    setMotor5TighteningForce(20);
    
    /**
     * 三、去数据库获取【夹持区】的z坐标，电机配合旋转。
     */
    QString gripAreaSql = "SELECT gripperZ FROM other WHERE name = 'gripArea'";
    QSqlQuery gripAreaQuery = dbm->query(gripAreaSql);
    int gripAreaZ;
    if (gripAreaQuery.next()) {
        gripAreaZ = gripAreaQuery.value("gripperZ").toInt();
    } else {
        qWarning() << "未找到 gripArea 的数据";
        return;
    }
    QString moveToGripUpZCommand = tcpCore->buildDeviceCommand("06", "D", gripAreaZ, 8);
    tcpCore->sendMessageAsync(moveToGripUpZCommand.toUtf8(), true); // 下移
    rotateMotor5ByCircles(5.4); // 旋转

    QString waitmoveToGripUpZCommand = tcpCore->buildDeviceCommand("05", "03", "0203", 1, 4); // 拧紧是要报错0003： 0 代表运动中， 1 代表到达位置， 3旋转过程中堵转
    tcpCore->sendMessageAsync(waitmoveToGripUpZCommand.toUtf8(), false, "0503020003");

    // 达到旋转力矩后  【停止5号旋转】 和 【停止6号移动】
    QString stop5MotorCommand = tcpCore->buildDeviceCommand("05", "06", "0102", 1, 4); // 执行紧急停止（写操作）
    tcpCore->sendMessageAsync(stop5MotorCommand.toUtf8(), false);   
    QString waitStop5MotorCommand = tcpCore->buildDeviceCommand("05", "03", "0102", 1, 4); // 查询紧急停止（读操作）
    tcpCore->sendMessageAsync(waitStop5MotorCommand.toUtf8(), false, "0503020001");
    QString moveToGripDownZCommand = tcpCore->buildDeviceCommand("06", "K", 0, 1);
    tcpCore->sendMessageAsync(moveToGripDownZCommand.toUtf8(), true, "06K");

    /**
     * 四、收尾
     */
    // 1、先释放5号夹爪
    QString initializeGripperCommand = tcpCore->buildDeviceCommand("05", "06", "0100", 1, 4);
    tcpCore->sendMessageAsync(initializeGripperCommand.toUtf8(), false);
    QString waitGripperInitializedCommand = tcpCore->buildDeviceCommand("05", "03", "0200", 1, 4);
    tcpCore->sendMessageAsync(waitGripperInitializedCommand.toUtf8(), false);
    QString openGripperCommand = tcpCore->buildDeviceCommand("05", "06", "0105", 0, 4);
    tcpCore->sendMessageAsync(openGripperCommand.toUtf8(), false, openGripperCommand);
    QString waitOpenGripperCommand = tcpCore->buildDeviceCommand("05", "03", "0202", 1, 4); // 05  03  0202  0001  25F6
    tcpCore->sendMessageAsync(waitOpenGripperCommand.toUtf8(), false, "0503020001");
    // ********下

    // 2、电机速度恢复 已知的有5号电机的旋转和6号电机的Z轴
    QString resetSpeed5Command = tcpCore->buildDeviceCommand("05", "06", "0107", 100, 4);
    tcpCore->sendMessageAsync(resetSpeed5Command.toUtf8(), false, "050601070064");
    int resetSpeed6zSpeedValue = (100 * 6400) / 60;
    QString resetSpeed6zValueString = QString::number(resetSpeed6zSpeedValue, 16).toUpper().rightJustified(8, '0')+"0A0A"; // speed5Value转换成8位16进制字符串
    QString resetSpeed6zCommand = tcpCore->buildMessageWithCrc(">06B00000000" + resetSpeed6zValueString);
    tcpCore->sendMessageAsync(resetSpeed6zCommand.toUtf8(), true, "06B");
    // 3、速度恢复
    setMotor5Speed(100);
    setMotor6ZSpeed(1000);
    // 4、旋转归零恢复
    //QString rotateInitCommand = tcpCore->buildDeviceCommand("05", "06", "0101", 1, 4);
    tcpCore->sendMessageAsync(rotateInitCommand.toUtf8(), false);
    //QString waitRotateInitCommand = tcpCore->buildDeviceCommand("05", "03", "0201", 1, 4);
    tcpCore->sendMessageAsync(waitRotateInitCommand.toUtf8(), false);

    // ********上
    // 5、恢复夹持
    QString enableGripperCommand = tcpCore->buildDeviceCommand("05", "06", "0105", 100, 4);
    tcpCore->sendMessageAsync(enableGripperCommand.toUtf8(), false);
    QString waitGripperEnableCommand = tcpCore->buildDeviceCommand("05", "03", "0202", 1, 4);
    tcpCore->sendMessageAsync(waitGripperEnableCommand.toUtf8(), false, "0503020002"); // 目前是01
    // 6、旋转力矩恢复
    setMotor5TighteningForce(60);

}


