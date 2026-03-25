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
#include <cmath>


void MainWindow::on_pushButton_7_clicked()
{
#if 0
    if (!tcpCore) {
        qWarning() << "TCP核心对象未初始化，无法发送命令";
        return;
    }

    // 创建配方
    RecipeQueueItem newRecipe;
    newRecipe.recipeName = "测试转动";
    newRecipe.createTime = QDateTime::currentDateTime();
    newRecipe.processState = RecipeNotProcessed;

    /************** 分成3份，使用余弦平滑过渡速度控制 *******************/
    // ========== 方式1：固定三级速度（当前使用）==========
    // 第1级：最大速度500（测试用）
    // 第2级：固定100
    // 第3级：固定50
    const int maxSpeed = 500;
    const int totalSegments = 3;
    
    auto calculateSmoothSpeed = [maxSpeed](int segment) -> int {
        if (segment == 0) {
            return maxSpeed;  // 第1次：最大速度500
        } else if (segment == 1) {
            return 50;       // 第2次：固定中间速度100
        } else {
            return 50;        // 第3次：固定最小速度50
        }
    };
    
    // ========== 方式2：余弦平滑过渡（已屏蔽）==========
    // 使用余弦函数 π/2 到 π 实现速度平滑过渡
    // const int minSpeed = 50;
    // auto calculateSmoothSpeed = [maxSpeed, minSpeed, totalSegments](int segment) -> int {
    //     // 将 segment 映射到 π/2 到 π 的角度范围
    //     // segment=0 → π/2 (cos=0, 最大速度)
    //     // segment=1 → 3π/4 (cos≈-0.707, 中间速度)
    //     // segment=2 → π (cos=-1, 最小速度)
    //     double angle = M_PI / 2.0 + segment * (M_PI / 2.0) / (totalSegments - 1);
    //     double cosValue = cos(angle);
    //     
    //     // cos从0到-1，映射到速度从maxSpeed到minSpeed
    //     // speed = maxSpeed + cosValue × (maxSpeed - minSpeed)
    //     int speed = static_cast<int>(maxSpeed + cosValue * (maxSpeed - minSpeed));
    //     
    //     return speed;
    // };

    // 将称重过程分成3份，固定三级速度
    for (int segment = 0; segment < totalSegments; segment++) {
        // 根据 segment 计算速度（固定三级）
        int m_Speed = calculateSmoothSpeed(segment);

        // 改速度
        qDebug() << "速度" << m_Speed;
        int m_needHZ = m_Speed * 320 / 3;  // 或者 m_Speed * 106.6667 但保持整数运算
        QString m_needHZ16Command = QString("%1%2")
                                        .arg(m_needHZ, 8, 16, QLatin1Char('0'))
                                        .arg("0A0A")
                                        .toUpper();
        bool ok;
        qint64  m_needHZ16to10Command = m_needHZ16Command.toLongLong(&ok, 16);
        if (!ok) {
            qDebug() << "十六进制转换失败:" << m_needHZ16Command;
        }

        QString setSpeedCommand = tcpCore->buildDeviceCommand("01", "B", m_needHZ16to10Command, 20);
        newRecipe.messageQueue.enqueue(MessageQueueItem(setSpeedCommand.toUtf8(), true));

        // 转起来（使用较大的旋转距离，当重量达到时会自动停止）
        QString changeSpeedCommand = tcpCore->buildDeviceCommand("01", "D", -10000000, 8);
        newRecipe.messageQueue.enqueue(MessageQueueItem(changeSpeedCommand.toUtf8(), true));

        // 等待旋转完成或重量到达（重量到达时会通过信号自动停止）
        QString waitChangeSpeedCommand = tcpCore->buildDeviceCommand("01", "d", 0, 0);
        newRecipe.messageQueue.enqueue(MessageQueueItem(waitChangeSpeedCommand.toUtf8(), true, "01d01"));

        // 暂停，机器需要暂停再开始（重量到达时会自动跳到下一段）
        QString stopMachineCommand = tcpCore->buildDeviceCommand("01", "K", 0, 1);
        newRecipe.messageQueue.enqueue(MessageQueueItem(stopMachineCommand.toUtf8(), true, "01K"));
    }

    // 保存到数据库并执行
    saveAndExecuteRecipe(newRecipe);

    qDebug() << "测试转动配方已创建并开始执行，共" << totalSegments << "段";

#elif 0

    if (!tcpCore) {
        qWarning() << "TCP核心对象未初始化，无法发送命令";
        return;
    }

    // 改速度 - 从UI的lineEdit获取数字
    int m_Speed = 300; // 默认值
    if (ui->lineEdit) {
        bool ok = false;
        int speedValue = ui->lineEdit->text().toInt(&ok);
        if (ok && speedValue > 0) {
            m_Speed = speedValue;
        } else {
            qWarning() << "速度值无效或为空，使用默认值300";
        }
    } else {
        qWarning() << "lineEdit控件不存在，使用默认速度值300";
    }
    // 整数除法，直接舍弃小数部分
    int m_needHZ = (m_Speed * 320) / 3;
    QString m_needHZ16 = QString::number(m_needHZ, 16).toUpper().rightJustified(8, '0');
    QString m_needHZ16Command = "00000000" + m_needHZ16 + "0000";

    // 构建完整命令（1号电机速度设置，格式：>01B + 数据 + CRC）
    QString fullCommand = ">01B" + m_needHZ16Command;
    QString commandWithCrc = tcpCore->buildMessageWithCrc(fullCommand);
    tcpCore->sendMessageAsync(commandWithCrc.toUtf8(), true, "01B");
    qDebug() << "已发送速度设置命令:" << commandWithCrc;

    // 1号电机前进1000
    int distance = 10000000;
    QString moveCommand = tcpCore->buildDeviceCommand("01", "D", distance, 8);
    tcpCore->sendMessageAsync(moveCommand.toUtf8(), true, "01D");

    // 等待到位
    QString waitCommand = tcpCore->buildDeviceCommand("01", "d", 0, 0);
    tcpCore->sendMessageAsync(waitCommand.toUtf8(), true, "01d01");

    qDebug() << "已发送1号电机前进命令，距离:" << distance;

#elif 0

    qDebug() << "\n\n++++++++++++++++++++++";
    // 打印重量表的值（静态变量使用 g_ 前缀）
    for (int i = 0; i < 3; i++) {  // 改为3个阈值（之前是5个）
        qDebug() << QString("重量阈值%1: %2  |  阈值触发%3: %4")
                    .arg(i).arg(TcpClientCore::g_weightThresholds[i], 0, 'f', 4)
                    .arg(i).arg(TcpClientCore::g_thresholdTriggered[i]);
    }
    qDebug() << "++++++++++++++++++++++\n\n";


#elif 0


    // ============ 步骤1：创建新的配方队列项 ============
    RecipeQueueItem newRecipe;

    // ============ 步骤2：设置配方信息 ============
    newRecipe.recipeName = "测试天平";                     // 使用化学方程式作为配方名称
    newRecipe.createTime = QDateTime::currentDateTime(); // 记录创建时间
    newRecipe.processState = RecipeNotProcessed;         // 配方初始为“未处理”

    // getSolid_test("FAI", 0.01, newRecipe.messageQueue);

    {
        int desiredState = RecipeFinished;
        QString stateCmd = QString("AAsetRecipeProcessState:%1").arg(desiredState);
        newRecipe.messageQueue.enqueue(MessageQueueItem(stateCmd.toUtf8(), true));
    }

    // 保存到数据库并执行
    saveAndExecuteRecipe(newRecipe);

#elif 0
 
// 判断m_messageQueue是否为空，打印m_messageQueue内容个数
    if(tcpCore->m_messageQueue.isEmpty())
    {
        qDebug() << "队列已空";
    }
    else
    {
        qDebug() << "队列不为空" << "m_messageQueue.size():" << tcpCore->m_messageQueue.size();;
    }
    tcpCore->writeBalanceTareCommand(">01K0EE65", TcpClientCore::AsciiMode);

#elif 0

    // 打印期望重量
    // ============ 步骤1：创建新的配方队列项 ============
    RecipeQueueItem newRecipe;

    newRecipe.recipeName = "1212";                     // 使用化学方程式作为配方名称
    newRecipe.createTime = QDateTime::currentDateTime(); // 记录创建时间
    newRecipe.processState = RecipeNotProcessed;         // 配方初始为“未处理”


    // 拧紧瓶子放置去摇床
    tightenBottle(newRecipe.messageQueue);
#else

    qDebug() << "------------------------------------------------------------------:";
    // 遍历m_messageQueue的内容
    while (!tcpCore->m_messageQueue.isEmpty()) {
        MessageQueueItem item = tcpCore->m_messageQueue.dequeue();
        qDebug() << "消息队列内容:" << item.content;
    }


#endif
}

void MainWindow::on_pushButton_8_clicked()
{
#if 0

    // 构建命令（带CRC）
    QString moveCommand11 = tcpCore->buildDeviceCommand("01", "K", 0, 1);
    tcpCore->writeBalanceTareCommand(moveCommand11, TcpClientCore::StringMode);

    QString fCommand = tcpCore->buildDeviceCommand("01", "f", 0, 0);
    tcpCore->writeBalanceTareCommand(fCommand, TcpClientCore::StringMode);

    qDebug() << "已直接发送1号电机停止命令:" << moveCommand11;

#elif 0

    // 释放电磁铁，直接发送
    QString releaseElectromagnetCommand = tcpCore->buildDeviceCommand("0D", "05", "00000000", 0, 0);
    tcpCore->writeBalanceTareCommand(releaseElectromagnetCommand, TcpClientCore::StringMode);

#else

    // 暂停队列
    tcpCore->pauseQueue();


#endif
}


void MainWindow::on_pushButton_3_clicked()
{
    tcpCore->resumeQueue();
}




