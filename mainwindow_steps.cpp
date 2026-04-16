#include "mainwindow.h"
#include "tcpclientcore.h"
#include <QCheckBox>
#include <QMenu>
#include <QPushButton>

// 获取步骤对应的勾选框
QCheckBox* MainWindow::getCheckBoxForStep(const QString& stepName)
{
    if (stepName == "reset") return m_processCheckBox_reset;
    if (stepName == "xyzBackToOrigin1") return m_processCheckBox_xyzBackToOrigin1;
    if (stepName == "takeEmptyBottle") return m_processCheckBox_takeEmptyBottle;
    if (stepName == "getSolid") return m_processCheckBox_getSolid;
    if (stepName == "xyzBackToOrigin2") return m_processCheckBox_xyzBackToOrigin2;
    if (stepName == "getLiquid") return m_processCheckBox_getLiquid;
    if (stepName == "capBottleAndTransferToShaker") return m_processCheckBox_tightenBottle;
    return nullptr;
}

// 运行选中的步骤
void MainWindow::runSelectedSteps()
{
    if (!tcpCore || !dbm) {
        qWarning() << "TCP核心或数据库未初始化";
        return;
    }

    // 1. 创建新配方
    RecipeQueueItem newRecipe;
    newRecipe.recipeName = "手动选择步骤";
    newRecipe.createTime = QDateTime::currentDateTime();
    newRecipe.processState = RecipeNotProcessed;

    // 2. 清空所有状态集合（开始新的运行）
    m_selectedSteps.clear();
    m_skippedSteps.clear();
    m_completedSteps.clear();
    m_currentStep.clear();

    // 3. 按顺序检查每个步骤是否被勾选
    QStringList allSteps = {"reset", "xyzBackToOrigin1", "takeEmptyBottle", "getSolid", "xyzBackToOrigin2", "getLiquid", "capBottleAndTransferToShaker"};

    for (const QString& step : allSteps) {
        QCheckBox* checkBox = getCheckBoxForStep(step);
        if (!checkBox || !checkBox->isChecked()) {
            // 未勾选：加入跳过集合，插入跳过标记
            m_skippedSteps.insert(step);
            QString skipCmd = QString("AAskipStep:%1").arg(step);
            newRecipe.messageQueue.enqueue(MessageQueueItem(skipCmd.toUtf8(), false));
            qDebug() << "步骤未勾选，将跳过:" << step;
            continue;
        }

        // 已勾选：加入选中集合，调用对应的业务函数填充消息队列
        m_selectedSteps.insert(step);
        qDebug() << "步骤已勾选，将执行:" << step;
        if (step == "reset") {
            initializeAllDevices(newRecipe.messageQueue);
        } else if (step == "xyzBackToOrigin1") {
            resetXYZMotorsToZero(newRecipe.messageQueue, "xyzBackToOrigin1");
        } else if (step == "takeEmptyBottle") {
            takeEmptyBottle("", newRecipe.messageQueue);
        } else if (step == "getSolid") { // 使用默认参数：固体名称和质量
            getSolid("PbI2", 100.0, newRecipe.messageQueue);
        } else if (step == "xyzBackToOrigin2") {
            resetXYZMotorsToZero(newRecipe.messageQueue, "xyzBackToOrigin2");
        } else if (step == "getLiquid") { // 使用默认参数：液体名称和体积
            getLiquid("DMF", 10.0, newRecipe.messageQueue);
        } else if (step == "capBottleAndTransferToShaker") {
            capBottleAndTransferToShaker(newRecipe.messageQueue);
        }
    }

    // 3b. 立刻刷新流程状态：未勾选的步骤显示删除线，已勾选的显示灰色
    updateProcessStateDisplay("");

    // 4. 保存并执行配方
    saveAndExecuteRecipe(newRecipe, false);

    // 5. 禁用勾选框和运行按钮（执行过程中不可修改）
    updateStepCheckBoxStates();

    qDebug() << "已提交选中步骤到执行队列";
}

// 取消步骤（在执行过程中调用）
void MainWindow::cancelStep(const QString& stepName)
{
    if (!tcpCore) {
        qWarning() << "TCP核心未初始化";
        return;
    }

    // 1. 将步骤加入跳过集合
    m_skippedSteps.insert(stepName);

    // 2. 构建跳过命令并立即发送
    QString skipCmd = QString("AAskipStep:%1").arg(stepName);
    tcpCore->sendMessageAsync(skipCmd.toUtf8(), true);

    // 3. 更新UI显示（该步骤变为灰色+删除线）
    QCheckBox* checkBox = getCheckBoxForStep(stepName);
    if (checkBox) {
        checkBox->setStyleSheet("color: gray; text-decoration: line-through;");
        checkBox->setEnabled(false);
    }

    qDebug() << "已取消步骤:" << stepName;
}

// 更新步骤勾选框的可用状态
void MainWindow::updateStepCheckBoxStates()
{
    // 不再禁用任何控件，保持所有复选框始终可用
}