// 取空瓶 - 重载版本
bool MainWindow::takeEmptyBottle(const QString& trayName, QQueue<MessageQueueItem>& messageQueue)
{
    Q_UNUSED(trayName);

    // 1. 从 pan_init 获取空瓶区盘首坐标和网格参数
    QString transferAreaSql = "SELECT x, y, gripperZ, rightSpacing, bottomSpacing, cols, `rows` FROM pan_init WHERE name = 'emptyPosition'";
    QSqlQuery transferAreaQuery = dbm->query(transferAreaSql);

    int transferOriginX = 0, transferOriginY = 0, transferGripperZ = 0;
    int transferRightSpacing = 0, transferBottomSpacing = 0, transferCols = 1, transferRows = 1;

    if (transferAreaQuery.next()) {
        transferOriginX    = transferAreaQuery.value("x").toInt();
        transferOriginY    = transferAreaQuery.value("y").toInt();
        transferGripperZ   = transferAreaQuery.value("gripperZ").toInt();
        transferRightSpacing  = transferAreaQuery.value("rightSpacing").toInt();
        transferBottomSpacing = transferAreaQuery.value("bottomSpacing").toInt();
        transferCols       = transferAreaQuery.value("cols").toInt();
        transferRows       = transferAreaQuery.value("rows").toInt();
    } else {
        qWarning() << "未找到 pan_init 表的 emptyPosition 数据";
        return false;
    }

    // 2. 从 pan_EmptyBottlePosition 找 drug_name 不为空且 slot_index 最小的可用槽位
    QString slotSql = "SELECT slot_index FROM pan_EmptyBottlePosition WHERE drug_name != '' ORDER BY slot_index ASC LIMIT 1";
    QSqlQuery slotQuery = dbm->query(slotSql);

    int transferSlotIndex = -1;
    if (slotQuery.next()) {
        transferSlotIndex = slotQuery.value("slot_index").toInt();
    } else {
        qWarning() << "pan_EmptyBottlePosition 中没有可用槽位（drug_name 均为空）";
        return false;
    }

    // 校验槽位下标范围，超出范围就取首个位置坐标
    int maxSlot = transferCols * transferRows - 1;
    if (transferSlotIndex < 0 || transferSlotIndex > maxSlot) {
        qWarning() << "slot_index" << transferSlotIndex << "超出范围 [0," << maxSlot << "]";
        transferSlotIndex = 0;
    }

    // 3. 计算目标坐标
    SlotPositionConfig config(transferOriginX, transferOriginY, transferCols, transferRows, transferRightSpacing, transferBottomSpacing);
    QPoint targetPos = calculateSlotPosition(config, transferSlotIndex);
    int emptyBottleAreaTargetX = targetPos.x();
    int emptyBottleAreaTargetY = targetPos.y();

    // 4. 取走空瓶后将该槽位的 drug_name 清空（不修改 value 字段）
    QString updateSlotSql = QString("UPDATE pan_EmptyBottlePosition SET drug_name = '' WHERE slot_index = %1").arg(transferSlotIndex);
    QSqlQuery updateSlotQuery = dbm->query(updateSlotSql);
    if (updateSlotQuery.lastError().isValid()) {
        qWarning() << "清空 pan_EmptyBottlePosition drug_name 失败:" << updateSlotQuery.lastError().text();
    }

    // 添加命令到消息队列而不是直接发送
    QString moveToTransferXCommand = tcpCore->buildDeviceCommand("0A", "D", emptyBottleAreaTargetX, 8);
    messageQueue.enqueue(MessageQueueItem(moveToTransferXCommand.toUtf8(), true));
    QString waitTransferXCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitTransferXCommand.toUtf8(), true, "0Ad01"));
    QString moveToTransferYCommand = tcpCore->buildDeviceCommand("09", "D", emptyBottleAreaTargetY, 8);
    messageQueue.enqueue(MessageQueueItem(moveToTransferYCommand.toUtf8(), true));
    QString waitTransferYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitTransferYCommand.toUtf8(), true, "09d01"));
    QString moveToTransferZCommand = tcpCore->buildDeviceCommand("06", "D", transferGripperZ, 8);
    messageQueue.enqueue(MessageQueueItem(moveToTransferZCommand.toUtf8(), true));
    QString waitTransferZCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitTransferZCommand.toUtf8(), true, "06d01"));

    // 先释放5号夹爪，确保状态重置
    QString releaseGripperBeforeCatchCommand = tcpCore->buildDeviceCommand("05", "06", "0105", 0, 4);
    messageQueue.enqueue(MessageQueueItem(releaseGripperBeforeCatchCommand.toUtf8(), false));
    QString waitReleaseGripperBeforeCatchCommand = tcpCore->buildDeviceCommand("05", "03", "0202", 1, 4);
    messageQueue.enqueue(MessageQueueItem(waitReleaseGripperBeforeCatchCommand.toUtf8(), false, "0503020001"));

    // Catch it - 夹紧5号夹爪
    QString enableGripperCommand = tcpCore->buildDeviceCommand("05", "06", "0105", 100, 4);
    messageQueue.enqueue(MessageQueueItem(enableGripperCommand.toUtf8(), false));
    QString waitGripperEnableCommand = tcpCore->buildDeviceCommand("05", "03", "0202", 1, 4);
    messageQueue.enqueue(MessageQueueItem(waitGripperEnableCommand.toUtf8(), false, "0503020002"));

    // 更新数据库
    //messageQueue.enqueue(MessageQueueItem("AAemptyBottleAreaCurrentIndexPlusOne", true));

    // Rise
    QString raiseTransferZCommand = tcpCore->buildDeviceCommand("06", "D", 0, 8);
    messageQueue.enqueue(MessageQueueItem(raiseTransferZCommand.toUtf8(), true));
    QString waitTransferZRaisedCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitTransferZRaisedCommand.toUtf8(), true, "06d01"));

    // Open the gripper and wait for the empty bottle to come
    QString openGripCommand = tcpCore->buildDeviceCommand("0B", "06", "0105", 0, 4);
    messageQueue.enqueue(MessageQueueItem(openGripCommand.toUtf8(), false, "0B0601050000"));
    QString waitGripOpenFeedbackCommand = tcpCore->buildDeviceCommand("0B", "03", "0202", 1, 4);
    messageQueue.enqueue(MessageQueueItem(waitGripOpenFeedbackCommand.toUtf8(), false, "0B03020001"));

    // 去other表读取gripArea的x y z值
    QString gripAreaSql = "SELECT originX, originY, gripperZ FROM other WHERE name = 'gripArea'";
    QSqlQuery gripAreaQuery = dbm->query(gripAreaSql);
    int gripAreaX, gripAreaY, gripAreaZ;
    if (gripAreaQuery.next()) {
        gripAreaX = gripAreaQuery.value("originX").toInt();
        gripAreaY = gripAreaQuery.value("originY").toInt();
        gripAreaZ = gripAreaQuery.value("gripperZ").toInt();
    } else {
        qWarning() << "未找到 gripArea 的数据";
        return false;
    }
    QString moveToGripAreaXCommand = tcpCore->buildDeviceCommand("0A", "D", gripAreaX, 8);
    messageQueue.enqueue(MessageQueueItem(moveToGripAreaXCommand.toUtf8(), true));
    QString waitGripAreaXCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitGripAreaXCommand.toUtf8(), true, "0Ad01"));
    QString moveToGripAreaYCommand = tcpCore->buildDeviceCommand("09", "D", gripAreaY, 8);
    messageQueue.enqueue(MessageQueueItem(moveToGripAreaYCommand.toUtf8(), true));
    QString waitGripAreaYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitGripAreaYCommand.toUtf8(), true, "09d01"));
    QString moveToGripAreaZCommand = tcpCore->buildDeviceCommand("06", "D", gripAreaZ, 8);
    messageQueue.enqueue(MessageQueueItem(moveToGripAreaZCommand.toUtf8(), true));
    QString waitGripAreaZCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitGripAreaZCommand.toUtf8(), true, "06d01"));

    // 调整电爪的夹紧与松开的力矩
    QString setGripTorqueCommand = tcpCore->buildDeviceCommand("0B", "06", "0103", 100, 4);
    messageQueue.enqueue(MessageQueueItem(setGripTorqueCommand.toUtf8(), false));

    // 电爪夹住玻璃瓶
    QString closeGripCommand = tcpCore->buildDeviceCommand("0B", "06", "0105", 100, 4);
    messageQueue.enqueue(MessageQueueItem(closeGripCommand.toUtf8(), false, "0B0601050064"));
    QString waitGripClosedCommand = tcpCore->buildDeviceCommand("0B", "03", "0202", 1, 4);
    messageQueue.enqueue(MessageQueueItem(waitGripClosedCommand.toUtf8(), false, "0B03020002"));

    // 开盖 - 添加开盖命令到队列（这里简化处理，实际可能需要更多命令）
    // TODO: 如果 openBottleCap() 函数也需要支持队列版本，需要创建相应的重载
    openBottleCap(messageQueue);
    

    // Z轴上移一点
    QString liftZAfterRotationCommand = tcpCore->buildDeviceCommand("06", "D", 163687, 8);
    messageQueue.enqueue(MessageQueueItem(liftZAfterRotationCommand.toUtf8(), true));
    QString waitZAfterRotationCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitZAfterRotationCommand.toUtf8(), true, "06d01"));

    // 夹持区力矩/速度设置
    QString setGripAreaTorqueCommand = tcpCore->buildDeviceCommand("0B", "06", "0103", "003C");
    messageQueue.enqueue(MessageQueueItem(setGripAreaTorqueCommand.toUtf8(), false, "0B060103003C"));

    // 移动到放瓶盖区域
    QString moveToCapDropXCommand = tcpCore->buildDeviceCommand("0A", "D", 17464, 8);
    messageQueue.enqueue(MessageQueueItem(moveToCapDropXCommand.toUtf8(), true));
    QString waitCapDropXCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitCapDropXCommand.toUtf8(), true, "0Ad01"));

    QString moveToCapDropYCommand = tcpCore->buildDeviceCommand("09", "D", 35572, 8);
    messageQueue.enqueue(MessageQueueItem(moveToCapDropYCommand.toUtf8(), true));
    QString waitCapDropYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitCapDropYCommand.toUtf8(), true, "09d01"));

    QString moveToCapDropZCommand = tcpCore->buildDeviceCommand("06", "D", 257246, 8);
    messageQueue.enqueue(MessageQueueItem(moveToCapDropZCommand.toUtf8(), true));
    QString waitCapDropZCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitCapDropZCommand.toUtf8(), true, "06d01"));

    // 松爪并归位
    QString homeGripReleaseCommand = tcpCore->buildDeviceCommand("05", "06", "0105", 0, 4);
    messageQueue.enqueue(MessageQueueItem(homeGripReleaseCommand.toUtf8(), false));
    QString waitHomeGripReleaseCommand = tcpCore->buildDeviceCommand("05", "03", "0202", 1, 4);
    messageQueue.enqueue(MessageQueueItem(waitHomeGripReleaseCommand.toUtf8(), false, "0503020001"));

    // Z轴上移一点
    messageQueue.enqueue(MessageQueueItem(liftZAfterRotationCommand.toUtf8(), true));
    QString waitZeroZCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitZeroZCommand.toUtf8(), true, "06d01"));

    // 回到gripArea
    messageQueue.enqueue(MessageQueueItem(moveToGripAreaXCommand.toUtf8(), true));
    messageQueue.enqueue(MessageQueueItem(waitGripAreaXCommand.toUtf8(), true, "0Ad01"));
    messageQueue.enqueue(MessageQueueItem(moveToGripAreaYCommand.toUtf8(), true));
    messageQueue.enqueue(MessageQueueItem(waitGripAreaYCommand.toUtf8(), true, "09d01"));
    QString back2gripAreaZCommand = tcpCore->buildDeviceCommand("06", "D", 269680, 8);
    messageQueue.enqueue(MessageQueueItem(back2gripAreaZCommand.toUtf8(), true));
    QString waitback2gripAreaZCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitback2gripAreaZCommand.toUtf8(), true, "06d01"));

    // 夹住玻璃瓶，松爪
    messageQueue.enqueue(MessageQueueItem(enableGripperCommand.toUtf8(), false));
    messageQueue.enqueue(MessageQueueItem(waitGripperEnableCommand.toUtf8(), false, "0503020002"));

    QString oprnGripCommand = tcpCore->buildDeviceCommand("0B", "06", "0105", 1, 4);
    messageQueue.enqueue(MessageQueueItem(oprnGripCommand.toUtf8(), false));
    messageQueue.enqueue(MessageQueueItem(oprnGripCommand.toUtf8(), false));
    QString waitGripOpendCommand = tcpCore->buildDeviceCommand("0B", "03", "0202", 1, 4);
    messageQueue.enqueue(MessageQueueItem(waitGripOpendCommand.toUtf8(), false, "0B03020001"));

    messageQueue.enqueue(MessageQueueItem(raiseTransferZCommand.toUtf8(), true));
    messageQueue.enqueue(MessageQueueItem(waitTransferZRaisedCommand.toUtf8(), true, "06d01"));

    // 移动到天平
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
    messageQueue.enqueue(MessageQueueItem(moveToBalanceAreaXCommand.toUtf8(), true));
    QString waitBalanceAreaXCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitBalanceAreaXCommand.toUtf8(), true, "0Ad01"));
    QString moveToBalanceAreaYCommand = tcpCore->buildDeviceCommand("09", "D", balanceAreaY, 8);
    messageQueue.enqueue(MessageQueueItem(moveToBalanceAreaYCommand.toUtf8(), true));
    QString waitBalanceAreaYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitBalanceAreaYCommand.toUtf8(), true, "09d01"));
    QString moveToBalanceAreaZCommand = tcpCore->buildDeviceCommand("06", "D", balanceAreaZ, 8);
    messageQueue.enqueue(MessageQueueItem(moveToBalanceAreaZCommand.toUtf8(), true));
    QString waitBalanceAreaZCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitBalanceAreaZCommand.toUtf8(), true, "06d01"));

    // 释放，z轴回到零点
    messageQueue.enqueue(MessageQueueItem(homeGripReleaseCommand.toUtf8(), false));
    messageQueue.enqueue(MessageQueueItem(waitHomeGripReleaseCommand.toUtf8(), false, "0503020001"));
    messageQueue.enqueue(MessageQueueItem(raiseTransferZCommand.toUtf8(), true));
    messageQueue.enqueue(MessageQueueItem(waitTransferZRaisedCommand.toUtf8(), true, "06d01"));

    return true;  // 成功返回 true
}

// 取液体 - 重载版本
bool MainWindow::getLiquid(const QString& liquidName, double volumeMl, QQueue<MessageQueueItem>& messageQueue)
{
    Q_UNUSED(volumeMl);

    // 移动到liquidName的xyz坐标
    // 2.1 从 pan_init 取液体盘网格参数
    QString liquidAreaSql = "SELECT x, y, gripperZ, rightSpacing, bottomSpacing, cols, `rows` FROM pan_init WHERE name = 'liquidPositon'";
    QSqlQuery liquidAreaQuery = dbm->query(liquidAreaSql);
    int liquidAreaX = 0, liquidAreaY = 0, liquidNameZ = 0;
    int liquidAreaRightSpacing = 0, liquidAreaBottomSpacing = 0, liquidAreaCols = 1, liquidAreaRows = 1;
    if (liquidAreaQuery.next()) {
        liquidAreaX            = liquidAreaQuery.value("x").toInt();
        liquidAreaY            = liquidAreaQuery.value("y").toInt();
        liquidNameZ            = liquidAreaQuery.value("gripperZ").toInt();
        liquidAreaRightSpacing = liquidAreaQuery.value("rightSpacing").toInt();
        liquidAreaBottomSpacing= liquidAreaQuery.value("bottomSpacing").toInt();
        liquidAreaCols         = liquidAreaQuery.value("cols").toInt();
        liquidAreaRows         = liquidAreaQuery.value("rows").toInt();
    } else {
        qWarning() << "未找到 pan_init 表的 liquidPositon 数据";
        return false;
    }

    // 2.2 从 pan_LiquidPosition 查液体槽位下标
    QString slotSql = "SELECT slot_index FROM pan_LiquidPosition WHERE drug_name = '" + liquidName + "' LIMIT 1";
    QSqlQuery slotIndexQuery = dbm->query(slotSql);
    int liquidSlotIndex = -1;
    if (slotIndexQuery.next()) {
        liquidSlotIndex = slotIndexQuery.value("slot_index").toInt();
    } else {
        qWarning() << "pan_LiquidPosition 中未找到 drug_name = " << liquidName;
        return false;
    }

    SlotPositionConfig liquidNameConfig(liquidAreaX, liquidAreaY, liquidAreaCols, liquidAreaRows, liquidAreaRightSpacing, liquidAreaBottomSpacing);
    QPoint targetPos = calculateSlotPosition(liquidNameConfig, liquidSlotIndex);
    int liquidNameTargetX = targetPos.x();
    int liquidNameTargetY = targetPos.y();

    QString moveToLiquidNameXCommand = tcpCore->buildDeviceCommand("0A", "D", liquidNameTargetX, 8);
    messageQueue.enqueue(MessageQueueItem(moveToLiquidNameXCommand.toUtf8(), true));
    QString waitLiquidNameXCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitLiquidNameXCommand.toUtf8(), true, "0Ad01"));

    QString moveToLiquidNameYCommand = tcpCore->buildDeviceCommand("09", "D", liquidNameTargetY, 8);
    messageQueue.enqueue(MessageQueueItem(moveToLiquidNameYCommand.toUtf8(), true));
    QString waitLiquidNameYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitLiquidNameYCommand.toUtf8(), true, "09d01"));

    QString moveToLiquidNameZCommand = tcpCore->buildDeviceCommand("06", "D", liquidNameZ, 8);
    messageQueue.enqueue(MessageQueueItem(moveToLiquidNameZCommand.toUtf8(), true));
    QString waitLiquidNameZCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitLiquidNameZCommand.toUtf8(), true, "06d01"));

    // 夹住，上移
    QString enableGripperCommand = tcpCore->buildDeviceCommand("05", "06", "0105", 100, 4);
    messageQueue.enqueue(MessageQueueItem(enableGripperCommand.toUtf8(), false));
    QString waitGripperEnableCommand = tcpCore->buildDeviceCommand("05", "03", "0202", 1, 4);
    messageQueue.enqueue(MessageQueueItem(waitGripperEnableCommand.toUtf8(), false, "0503020002"));
    QString raiseTransferZCommand = tcpCore->buildDeviceCommand("06", "D", 0, 8);
    messageQueue.enqueue(MessageQueueItem(raiseTransferZCommand.toUtf8(), true));
    QString waitTransferZRaisedCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitTransferZRaisedCommand.toUtf8(), true, "06d01"));

    // 打开固定夹爪
    QString openGripCommand = tcpCore->buildDeviceCommand("0B", "06", "0105", 0, 4);
    messageQueue.enqueue(MessageQueueItem(openGripCommand.toUtf8(), false, "0B0601050000"));
    QString waitGripOpenFeedbackCommand = tcpCore->buildDeviceCommand("0B", "03", "0202", 1, 4);
    messageQueue.enqueue(MessageQueueItem(waitGripOpenFeedbackCommand.toUtf8(), false, "0B03020001"));

    // 去other表读取gripArea的x y z值
    QString gripAreaSql = "SELECT originX, originY, gripperZ FROM other WHERE name = 'gripArea'";
    QSqlQuery gripAreaQuery = dbm->query(gripAreaSql);
    int gripAreaX, gripAreaY, gripAreaZ;
    if (gripAreaQuery.next()) {
        gripAreaX = gripAreaQuery.value("originX").toInt();
        gripAreaY = gripAreaQuery.value("originY").toInt();
        gripAreaZ = gripAreaQuery.value("gripperZ").toInt();
    } else {
        qWarning() << "未找到 gripArea 的数据";
        return false;
    }
    QString moveToGripAreaXCommand = tcpCore->buildDeviceCommand("0A", "D", gripAreaX, 8);
    messageQueue.enqueue(MessageQueueItem(moveToGripAreaXCommand.toUtf8(), true));
    QString waitGripAreaXCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitGripAreaXCommand.toUtf8(), true, "0Ad01"));

    QString moveToGripAreaYCommand = tcpCore->buildDeviceCommand("09", "D", gripAreaY, 8);
    messageQueue.enqueue(MessageQueueItem(moveToGripAreaYCommand.toUtf8(), true));
    QString waitGripAreaYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitGripAreaYCommand.toUtf8(), true, "09d01"));

    QString moveToGripAreaZCommand = tcpCore->buildDeviceCommand("06", "D", gripAreaZ, 8);
    messageQueue.enqueue(MessageQueueItem(moveToGripAreaZCommand.toUtf8(), true));
    QString waitGripAreaZCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitGripAreaZCommand.toUtf8(), true, "06d01"));

    // 夹住玻璃瓶
    QString closeGripCommand = tcpCore->buildDeviceCommand("0B", "06", "0105", 100, 4);
    messageQueue.enqueue(MessageQueueItem(closeGripCommand.toUtf8(), false));
    QString waitGripClosedCommand = tcpCore->buildDeviceCommand("0B", "03", "0202", 1, 4);
    messageQueue.enqueue(MessageQueueItem(waitGripClosedCommand.toUtf8(), false, "0B03020002"));

    // 开盖 - TODO: 如果需要完整实现，需要将 openBottleCap() 的内容也转换为队列版本
    openBottleCap(messageQueue);

    messageQueue.enqueue(MessageQueueItem(raiseTransferZCommand.toUtf8(), true));
    messageQueue.enqueue(MessageQueueItem(waitTransferZRaisedCommand.toUtf8(), true, "06d01"));


    // 去tipsHeadUsage表顺序读取status的值为1的记录，然后取字段selfLocation的值出来待用
    // 使用 ORDER BY selfLocation ASC 确保按位置从小到大顺序读取第一个可用的
    QString tipsHeadUsageSql = "SELECT selfLocation FROM tipsHeadUsage WHERE status = 1 ORDER BY selfLocation ASC LIMIT 1";
    QSqlQuery tipsHeadUsageQuery = dbm->query(tipsHeadUsageSql);
    int tipsHeadUsageSelfLocation=0;
    if (tipsHeadUsageQuery.next()) {
        tipsHeadUsageSelfLocation = tipsHeadUsageQuery.value("selfLocation").toInt();
    }
    else {
        qWarning() << "未找到 status = 1 的记录";
        return false;
    }
    // 移动到tips头区域
    QString tipsHeadSql = "SELECT originX, originY, currentIndex, cols, `rows`, rightSpacing, bottomSpacing FROM other WHERE name = 'tipsHeadArea'";
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
    }

    SlotPositionConfig config(tipsHeadX+tipsHeadSlotIndex*2223, tipsHeadY, tipsHeadCols, tipsHeadRows, tipsHeadRightSpacing, tipsHeadBottomSpacing);
    QPoint tipstargetPos = calculateSlotPosition(config, tipsHeadUsageSelfLocation);
    int tipsHeadTargetX = tipstargetPos.x();
    int tipsHeadTargetY = tipstargetPos.y();


    // 去表里找到selfLocation为tipsHeadUsageSelfLocation的那一行，把status值加1（表示已使用）
    QString tipsHeadUsageSql_update = QString("UPDATE tipsHeadUsage SET status = status + 1 WHERE selfLocation = %1").arg(tipsHeadUsageSelfLocation);  
    QSqlQuery tipsHeadUsageQuery_update = dbm->query(tipsHeadUsageSql_update);
    if (tipsHeadUsageQuery_update.lastError().isValid()) {
        qWarning() << "更新tipsHeadUsage表失败:" << tipsHeadUsageQuery_update.lastError().text();
    } else {
        qDebug() << QString("已更新tipsHeadUsage位置 %1 的status自增1").arg(tipsHeadUsageSelfLocation);
    }
    

    QString moveToTipsHeadXCommand = tcpCore->buildDeviceCommand("0A", "D", tipsHeadTargetX, 8);
    messageQueue.enqueue(MessageQueueItem(moveToTipsHeadXCommand.toUtf8(), true));
    QString waitTipsHeadXCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitTipsHeadXCommand.toUtf8(), true, "0Ad01"));

    QString moveToTipsHeadYCommand = tcpCore->buildDeviceCommand("09", "D", tipsHeadTargetY, 8);
    messageQueue.enqueue(MessageQueueItem(moveToTipsHeadYCommand.toUtf8(), true));
    QString waitTipsHeadYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitTipsHeadYCommand.toUtf8(), true, "09d01"));

    QString moveToGetTipZCommand = tcpCore->buildDeviceCommand("08", "i", 150000, 8);
    messageQueue.enqueue(MessageQueueItem(moveToGetTipZCommand.toUtf8(), true));
    QString waitMoveToGetTipZCommand = tcpCore->buildDeviceCommand("08", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitMoveToGetTipZCommand.toUtf8(), true, "08d01"));

    
    // 通过 AAtipsHeadAreaCurrentIndexPlusOne 指令，将 tipsHeadUsageSelfLocation 传递给 TcpClientCore
    // 由 TcpClientCore 在解析后，通过 tipsHeadAreaCurrentIndexPlusOneRequested(tipsHeadUsageSelfLocation) 信号回调到界面层
    QString tipsHeadCmd = QString("AAtipsHeadAreaCurrentIndexPlusOne:%1").arg(tipsHeadUsageSelfLocation);
    messageQueue.enqueue(MessageQueueItem(tipsHeadCmd.toUtf8(), true));



    QString raiseTipZCommand = tcpCore->buildDeviceCommand("08", "D", 100, 8);
    messageQueue.enqueue(MessageQueueItem(raiseTipZCommand.toUtf8(), true));
    QString waitRaiseTipZCommand = tcpCore->buildDeviceCommand("08", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitRaiseTipZCommand.toUtf8(), true, "08d01"));

    const int volumeUl = static_cast<int>(volumeMl * 1000.0 + 0.5);

    QString gripLiquidAreaSql = "SELECT originX, originY FROM other WHERE name = 'gripLiquidArea'";
    QSqlQuery gripLiquidAreaQuery = dbm->query(gripLiquidAreaSql);
    int gripLiquidAreaX=0, gripLiquidAreaY=0;
    if (gripLiquidAreaQuery.next()) {
        gripLiquidAreaX = gripLiquidAreaQuery.value("originX").toInt();
        gripLiquidAreaY = gripLiquidAreaQuery.value("originY").toInt();
    } else {
        qWarning() << "未找到 gripLiquidArea 的数据";
        return false;
    }
    QString moveToGripLiquidAreaXCommand = tcpCore->buildDeviceCommand("0A", "D", gripLiquidAreaX, 8);
    messageQueue.enqueue(MessageQueueItem(moveToGripLiquidAreaXCommand.toUtf8(), true));
    QString waitGripLiquidAreaXCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitGripLiquidAreaXCommand.toUtf8(), true, "0Ad01"));
    QString moveToGripLiquidAreaYCommand = tcpCore->buildDeviceCommand("09", "D", gripLiquidAreaY, 8);
    messageQueue.enqueue(MessageQueueItem(moveToGripLiquidAreaYCommand.toUtf8(), true));
    QString waitGripLiquidAreaYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitGripLiquidAreaYCommand.toUtf8(), true, "09d01"));

    QString enablePressureDetectionCommand = tcpCore->buildDeviceCommand("07", "N", 1, 2);
    messageQueue.enqueue(MessageQueueItem(enablePressureDetectionCommand.toUtf8(), true, "07N"));
    QString liquidProbeComboCommand = tcpCore->buildDeviceCommand("08", "j", 2147483667827, 16);
    messageQueue.enqueue(MessageQueueItem(liquidProbeComboCommand.toUtf8(), true, "08j"));
    QString waitEnablePressureDetectionCommand = tcpCore->buildDeviceCommand("07", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitEnablePressureDetectionCommand.toUtf8(), true, "07d03"));
    QString waitLiquidProbeComboCommand = tcpCore->buildDeviceCommand("08", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitLiquidProbeComboCommand.toUtf8(), true, "08d03"));
    QString disablePressureDetectionCommand = tcpCore->buildDeviceCommand("07", "N", 0, 2);
    messageQueue.enqueue(MessageQueueItem(disablePressureDetectionCommand.toUtf8(), true));
    QString aspirateVolumeCommand = tcpCore->buildDeviceCommand("07", "n", volumeUl, 4);
    messageQueue.enqueue(MessageQueueItem(aspirateVolumeCommand.toUtf8(), true));
    QString waitAspirateVolumeCommand = tcpCore->buildDeviceCommand("07", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitAspirateVolumeCommand.toUtf8(), true, "07d01"));

    messageQueue.enqueue(MessageQueueItem(raiseTipZCommand.toUtf8(), true));
    messageQueue.enqueue(MessageQueueItem(waitRaiseTipZCommand.toUtf8(), true, "08d01"));

    QString balanceAreaSql = "SELECT originX, originY, tipsZ FROM other WHERE name = 'balanceArea'";
    QSqlQuery balanceAreaQuery = dbm->query(balanceAreaSql);
    int balanceAreaForTipsAreaX=0, balanceAreaForTipsAreaY=0,  balanceAreaForTipsAreaZ=0;
    if (balanceAreaQuery.next()) {
        balanceAreaForTipsAreaX = balanceAreaQuery.value("originX").toInt()+291;
        balanceAreaForTipsAreaY = balanceAreaQuery.value("originY").toInt()+6246;
        balanceAreaForTipsAreaZ = balanceAreaQuery.value("tipsZ").toInt();
    } else {
        qWarning() << "未找到 balanceAreaForTipsArea 的数据";
        return false;
    }
    QString moveToBalanceAreaXCommand = tcpCore->buildDeviceCommand("0A", "D", balanceAreaForTipsAreaX, 8);
    messageQueue.enqueue(MessageQueueItem(moveToBalanceAreaXCommand.toUtf8(), true));
    QString waitBalanceAreaXCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitBalanceAreaXCommand.toUtf8(), true, "0Ad01"));
    QString moveToBalanceAreaYCommand = tcpCore->buildDeviceCommand("09", "D", balanceAreaForTipsAreaY, 8);
    messageQueue.enqueue(MessageQueueItem(moveToBalanceAreaYCommand.toUtf8(), true));
    QString waitBalanceAreaYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitBalanceAreaYCommand.toUtf8(), true, "09d01"));
    QString moveToBalanceAreaZCommand = tcpCore->buildDeviceCommand("08", "D", balanceAreaForTipsAreaZ, 8);
    messageQueue.enqueue(MessageQueueItem(moveToBalanceAreaZCommand.toUtf8(), true));
    QString waitBalanceAreaZCommand = tcpCore->buildDeviceCommand("08", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitBalanceAreaZCommand.toUtf8(), true, "08d01"));

    QString dispenseVolumeCommand = tcpCore->buildDeviceCommand("07", "p", 0, 4);
    messageQueue.enqueue(MessageQueueItem(dispenseVolumeCommand.toUtf8(), true));
    QString waitDispenseVolumeCommand = tcpCore->buildDeviceCommand("07", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitDispenseVolumeCommand.toUtf8(), true, "07d01"));

    messageQueue.enqueue(MessageQueueItem(raiseTipZCommand.toUtf8(), true));
    messageQueue.enqueue(MessageQueueItem(waitRaiseTipZCommand.toUtf8(), true, "08d01"));

    QString wasteAreaSql = "SELECT originX, originY, tipsZ FROM other WHERE name = 'wasteArea'";
    QSqlQuery wasteAreaQuery = dbm->query(wasteAreaSql);
    int wasteAreaX, wasteAreaY, wastetipsZ;
    if (wasteAreaQuery.next()) {
        wasteAreaX = wasteAreaQuery.value("originX").toInt();
        wasteAreaY = wasteAreaQuery.value("originY").toInt();
        wastetipsZ = wasteAreaQuery.value("tipsZ").toInt();
    } else {
        qWarning() << "未找到 wasteArea 的数据";
        return false;
    }
    QString moveToWasteAreaXCommand = tcpCore->buildDeviceCommand("0A", "D", wasteAreaX, 8);
    messageQueue.enqueue(MessageQueueItem(moveToWasteAreaXCommand.toUtf8(), true));
    QString waitWasteAreaXCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitWasteAreaXCommand.toUtf8(), true, "0Ad01"));
    QString moveToWasteAreaYCommand = tcpCore->buildDeviceCommand("09", "D", wasteAreaY, 8);
    messageQueue.enqueue(MessageQueueItem(moveToWasteAreaYCommand.toUtf8(), true));
    QString waitWasteAreaYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitWasteAreaYCommand.toUtf8(), true, "09d01"));
    QString moveToWasteAreaZCommand = tcpCore->buildDeviceCommand("08", "D", wastetipsZ, 8);
    messageQueue.enqueue(MessageQueueItem(moveToWasteAreaZCommand.toUtf8(), true));
    QString waitWasteAreaZCommand = tcpCore->buildDeviceCommand("08", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitWasteAreaZCommand.toUtf8(), true, "08d01"));

    QString discardTipHeadCommand = tcpCore->buildDeviceCommand("07", "Q", 0, 0);
    messageQueue.enqueue(MessageQueueItem(discardTipHeadCommand.toUtf8(), true));
    QString waitDiscardTipHeadCommand = tcpCore->buildDeviceCommand("07", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitDiscardTipHeadCommand.toUtf8(), true, "07d01"));
    QString checkDiscardTipHeadCommand = tcpCore->buildDeviceCommand("07", "q", 0, 0);
    messageQueue.enqueue(MessageQueueItem(checkDiscardTipHeadCommand.toUtf8(), true, "07q02"));
    messageQueue.enqueue(MessageQueueItem(raiseTipZCommand.toUtf8(), true));
    messageQueue.enqueue(MessageQueueItem(waitRaiseTipZCommand.toUtf8(), true, "08d01"));

    messageQueue.enqueue(MessageQueueItem(moveToGripAreaXCommand.toUtf8(), true));
    messageQueue.enqueue(MessageQueueItem(waitGripAreaXCommand.toUtf8(), true, "0Ad01"));
    messageQueue.enqueue(MessageQueueItem(moveToGripAreaYCommand.toUtf8(), true));
    messageQueue.enqueue(MessageQueueItem(waitGripAreaYCommand.toUtf8(), true, "09d01"));
    QString moveToGripAreaZCommand_reduction= tcpCore->buildDeviceCommand("06", "D", gripAreaZ-24994, 8);
    messageQueue.enqueue(MessageQueueItem(moveToGripAreaZCommand_reduction.toUtf8(), true));
    messageQueue.enqueue(MessageQueueItem(waitGripAreaZCommand.toUtf8(), true, "06d01"));

    // 关盖 - TODO: 如果需要完整实现，需要将 closeBottleCap() 的内容也转换为队列版本
    closeBottleCap(messageQueue);

    QString releaseGripCommand = tcpCore->buildDeviceCommand("0B", "06", "0105", 0, 4);
    messageQueue.enqueue(MessageQueueItem(releaseGripCommand.toUtf8(), false));
    QString waitReleaseGripCommand = tcpCore->buildDeviceCommand("0B", "03", "0202", 1, 4);
    messageQueue.enqueue(MessageQueueItem(waitReleaseGripCommand.toUtf8(), false, "0B03020001"));

    QString raiseMoveGripCommand = tcpCore->buildDeviceCommand("06", "D", 0, 8);
    messageQueue.enqueue(MessageQueueItem(raiseMoveGripCommand.toUtf8(), true));
    QString waitRaiseMoveGripCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitRaiseMoveGripCommand.toUtf8(), true, "06d01"));

    messageQueue.enqueue(MessageQueueItem(moveToLiquidNameXCommand.toUtf8(), true));
    messageQueue.enqueue(MessageQueueItem(waitLiquidNameXCommand.toUtf8(), true, "0Ad01"));
    messageQueue.enqueue(MessageQueueItem(moveToLiquidNameYCommand.toUtf8(), true));
    messageQueue.enqueue(MessageQueueItem(waitLiquidNameYCommand.toUtf8(), true, "09d01"));
    messageQueue.enqueue(MessageQueueItem(moveToLiquidNameZCommand.toUtf8(), true));
    messageQueue.enqueue(MessageQueueItem(waitLiquidNameZCommand.toUtf8(), true, "06d01"));

    QString releaseMoveGripCommand = tcpCore->buildDeviceCommand("05", "06", "0105", 0, 4);
    messageQueue.enqueue(MessageQueueItem(releaseMoveGripCommand.toUtf8(), false));
    QString waitReleaseMoveGripCommand = tcpCore->buildDeviceCommand("05", "03", "0202", 1, 4);
    messageQueue.enqueue(MessageQueueItem(waitReleaseMoveGripCommand.toUtf8(), false, "0503020001"));

    messageQueue.enqueue(MessageQueueItem(raiseMoveGripCommand.toUtf8(), true));
    messageQueue.enqueue(MessageQueueItem(waitRaiseMoveGripCommand.toUtf8(), true, "06d01"));

    return true;
}

// 取固体 - 重载版本
bool MainWindow::getSolid(const QString& solidName, double mass, QQueue<MessageQueueItem>& messageQueue, int currentIndex)
{
    Q_UNUSED(currentIndex);  // 不使用函数参数的 currentIndex，改用表中的
    qDebug() << "getSolid 队列版本1: " << solidName << " " << mass << "mg";


    messageQueue.enqueue(MessageQueueItem("AA1", true)); // 打开天平打印  
    messageQueue.enqueue(MessageQueueItem("AA2", true)); // 去皮
    
    // 从 SolidMaterialArea 表中根据固体名称查找配置
    QString solidAreaSql = "SELECT originX, originY, solidZ, rightSpacing, bottomSpacing, cols, `rows`, currentIndex FROM SolidMaterialArea WHERE solidName = '" + solidName + "'";
    QSqlQuery solidAreaQuery = dbm->query(solidAreaSql);
    
    int solidAreaX = 0, solidAreaY = 0, solidAreaZ = 0;
    int rightSpacing = 0, bottomSpacing = 0, cols = 1, rows = 1;
    int solidCurrentIndex = 0;  // 使用表中的 currentIndex
    
    if (solidAreaQuery.next()) {
        solidAreaX = solidAreaQuery.value("originX").toInt();
        solidAreaY = solidAreaQuery.value("originY").toInt();
        solidAreaZ = solidAreaQuery.value("solidZ").toInt();
        rightSpacing = solidAreaQuery.value("rightSpacing").toInt();
        bottomSpacing = solidAreaQuery.value("bottomSpacing").toInt();
        cols = solidAreaQuery.value("cols").toInt();
        rows = solidAreaQuery.value("rows").toInt();
        solidCurrentIndex = solidAreaQuery.value("currentIndex").toInt();  // 从表中获取 currentIndex
        
        qDebug() << QString("找到固体 %1: originX=%2, originY=%3, solidZ=%4, currentIndex=%5")
                    .arg(solidName).arg(solidAreaX).arg(solidAreaY).arg(solidAreaZ).arg(solidCurrentIndex);
    } else {
        qWarning() << "未找到固体:" << solidName;
        return false;
    }
    
    // 使用 calculateSlotPosition 函数计算目标槽位坐标
    // 固体盘：X方向往右(xReverse=true)
    SlotPositionConfig config(solidAreaX, solidAreaY, cols, rows, rightSpacing, bottomSpacing, true);
    
    QPoint targetPos = calculateSlotPosition(config, solidCurrentIndex);  // 使用表中的 currentIndex
    int solidAreaTargetX = targetPos.x();
    int solidAreaTargetY = targetPos.y();
    
    // 使用计算后的坐标
    solidAreaX = solidAreaTargetX;
    solidAreaY = solidAreaTargetY;

    qDebug() << QString("固体盘槽位计算: currentIndex=%1, targetX=%2, targetY=%3, rightSpacing=%4, bottomSpacing=%5")
                .arg(solidCurrentIndex).arg(solidAreaTargetX).arg(solidAreaTargetY).arg(rightSpacing).arg(bottomSpacing);
    qDebug() << "solidAreaX: " << solidAreaX << "solidAreaY: " << solidAreaY;

    // 移动0A电机到100处
    QString moveToZeroACommand = tcpCore->buildDeviceCommand("0A", "D", 100, 8);
    messageQueue.enqueue(MessageQueueItem(moveToZeroACommand.toUtf8(), true));

    QString moveToSolidAreaXCommand = tcpCore->buildDeviceCommand("04", "D", solidAreaX, 8);
    messageQueue.enqueue(MessageQueueItem(moveToSolidAreaXCommand.toUtf8(), true));
    QString waitSolidAreaXCommand = tcpCore->buildDeviceCommand("04", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitSolidAreaXCommand.toUtf8(), true, "04d01"));
    QString waitZeroACommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitZeroACommand.toUtf8(), true, "0Ad01"));

    QString moveToSolidAreaYCommand = tcpCore->buildDeviceCommand("03", "D", solidAreaY, 8);
    messageQueue.enqueue(MessageQueueItem(moveToSolidAreaYCommand.toUtf8(), true));
    QString waitSolidAreaYCommand = tcpCore->buildDeviceCommand("03", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitSolidAreaYCommand.toUtf8(), true, "03d01"));


    QString moveToSolidAreaZCommand = tcpCore->buildDeviceCommand("02", "D", solidAreaZ, 8);
    messageQueue.enqueue(MessageQueueItem(moveToSolidAreaZCommand.toUtf8(), true, "02D"));
    QString waitSolidAreaZCommand = tcpCore->buildDeviceCommand("02", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitSolidAreaZCommand.toUtf8(), true, "02d01"));

    // 电磁铁打开（吸住）
    QString openElectromagnetCommand = tcpCore->buildDeviceCommand("0D", "05", "0000FF00", 0, 0);
    messageQueue.enqueue(MessageQueueItem(openElectromagnetCommand.toUtf8(), false));

    // 上移
    QString zeroMotorCommand = tcpCore->buildDeviceCommand("02", "D", 0, 8);
    messageQueue.enqueue(MessageQueueItem(zeroMotorCommand.toUtf8(), true));
    QString waitZeroMotorCommand = tcpCore->buildDeviceCommand("02", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitZeroMotorCommand.toUtf8(), true, "02d01"));

    // 移动到天平
    QString balanceAreaSql = "SELECT originX, originY, solidZ FROM other WHERE name = 'balanceAreaForSolid'";
    QSqlQuery balanceAreaQuery = dbm->query(balanceAreaSql);
    int balanceAreaX=0, balanceAreaY=0;
    if (balanceAreaQuery.next()) {
        balanceAreaX = balanceAreaQuery.value("originX").toInt();
        balanceAreaY = balanceAreaQuery.value("originY").toInt();
    } else {
        qWarning() << "未找到 balanceArea 的数据";
        return false;
    }
    QString moveToBalanceAreaXCommand = tcpCore->buildDeviceCommand("04", "D", balanceAreaX, 8);
    messageQueue.enqueue(MessageQueueItem(moveToBalanceAreaXCommand.toUtf8(), true));
    QString waitBalanceAreaXCommand = tcpCore->buildDeviceCommand("04", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitBalanceAreaXCommand.toUtf8(), true, "04d01"));
    QString moveToBalanceAreaYCommand = tcpCore->buildDeviceCommand("03", "D", balanceAreaY, 8);
    messageQueue.enqueue(MessageQueueItem(moveToBalanceAreaYCommand.toUtf8(), true));
    QString waitBalanceAreaYCommand = tcpCore->buildDeviceCommand("03", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitBalanceAreaYCommand.toUtf8(), true, "03d01"));
    // messageQueue.enqueue(MessageQueueItem("AA2", true)); // 去皮

    // 设置期望重量值（通过特殊命令队列化，避免多配方时被覆盖）
    // mass 单位是 g，需要转换为 mg（天平使用 mg）
    QString setExpectedWeightCommand = QString("AAsetExpectedWeight:%1").arg(mass);
    messageQueue.enqueue(MessageQueueItem(setExpectedWeightCommand.toUtf8(), true));



    // QString moveToBalanceAreaZCommand = tcpCore->buildDeviceCommand("02", "D", balanceAreasolidZ, 8);
    // messageQueue.enqueue(MessageQueueItem(moveToBalanceAreaZCommand.toUtf8(), true));
    // QString waitBalanceAreaZCommand = tcpCore->buildDeviceCommand("02", "d", 0, 0);
    // messageQueue.enqueue(MessageQueueItem(waitBalanceAreaZCommand.toUtf8(), true, "02d01"));

#if 0
    int str2int8 = 1000000; // 旋转的行程 1000000
    int str2int4 = 10000;   // 旋转的速度 4000
    QString xHex = QString::number(str2int8, 16).toUpper().rightJustified(8, '0');
    QString yHex = QString::number(str2int4, 16).toUpper().rightJustified(4, '0');
    QString k = xHex + yHex;
    bool kOk;
    qint64 kValue = k.toLongLong(&kOk, 16);
    if (!kOk) {
        qWarning() << "k 值转换失败";
        return false;
    }
    else
    {
        QString rotateGripToOpenCommand = tcpCore->buildDeviceCommand("01", "h", kValue, 12);
        messageQueue.enqueue(MessageQueueItem(rotateGripToOpenCommand.toUtf8(), true));
        QString waitRotateGripToOpenCommand = tcpCore->buildDeviceCommand("01", "d", 0, 0);
        messageQueue.enqueue(MessageQueueItem(waitRotateGripToOpenCommand.toUtf8(), true, "01d01"));
    }
#endif

    /************** 分成3份，使用余弦平滑过渡速度控制 *******************/
    // 根据目标重量确定最大速度（三个等级）
    int maxSpeed;
    if (mass < 0.0010)
    {
        maxSpeed = 100;  // 100
    }
    else if (mass < 0.0100)
    {
        maxSpeed = 200; // 150
    }
    else if(mass < 0.0300)
    {
        maxSpeed = 500; // 200
    }
    else
    {
        maxSpeed = 800; // 500
    }
    qDebug() << "速度最大值用：" << maxSpeed;
    
    // ========== 方式1：固定三级速度（当前使用）==========
    // 第1级：根据重量动态计算的最大速度
    // 第2级：固定100
    // 第3级：固定50
    const int totalSegments = 3;
    
    auto calculateSmoothSpeed = [maxSpeed](int segment) -> int {
        if (segment == 0) {
            return maxSpeed;  // 第1次：最大速度（根据重量动态计算）
        } else if (segment == 1) {
            return 100;       // 第2次：固定中间速度100
        } else {
            return 50;        // 第3次：固定最小速度50
        }
    };
    
    // ========== 方式2：余弦平滑过渡（已屏蔽）==========
    // 使用余弦函数 π/2 到 π 实现速度平滑过渡
    // const int minSpeed = 50;
    // auto calculateSmoothSpeed = [maxSpeed, minSpeed, totalSegments](int segment) -> int {
    //     // 将 segment 映射到 π/2 到 π 的角度范围
    //     // segment=0 → π/2 (cos=0, 最大速度)
    //     // segment=1 → 3π/4 (cos≈-0.707, 中间速度)
    //     // segment=2 → π (cos=-1, 最小速度)
    //     double angle = M_PI / 2.0 + segment * (M_PI / 2.0) / (totalSegments - 1);
    //     double cosValue = cos(angle);
    //     
    //     // cos从0到-1，映射到速度从maxSpeed到minSpeed
    //     // speed = maxSpeed + cosValue × (maxSpeed - minSpeed)
    //     int speed = static_cast<int>(maxSpeed + cosValue * (maxSpeed - minSpeed));
    //     
    //     return speed;
    // };

    qDebug() << "将称重过程分成3份，固定三级速度。" << "  速度最大值用：" << maxSpeed;
    for (int segment = 0; segment < totalSegments; segment++) {
        // 根据 segment 计算速度（固定三级）
        int m_Speed = calculateSmoothSpeed(segment);

        qDebug() << "●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●改速度：" << m_Speed;

        int m_needHZ = m_Speed * 320 / 3;  // 或者 m_Speed * 106.6667 但保持整数运算
        QString m_needHZ16Command = QString("%1%2")
                                        .arg(m_needHZ, 8, 16, QLatin1Char('0'))
                                        .arg("640A")
                                        .toUpper();
        bool ok;
        qint64  m_needHZ16to10Command = m_needHZ16Command.toLongLong(&ok, 16);
        if (!ok) {
            qDebug() << "十六进制转换失败:" << m_needHZ16Command;
        }

        QString fCommand = tcpCore->buildDeviceCommand("01", "f", 0, 0);
        messageQueue.enqueue(MessageQueueItem(fCommand.toUtf8(), true));

        QString setSpeedCommand = tcpCore->buildDeviceCommand("01", "B", m_needHZ16to10Command, 20);
        messageQueue.enqueue(MessageQueueItem(setSpeedCommand.toUtf8(), true));

        QString changeSpeedCommand = tcpCore->buildDeviceCommand("01", "D", -1000000000, 8);
        messageQueue.enqueue(MessageQueueItem(changeSpeedCommand.toUtf8(), true));

        // 在 TcpClientCore 里面有静态变量 g_isWeightPauseActiv e 控制天平暂停，
        // 这里通过发送一个特殊 AA 命令，让 TcpClientCore 将该变量设置为 true（表示进入“暂停称重触发”状态）
        // 注意：这个命令只在本地拦截，不会真正发送到下位机
        messageQueue.enqueue(MessageQueueItem("AAEnableWeightCheck", true));

        QString waitChangeSpeedCommand = tcpCore->buildDeviceCommand("01", "d", 0, 0);
        messageQueue.enqueue(MessageQueueItem(waitChangeSpeedCommand.toUtf8(), true, "01d01"));
        
        qDebug() << "=====一个循环结束=====" << segment ;

    }

    // 接用f去确认暂停
    QString fCommand = tcpCore->buildDeviceCommand("01", "f", 0, 0);
    messageQueue.enqueue(MessageQueueItem(fCommand.toUtf8(), true));

    // 回零
    messageQueue.enqueue(MessageQueueItem(zeroMotorCommand.toUtf8(), true));
    messageQueue.enqueue(MessageQueueItem(waitZeroMotorCommand.toUtf8(), true, "02d01"));



    // 回到原固体盘位置
    messageQueue.enqueue(MessageQueueItem(moveToSolidAreaXCommand.toUtf8(), true));
    messageQueue.enqueue(MessageQueueItem(waitSolidAreaXCommand.toUtf8(), true, "04d01"));
    messageQueue.enqueue(MessageQueueItem(moveToSolidAreaYCommand.toUtf8(), true));
    messageQueue.enqueue(MessageQueueItem(waitSolidAreaYCommand.toUtf8(), true, "03d01"));
    messageQueue.enqueue(MessageQueueItem(moveToSolidAreaZCommand.toUtf8(), true));
    messageQueue.enqueue(MessageQueueItem(waitSolidAreaZCommand.toUtf8(), true, "02d01"));

    // 电磁铁关闭（释放）
    QString closeElectromagnetCommand = tcpCore->buildDeviceCommand("0D", "05", "00000000", 0, 0);
    messageQueue.enqueue(MessageQueueItem(closeElectromagnetCommand.toUtf8(), false));



    // 崴脚大法
    QString moveSolidXleftCommand = tcpCore->buildDeviceCommand("04", "D", solidAreaX-100, 8);
    messageQueue.enqueue(MessageQueueItem(moveSolidXleftCommand.toUtf8(), true));
    QString moveSolidXUpCommand = tcpCore->buildDeviceCommand("03", "D", solidAreaY-150, 8);
    messageQueue.enqueue(MessageQueueItem(moveSolidXUpCommand.toUtf8(), true));
    QString waitmoveSolidXUpCommand = tcpCore->buildDeviceCommand("03", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitmoveSolidXUpCommand.toUtf8(), true, "03d01"));
    QString waitmoveSolidXleftCommand = tcpCore->buildDeviceCommand("04", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitmoveSolidXleftCommand.toUtf8(), true, "04d01"));




    // 回零
    messageQueue.enqueue(MessageQueueItem(zeroMotorCommand.toUtf8(), true));
    messageQueue.enqueue(MessageQueueItem(waitZeroMotorCommand.toUtf8(), true, "02d01"));


    // 关闭天平打印
    messageQueue.enqueue(MessageQueueItem("AA0", true));


    return true;
}

// 拧紧瓶子 - 重载版本
void MainWindow::tightenBottle(QQueue<MessageQueueItem>& messageQueue)
{
    // 移动到天平
    QString balanceAreaSql = "SELECT originX, originY, gripperZ FROM other WHERE name = 'balanceArea'";
    QSqlQuery balanceAreaQuery = dbm->query(balanceAreaSql);
    int balanceAreaX=0, balanceAreaY=0, balanceAreaZ=0;
    if (balanceAreaQuery.next()) {
        balanceAreaX = balanceAreaQuery.value("originX").toInt();
        balanceAreaY = balanceAreaQuery.value("originY").toInt();
        balanceAreaZ = balanceAreaQuery.value("gripperZ").toInt();
    }
    QString moveToBalanceAreaZCommand = tcpCore->buildDeviceCommand("0A", "D", balanceAreaX, 8);
    messageQueue.enqueue(MessageQueueItem(moveToBalanceAreaZCommand.toUtf8(), true));
    QString waitBalanceAreaZCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitBalanceAreaZCommand.toUtf8(), true, "0Ad01"));

    QString moveToBalanceAreaYCommand = tcpCore->buildDeviceCommand("09", "D", balanceAreaY, 8);
    messageQueue.enqueue(MessageQueueItem(moveToBalanceAreaYCommand.toUtf8(), true));
    QString waitBalanceAreaYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitBalanceAreaYCommand.toUtf8(), true, "09d01"));

    QString moveToBalanceAreaXCommand = tcpCore->buildDeviceCommand("06", "D", balanceAreaZ, 8);
    messageQueue.enqueue(MessageQueueItem(moveToBalanceAreaXCommand.toUtf8(), true));
    QString waitBalanceAreaXCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitBalanceAreaXCommand.toUtf8(), true, "06d01"));

    // 夹住瓶子，上移
    QString enableGripperCommand = tcpCore->buildDeviceCommand("05", "06", "0105", 100, 4);
    messageQueue.enqueue(MessageQueueItem(enableGripperCommand.toUtf8(), false));
    QString waitGripperEnableCommand = tcpCore->buildDeviceCommand("05", "03", "0202", 1, 4);
    messageQueue.enqueue(MessageQueueItem(waitGripperEnableCommand.toUtf8(), false, "0503020002"));

    QString raiseTransferZCommand = tcpCore->buildDeviceCommand("06", "D", 0, 8);
    messageQueue.enqueue(MessageQueueItem(raiseTransferZCommand.toUtf8(), true));
    QString waitTransferZRaisedCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitTransferZRaisedCommand.toUtf8(), true, "06d01"));

    // 移动到固定夹爪
    QString gripAreaSql = "SELECT originX, originY, gripperZ FROM other WHERE name = 'gripArea'";
    QSqlQuery gripAreaQuery = dbm->query(gripAreaSql);
    int gripAreaX=0, gripAreaY=0, gripAreaZ=0;
    if (gripAreaQuery.next()) {
        gripAreaX = gripAreaQuery.value("originX").toInt();
        gripAreaY = gripAreaQuery.value("originY").toInt();
        gripAreaZ = gripAreaQuery.value("gripperZ").toInt();
    }
    QString moveToGripAreaXCommand = tcpCore->buildDeviceCommand("0A", "D", gripAreaX, 8);
    messageQueue.enqueue(MessageQueueItem(moveToGripAreaXCommand.toUtf8(), true));
    QString waitGripAreaXCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitGripAreaXCommand.toUtf8(), true, "0Ad01"));
    QString moveToGripAreaYCommand = tcpCore->buildDeviceCommand("09", "D", gripAreaY, 8);
    messageQueue.enqueue(MessageQueueItem(moveToGripAreaYCommand.toUtf8(), true));
    QString waitGripAreaYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitGripAreaYCommand.toUtf8(), true, "09d01"));
    QString moveToGripAreaZCommand = tcpCore->buildDeviceCommand("06", "D", gripAreaZ, 8);
    messageQueue.enqueue(MessageQueueItem(moveToGripAreaZCommand.toUtf8(), true));
    QString waitGripAreaZCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitGripAreaZCommand.toUtf8(), true, "06d01"));

    // 固定夹爪夹住瓶子
    QString enableFixedGripperCommand = tcpCore->buildDeviceCommand("0B", "06", "0105", 100, 4);
    messageQueue.enqueue(MessageQueueItem(enableFixedGripperCommand.toUtf8(), false));
    QString waitFixedGripperEnableCommand = tcpCore->buildDeviceCommand("0B", "03", "0202", 1, 4);
    messageQueue.enqueue(MessageQueueItem(waitFixedGripperEnableCommand.toUtf8(), false, "0B03020002"));

    QString releaseGripperCommand = tcpCore->buildDeviceCommand("05", "06", "0105", 0, 4);
    messageQueue.enqueue(MessageQueueItem(releaseGripperCommand.toUtf8(), false));
    QString waitGripperReleaseCommand = tcpCore->buildDeviceCommand("05", "03", "0202", 1, 4);
    messageQueue.enqueue(MessageQueueItem(waitGripperReleaseCommand.toUtf8(), false, "0503020001"));

    messageQueue.enqueue(MessageQueueItem(raiseTransferZCommand.toUtf8(), true));
    messageQueue.enqueue(MessageQueueItem(waitTransferZRaisedCommand.toUtf8(), true, "06d01"));

    // 移动到帽子区域
    QString hatAreaSql = "SELECT originX, originY, gripperZ FROM other WHERE name = 'hatArea'";
    QSqlQuery hatAreaQuery = dbm->query(hatAreaSql);
    int hatAreaX=0, hatAreaY=0, hatAreaZ=0;
    if (hatAreaQuery.next()) {
        hatAreaX = hatAreaQuery.value("originX").toInt();
        hatAreaY = hatAreaQuery.value("originY").toInt();
        hatAreaZ = hatAreaQuery.value("gripperZ").toInt();
    }
    QString moveToHatAreaZCommand = tcpCore->buildDeviceCommand("0A", "D", hatAreaX, 8);
    messageQueue.enqueue(MessageQueueItem(moveToHatAreaZCommand.toUtf8(), true));
    QString waitHatAreaZCommand = tcpCore->buildDeviceCommand("10", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitHatAreaZCommand.toUtf8(), true, "0Ad01"));

    QString moveToHatAreaYCommand = tcpCore->buildDeviceCommand("09", "D", hatAreaY, 8);
    messageQueue.enqueue(MessageQueueItem(moveToHatAreaYCommand.toUtf8(), true));
    QString waitHatAreaYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitHatAreaYCommand.toUtf8(), true, "09d01"));

    QString moveToHatAreaXCommand = tcpCore->buildDeviceCommand("06", "D", hatAreaZ, 8);
    messageQueue.enqueue(MessageQueueItem(moveToHatAreaXCommand.toUtf8(), true));
    QString waitHatAreaXCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitHatAreaXCommand.toUtf8(), true, "06d01"));

    // 夹住瓶盖
    messageQueue.enqueue(MessageQueueItem(enableGripperCommand.toUtf8(), false));
    messageQueue.enqueue(MessageQueueItem(waitGripperEnableCommand.toUtf8(), false, "0503020002"));

    messageQueue.enqueue(MessageQueueItem(raiseTransferZCommand.toUtf8(), true));
    messageQueue.enqueue(MessageQueueItem(waitTransferZRaisedCommand.toUtf8(), true, "06d01"));




    // 移动到放盖子区域
    messageQueue.enqueue(MessageQueueItem(moveToGripAreaXCommand.toUtf8(), true));
    messageQueue.enqueue(MessageQueueItem(waitGripAreaXCommand.toUtf8(), true, "0Ad01"));
    messageQueue.enqueue(MessageQueueItem(moveToGripAreaYCommand.toUtf8(), true));
    messageQueue.enqueue(MessageQueueItem(waitGripAreaYCommand.toUtf8(), true, "09d01"));
    QString moveToGripAreaZCommand_reduction= tcpCore->buildDeviceCommand("06", "D", gripAreaZ-24994, 8);
    messageQueue.enqueue(MessageQueueItem(moveToGripAreaZCommand_reduction.toUtf8(), true));
    messageQueue.enqueue(MessageQueueItem(waitGripAreaZCommand.toUtf8(), true, "06d01"));



    closeBottleCap(messageQueue);


    // 松开固定夹爪
    QString releaseFixedGripperCommand = tcpCore->buildDeviceCommand("0B", "06", "0105", 0, 4);
    messageQueue.enqueue(MessageQueueItem(releaseFixedGripperCommand.toUtf8(), false));
    QString waitReleaseFixedGripperCommand = tcpCore->buildDeviceCommand("0B", "03", "0202", 1, 4);
    messageQueue.enqueue(MessageQueueItem(waitReleaseFixedGripperCommand.toUtf8(), false, "0B03020001"));

    messageQueue.enqueue(MessageQueueItem(raiseTransferZCommand.toUtf8(), true));
    messageQueue.enqueue(MessageQueueItem(waitTransferZRaisedCommand.toUtf8(), true, "06d01"));



    // 关闭摇床
    messageQueue.enqueue(MessageQueueItem("AAcloseShakeBed", true));



    // 找空床位
    QString shakeBedAreaSql = "SELECT selfLocation FROM shakeBedArea WHERE isEmpty = 1";
    QSqlQuery shakeBedAreaQuery = dbm->query(shakeBedAreaSql);
    int shakeBedAreaSelfLocation=0;
    if (shakeBedAreaQuery.next()) {
        shakeBedAreaSelfLocation = shakeBedAreaQuery.value("selfLocation").toInt();
    } else {
        qWarning() << "未找到 isEmpty = 1 的记录";
        return;
    }
    // 计算空床位
    QString otherSql = "SELECT originX, originY, rightSpacing, bottomSpacing, cols, `rows`, gripperZ FROM other WHERE name = 'shakeBedArea'";
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
    } else {
        qWarning() << "未找到 name = 'shakeBedArea' 的记录";
        return;
    }
    
    SlotPositionConfig shakeBedAreaConfig(otherOriginX, otherOriginY, otherCols, otherRows, otherRightSpacing, otherBottomSpacing);
    QPoint targetPos = calculateSlotPosition(shakeBedAreaConfig, shakeBedAreaSelfLocation);
    int shakeBedAreaTargetX = targetPos.x();
    int shakeBedAreaTargetY = targetPos.y();

    // 空床位自加1
    if (!dbm) {
        qWarning() << "数据库对象未初始化，无法更新shakeBedArea的isEmpty状态";
        return;
    }
    QString updateShakeBedAreaSql = QString("UPDATE shakeBedArea SET isEmpty = isEmpty + 1 WHERE selfLocation = %1")
            .arg(shakeBedAreaSelfLocation);
    QSqlQuery updateShakeBedAreaQuery = dbm->query(updateShakeBedAreaSql);
    if (updateShakeBedAreaQuery.lastError().isValid()) {
        qWarning() << "更新shakeBedArea表isEmpty失败:" << updateShakeBedAreaQuery.lastError().text()
                   << "SQL:" << updateShakeBedAreaSql;
        return;
    } else {
        qDebug() << "摇床位置立刻" << shakeBedAreaSelfLocation << "的isEmpty自加1，标记为占用";
    }


    // 移动到摇床位置
    QString moveToShakeBedAreaXCommand = tcpCore->buildDeviceCommand("0A", "D", shakeBedAreaTargetX, 8);
    messageQueue.enqueue(MessageQueueItem(moveToShakeBedAreaXCommand.toUtf8(), true));
    QString waitShakeBedAreaXCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitShakeBedAreaXCommand.toUtf8(), true, "0Ad01"));
    QString moveToShakeBedAreaYCommand = tcpCore->buildDeviceCommand("09", "D", shakeBedAreaTargetY, 8);
    messageQueue.enqueue(MessageQueueItem(moveToShakeBedAreaYCommand.toUtf8(), true));
    QString waitShakeBedAreaYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitShakeBedAreaYCommand.toUtf8(), true, "09d01"));
    QString moveToShakeBedAreaZCommand = tcpCore->buildDeviceCommand("06", "D", otherGripperZ, 8);
    messageQueue.enqueue(MessageQueueItem(moveToShakeBedAreaZCommand.toUtf8(), true));
    QString waitShakeBedAreaZCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitShakeBedAreaZCommand.toUtf8(), true, "06d01"));

    // 松移动夹爪，上移零点
    messageQueue.enqueue(MessageQueueItem(releaseGripperCommand.toUtf8(), false));
    messageQueue.enqueue(MessageQueueItem(waitGripperReleaseCommand.toUtf8(), false, "0503020001"));
    messageQueue.enqueue(MessageQueueItem(raiseTransferZCommand.toUtf8(), true));
    messageQueue.enqueue(MessageQueueItem(waitTransferZRaisedCommand.toUtf8(), true, "06d01"));

    // 启动摇床
    messageQueue.enqueue(MessageQueueItem("AAopenShakeBed", true));
    
    // 记录摇床时间信息（传递selfLocation和摇床持续时间，默认15秒），确认已经使用值自加1（此时为3）
    QString recordCmd = QString("AArecordShakeBedTime:%1:%2").arg(shakeBedAreaSelfLocation).arg(15);
    messageQueue.enqueue(MessageQueueItem(recordCmd.toUtf8(), true));
}







// 记录摇床区域的时间信息
bool MainWindow::recordShakeBedTime(int selfLocation, int shakeDurationSeconds)
{
    if (!dbm) {
        qWarning() << "数据库对象未初始化";
        return false;
    }

    // 1. 更新other表的currentIndex
    // if (!incrementDatabaseField("other", "currentIndex", "name = 'shakeBedArea'")) {
    //     qWarning() << "更新shakeBedArea的currentIndex失败";
    //     return false;
    // }

    // 2. 获取当前时间并格式化（yyyy-MM-dd hh:mm:ss格式）
    QDateTime currentDateTime = QDateTime::currentDateTime();
    QDateTime endDateTime = currentDateTime.addSecs(shakeDurationSeconds);
    QString startTimeStr = currentDateTime.toString("yyyy-MM-dd hh:mm:ss");
    QString endTimeStr = endDateTime.toString("yyyy-MM-dd hh:mm:ss");

    // 3. 更新数据库：设置startTime、endTime和isEmpty（使用字符串格式），，这里实现自加1
    QString updateSql = QString("UPDATE shakeBedArea SET startTime = '%1', endTime = '%2', isEmpty = isEmpty + 1 WHERE selfLocation = %3")
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

// 开盖（打开瓶盖）- 队列版本：将命令写入消息队列而不是立即发送
void MainWindow::openBottleCap(QQueue<MessageQueueItem>& messageQueue)
{
    // 一、准备动作：5号电机旋转归零
    QString rotateGripToZeroCommand = tcpCore->buildDeviceCommand("05", "06", "0108", 0, 4);
    messageQueue.enqueue(MessageQueueItem(rotateGripToZeroCommand.toUtf8(), false));
    QString waitrotateGripToZeroCommand = tcpCore->buildDeviceCommand("05", "03", "0203", 1, 4);
    messageQueue.enqueue(MessageQueueItem(waitrotateGripToZeroCommand.toUtf8(), false));

    // 二、变量设置
    int raiseHeight = 24994;

    // 三、【5号电机速度】、【6号电机移动Z】和【5号电机旋转力矩】设置
    // 这里仍然调用现有函数直接下发速度/力矩设置命令（通常是全局状态），
    // 如果将来需要也放入队列，可再为这些函数增加队列重载版本。
    setMotor5Speed(27, messageQueue);
    setMotor6ZSpeed(35, messageQueue);
    setMotor5TighteningForce(100, messageQueue);

    // 四、上开盖：从数据库获取夹持区Z坐标
    QString gripAreaSql = "SELECT gripperZ FROM other WHERE name = 'gripArea'";
    QSqlQuery gripAreaQuery = dbm->query(gripAreaSql);
    int gripAreaZ;
    if (gripAreaQuery.next()) {
        gripAreaZ = gripAreaQuery.value("gripperZ").toInt();
    } else {
        qWarning() << "未找到 gripArea 的数据";
        return;
    }

    // 6号电机上移到指定高度
    QString moveToGripUpZCommand = tcpCore->buildDeviceCommand("06", "D", gripAreaZ - raiseHeight, 8);
    messageQueue.enqueue(MessageQueueItem(moveToGripUpZCommand.toUtf8(), true));

    // 5号电机旋转 -5.4 圈（逆时针开盖）
    int angleValue = static_cast<int>(-5.4 * 360);
    QString rotateCommand = tcpCore->buildDeviceCommand("05", "06", "0108", angleValue, 4);
    messageQueue.enqueue(MessageQueueItem(rotateCommand.toUtf8(), false));

    // 等待5号电机旋转完成
    QString waitmoveToGripUpZCommand = tcpCore->buildDeviceCommand("05", "03", "0203", 1, 4);
    messageQueue.enqueue(MessageQueueItem(waitmoveToGripUpZCommand.toUtf8(), false, "0503020001"));

    // 等待6号电机到位
    QString waitrotateGripLeftCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitrotateGripLeftCommand.toUtf8(), true, "06d01"));

    // 五、收尾：恢复速度，并旋转归零
    setMotor5Speed(100, messageQueue);
    setMotor6ZSpeed(1000, messageQueue);

    QString rotateInitCommand = tcpCore->buildDeviceCommand("05", "06", "0101", 1, 4);
    messageQueue.enqueue(MessageQueueItem(rotateInitCommand.toUtf8(), false));
    QString waitRotateInitCommand = tcpCore->buildDeviceCommand("05", "03", "0201", 1, 4);
    messageQueue.enqueue(MessageQueueItem(waitRotateInitCommand.toUtf8(), false));
}

// 关盖函数（关闭瓶盖）
void MainWindow::closeBottleCap(QQueue<MessageQueueItem>& messageQueue)
{

    /**
     * 一、准备动作：5号电机旋转归零
     */
    QString rotateInitCommand = tcpCore->buildDeviceCommand("05", "06", "0101", 1, 4);
    messageQueue.enqueue(MessageQueueItem(rotateInitCommand.toUtf8(), false));
    QString waitRotateInitCommand = tcpCore->buildDeviceCommand("05", "03", "0201", 1, 4);
    messageQueue.enqueue(MessageQueueItem(waitRotateInitCommand.toUtf8(), false));

    /**
     * 二、5号电机转速、5号旋转力矩设置、6号电机上移速度
     */
    setMotor5Speed(27, messageQueue);
    setMotor6ZSpeed(35, messageQueue);
    setMotor5TighteningForce(20, messageQueue);
    
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
    messageQueue.enqueue(MessageQueueItem(moveToGripUpZCommand.toUtf8(), true)); // 下移
    rotateMotor5ByCircles(5.2, messageQueue); // 旋转圈数

    QString waitmoveToGripUpZCommand = tcpCore->buildDeviceCommand("05", "03", "0203", 1, 4); // 拧紧是要报错0003： 0 代表运动中， 1 代表到达位置， 3旋转过程中堵转
    messageQueue.enqueue(MessageQueueItem(waitmoveToGripUpZCommand.toUtf8(), false, "0503020003"));

    // 达到旋转力矩后  【停止5号旋转】 和 【停止6号移动】
    QString stop5MotorCommand = tcpCore->buildDeviceCommand("05", "06", "0102", 1, 4); // 执行紧急停止（写操作）
    messageQueue.enqueue(MessageQueueItem(stop5MotorCommand.toUtf8(), false));
    QString waitStop5MotorCommand = tcpCore->buildDeviceCommand("05", "03", "0102", 1, 4); // 查询紧急停止（读操作）
    messageQueue.enqueue(MessageQueueItem(waitStop5MotorCommand.toUtf8(), false, "0503020001"));
    QString moveToGripDownZCommand = tcpCore->buildDeviceCommand("06", "K", 0, 1);
    messageQueue.enqueue(MessageQueueItem(moveToGripDownZCommand.toUtf8(), true, "06K"));

    /**
     * 四、收尾
     */
    // 1、先释放5号夹爪
    QString initializeGripperCommand = tcpCore->buildDeviceCommand("05", "06", "0100", 1, 4);
    messageQueue.enqueue(MessageQueueItem(initializeGripperCommand.toUtf8(), false));
    QString waitGripperInitializedCommand = tcpCore->buildDeviceCommand("05", "03", "0200", 1, 4);
    messageQueue.enqueue(MessageQueueItem(waitGripperInitializedCommand.toUtf8(), false));
    QString openGripperCommand = tcpCore->buildDeviceCommand("05", "06", "0105", 0, 4);
    messageQueue.enqueue(MessageQueueItem(openGripperCommand.toUtf8(), false, openGripperCommand));
    QString waitOpenGripperCommand = tcpCore->buildDeviceCommand("05", "03", "0202", 1, 4); // 05  03  0202  0001  25F6
    messageQueue.enqueue(MessageQueueItem(waitOpenGripperCommand.toUtf8(), false, "0503020001"));
    // ********下

    // 2、电机速度恢复 已知的有5号电机的旋转和6号电机的Z轴
    QString resetSpeed5Command = tcpCore->buildDeviceCommand("05", "06", "0107", 100, 4);
    messageQueue.enqueue(MessageQueueItem(resetSpeed5Command.toUtf8(), false, "050601070064"));
    int resetSpeed6zSpeedValue = (100 * 6400) / 60;
    QString resetSpeed6zValueString = QString::number(resetSpeed6zSpeedValue, 16).toUpper().rightJustified(8, '0')+"0A0A"; // speed5Value转换成8位16进制字符串
    QString resetSpeed6zCommand = tcpCore->buildMessageWithCrc(">06B00000000" + resetSpeed6zValueString);
    messageQueue.enqueue(MessageQueueItem(resetSpeed6zCommand.toUtf8(), true, "06B"));
    // 3、速度恢复
    setMotor5Speed(100, messageQueue);
    setMotor6ZSpeed(1000, messageQueue);
    // 4、旋转归零恢复
    //QString rotateInitCommand = tcpCore->buildDeviceCommand("05", "06", "0101", 1, 4);
    messageQueue.enqueue(MessageQueueItem(rotateInitCommand.toUtf8(), false));
    //QString waitRotateInitCommand = tcpCore->buildDeviceCommand("05", "03", "0201", 1, 4);
    messageQueue.enqueue(MessageQueueItem(waitRotateInitCommand.toUtf8(), false));

    // ********上
    // 5、恢复夹持
    QString enableGripperCommand = tcpCore->buildDeviceCommand("05", "06", "0105", 100, 4);
    messageQueue.enqueue(MessageQueueItem(enableGripperCommand.toUtf8(), false));
    QString waitGripperEnableCommand = tcpCore->buildDeviceCommand("05", "03", "0202", 1, 4);
    messageQueue.enqueue(MessageQueueItem(waitGripperEnableCommand.toUtf8(), false, "0503020002")); // 目前是01
    // 6、旋转力矩恢复
    setMotor5TighteningForce(60, messageQueue);

}

































