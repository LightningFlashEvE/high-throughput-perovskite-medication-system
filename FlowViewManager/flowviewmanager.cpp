#include "flowviewmanager.h"

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QVBoxLayout>
#include <QPen>
#include <QBrush>
#include <QVariantAnimation>
#include <QSequentialAnimationGroup>
#include <QTransform>

FlowViewManager::FlowViewManager(QObject *parent)
    : QObject(parent)
{
}

FlowViewManager::~FlowViewManager()
{
    for (QSequentialAnimationGroup *animation : flowAnimations) {
        if (animation) {
            animation->stop();
            delete animation;
        }
    }
    flowAnimations.clear();
}

void FlowViewManager::init(QWidget *containerWidget)
{
    container = containerWidget;

    flowView = new QGraphicsView(container);
    flowScene = new QGraphicsScene(this);
    flowView->setScene(flowScene);
    flowView->setRenderHint(QPainter::Antialiasing);
    flowView->setRenderHint(QPainter::TextAntialiasing);
    flowView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    flowView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    flowView->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);

    flowScene->setBackgroundBrush(QBrush(Qt::transparent));

    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(flowView);

    initDefaultSteps();
    relayout();
    applyFlowStatuses();

    setStepStatus("等待中", "active");
    setCurrentStep("等待中");
}

void FlowViewManager::relayout()
{
    if (!flowScene || !container) return;
    renderFlowNodes();
    renderFlowEdges();
    applyFlowStatuses();
    flowView->viewport()->update();
}

void FlowViewManager::initDefaultSteps()
{
    flowSteps.clear();
    createFlowStep("等待中", "等待中", 1, -1);
    createFlowStep("配方解析", "配方解析", 1, 0);
    createFlowStep("拿取空瓶", "拿取空瓶", 0, 1);
    createFlowStep("空瓶开盖", "空瓶开盖", 0, 2);
    createFlowStep("空瓶放置", "空瓶放置", 0, 3);
    createFlowStep("固体瓶抓取", "固体瓶抓取", 2, 1);
    createFlowStep("固体称重", "固体称重", 2, 2);
    createFlowStep("固体瓶返回", "固体瓶返回", 2, 3);
    createFlowStep("液体瓶抓取", "液体瓶抓取", 1, 4);
    createFlowStep("TIP头更换", "TIP头更换", 1, 5);
    createFlowStep("液体瓶开盖", "液体瓶开盖", 1, 6);
    createFlowStep("移液", "移液", 1, 7);
    createFlowStep("TIP头抛弃", "TIP头抛弃", 1, 8);
    createFlowStep("溶液封盖", "溶液封盖", 1, 9);
    createFlowStep("转移至摇床", "转移至摇床", 1, 10);
    createFlowStep("转移至中转区", "转移至中转区", 1, 11);

    currentStepId = "等待中";
}

void FlowViewManager::renderFlowNodes()
{
    for (QSequentialAnimationGroup *animation : flowAnimations) {
        if (animation) {
            animation->stop();
            delete animation;
        }
    }
    flowAnimations.clear();

    flowScene->clear();
    flowNodes.clear();

    QSize frameSize = container->size();
    if (frameSize.isEmpty()) frameSize = QSize(400, 300);

    int maxRow = 0;
    int minRow = 0;
    for (const FlowStep &step : flowSteps) {
        maxRow = qMax(maxRow, step.row);
        minRow = qMin(minRow, step.row);
    }

    int availableWidth = frameSize.width() - 20;
    int availableHeight = frameSize.height() - 20;

    int minColWidth = 120;
    int minRowHeight = 50;
    int minNodeWidth = 100;
    int minNodeHeight = 40;

    int dynamicColWidth = qMax(minColWidth, availableWidth / colCount);
    int dynamicRowHeight = qMax(minRowHeight, availableHeight / (maxRow - minRow + 2));
    int dynamicNodeWidth = qMin(160, dynamicColWidth - 10);
    int dynamicNodeHeight = qMin(40, dynamicRowHeight - 10);
    dynamicNodeWidth = qMax(minNodeWidth, dynamicNodeWidth);
    dynamicNodeHeight = qMax(minNodeHeight, dynamicNodeHeight);

    int totalWidth = colCount * dynamicColWidth;
    int newMarginLeft = (availableWidth - totalWidth) / 2;

    int sceneHeight = marginTop + (maxRow - minRow + 3) * dynamicRowHeight;
    int sceneWidth = availableWidth;
    flowScene->setSceneRect(0, 0, sceneWidth, sceneHeight);

    for (const FlowStep &step : flowSteps) {
        int x = newMarginLeft + step.col * dynamicColWidth + (dynamicColWidth - dynamicNodeWidth) / 2;
        int y = marginTop + (step.row - minRow) * dynamicRowHeight;
        x = qMax(0, x);
        y = qMax(0, y);

        QGraphicsRectItem *node = flowScene->addRect(0, 0, dynamicNodeWidth, dynamicNodeHeight,
                                                     QPen(QColor(95, 139, 152)),
                                                     QBrush(QColor(11, 30, 44, 191)));
        node->setPos(x, y);
        node->setData(0, step.id);
        node->setFlag(QGraphicsItem::ItemIsSelectable);
        node->setFlag(QGraphicsItem::ItemClipsChildrenToShape, true);
        node->setAcceptHoverEvents(true);

        QGraphicsTextItem *text = new QGraphicsTextItem(step.label, node);
        text->setDefaultTextColor(QColor(232, 251, 255));
        int fontSize = qMin(10, qMax(8, dynamicNodeHeight / 4));
        text->setFont(QFont("Microsoft YaHei", fontSize, QFont::Bold));
        QRectF textRect = text->boundingRect();
        text->setPos((dynamicNodeWidth - textRect.width()) / 2,
                     (dynamicNodeHeight - textRect.height()) / 2);
        text->setZValue(1);

        flowNodes.append(node);
    }
}

void FlowViewManager::renderFlowEdges()
{
    flowEdges.clear();

    QSize frameSize = container->size();
    if (frameSize.isEmpty()) frameSize = QSize(400, 300);

    int availableWidth = frameSize.width() - 20;
    int availableHeight = frameSize.height() - 20;
    int maxRow = 0;
    int minRow = 0;
    for (const FlowStep &step : flowSteps) {
        maxRow = qMax(maxRow, step.row);
        minRow = qMin(minRow, step.row);
    }

    int minColWidth = 120;
    int minRowHeight = 50;
    int minNodeHeight = 40;

    int dynamicColWidth = qMax(minColWidth, availableWidth / colCount);
    int dynamicRowHeight = qMax(minRowHeight, availableHeight / (maxRow - minRow + 2));
    int dynamicNodeHeight = qMin(40, dynamicRowHeight - 10);
    dynamicNodeHeight = qMax(minNodeHeight, dynamicNodeHeight);

    int totalWidth = colCount * dynamicColWidth;
    int newMarginLeft = (availableWidth - totalWidth) / 2;

    for (int i = 0; i < flowSteps.size(); ++i) {
        const FlowStep &step = flowSteps[i];
        for (int j = 0; j < flowSteps.size(); ++j) {
            const FlowStep &nextStep = flowSteps[j];
            if (nextStep.col == step.col && nextStep.row == step.row + 1) {
                int x = newMarginLeft + step.col * dynamicColWidth + dynamicColWidth / 2;
                int y1 = marginTop + (step.row - minRow) * dynamicRowHeight + dynamicNodeHeight;
                int y2 = marginTop + (nextStep.row - minRow) * dynamicRowHeight;
                QGraphicsLineItem *edge = flowScene->addLine(x, y1, x, y2);
                edge->setPen(QPen(QColor(34, 211, 238), 2));
                flowEdges.append(edge);
                break;
            }
        }
    }

    // 等待中 -> 配方解析
    int x1 = newMarginLeft + 1 * dynamicColWidth + dynamicColWidth / 2;
    int y1 = marginTop + (-1 - minRow) * dynamicRowHeight + dynamicNodeHeight;
    int x2 = newMarginLeft + 1 * dynamicColWidth + dynamicColWidth / 2;
    int y2 = marginTop + (0 - minRow) * dynamicRowHeight;
    QGraphicsLineItem *edge1 = flowScene->addLine(x1, y1, x2, y2);
    edge1->setPen(QPen(QColor(34, 211, 238), 2));
    flowEdges.append(edge1);

    // 配方解析 -> 拿取空瓶 (虚线)
    x1 = newMarginLeft + 1 * dynamicColWidth + dynamicColWidth / 2;
    y1 = marginTop + (0 - minRow) * dynamicRowHeight + dynamicNodeHeight;
    x2 = newMarginLeft + 0 * dynamicColWidth + dynamicColWidth / 2;
    y2 = marginTop + (1 - minRow) * dynamicRowHeight;
    QGraphicsLineItem *edge2 = flowScene->addLine(x1, y1, x2, y2);
    QPen dashedPen(QColor(34, 211, 238), 2);
    dashedPen.setStyle(Qt::DashLine);
    edge2->setPen(dashedPen);
    flowEdges.append(edge2);

    // 配方解析 -> 固体瓶抓取 (虚线)
    x1 = newMarginLeft + 1 * dynamicColWidth + dynamicColWidth / 2;
    y1 = marginTop + (0 - minRow) * dynamicRowHeight + dynamicNodeHeight;
    x2 = newMarginLeft + 2 * dynamicColWidth + dynamicColWidth / 2;
    y2 = marginTop + (1 - minRow) * dynamicRowHeight;
    QGraphicsLineItem *edge3 = flowScene->addLine(x1, y1, x2, y2);
    edge3->setPen(dashedPen);
    flowEdges.append(edge3);

    // 空瓶放置 -> 液体瓶抓取 (虚线)
    x1 = newMarginLeft + 0 * dynamicColWidth + dynamicColWidth / 2;
    y1 = marginTop + (3 - minRow) * dynamicRowHeight + dynamicNodeHeight;
    x2 = newMarginLeft + 1 * dynamicColWidth + dynamicColWidth / 2;
    y2 = marginTop + (4 - minRow) * dynamicRowHeight;
    QGraphicsLineItem *edge4 = flowScene->addLine(x1, y1, x2, y2);
    edge4->setPen(dashedPen);
    flowEdges.append(edge4);

    // 固体瓶返回 -> 液体瓶抓取 (虚线)
    x1 = newMarginLeft + 2 * dynamicColWidth + dynamicColWidth / 2;
    y1 = marginTop + (3 - minRow) * dynamicRowHeight + dynamicNodeHeight;
    x2 = newMarginLeft + 1 * dynamicColWidth + dynamicColWidth / 2;
    y2 = marginTop + (4 - minRow) * dynamicRowHeight;
    QGraphicsLineItem *edge5 = flowScene->addLine(x1, y1, x2, y2);
    edge5->setPen(dashedPen);
    flowEdges.append(edge5);
}

void FlowViewManager::applyFlowStatuses()
{
    for (int i = 0; i < flowSteps.size(); ++i) {
        const FlowStep &step = flowSteps[i];
        QGraphicsRectItem *node = flowNodes[i];
        QString status = (step.id == currentStepId) ? "active" : step.status;
        QColor borderColor = getStatusColor(status);
        node->setPen(QPen(borderColor, 2));
        node->setBrush(QBrush(QColor(11, 30, 44, 191)));
        updateNodeAnimation(i, status);
    }
}

QColor FlowViewManager::getStatusColor(const QString &status)
{
    if (status == "waiting") return QColor(51, 74, 87);
    if (status == "active") return QColor(14, 197, 233);
    if (status == "done") return QColor(34, 197, 94);
    if (status == "error") return QColor(239, 68, 68);
    if (status == "skip") return QColor(100, 116, 139);
    if (status == "disabled") return QColor(43, 59, 69);
    return QColor(51, 74, 87);
}

void FlowViewManager::setCurrentStep(const QString &stepId)
{
    currentStepId = stepId;
    applyFlowStatuses();
}

void FlowViewManager::setStepStatus(const QString &stepId, const QString &status)
{
    for (int i = 0; i < flowSteps.size(); ++i) {
        FlowStep &step = flowSteps[i];
        if (step.id == stepId) {
            step.status = status;
            updateNodeAnimation(i, status);
            break;
        }
    }
}

void FlowViewManager::updateAllNodeAnimations()
{
    for (int i = 0; i < flowSteps.size(); ++i) {
        updateNodeAnimation(i, flowSteps[i].status);
    }
}

void FlowViewManager::createFlowStep(const QString &id, const QString &label, int col, int row)
{
    FlowStep step{ id, label, col, row, QStringLiteral("waiting") };
    flowSteps.append(step);
}

void FlowViewManager::createBreathingAnimation(QGraphicsRectItem *node, int nodeIndex)
{
    while (flowAnimations.size() <= nodeIndex) flowAnimations.append(nullptr);
    if (flowAnimations[nodeIndex]) {
        flowAnimations[nodeIndex]->stop();
        delete flowAnimations[nodeIndex];
    }
    QSequentialAnimationGroup *animationGroup = new QSequentialAnimationGroup();
    QVariantAnimation *scaleUp = new QVariantAnimation();
    scaleUp->setDuration(450);
    scaleUp->setStartValue(1.0);
    scaleUp->setEndValue(1.03);
    scaleUp->setEasingCurve(QEasingCurve::InOutQuad);
    QVariantAnimation *scaleDown = new QVariantAnimation();
    scaleDown->setDuration(450);
    scaleDown->setStartValue(1.03);
    scaleDown->setEndValue(1.0);
    scaleDown->setEasingCurve(QEasingCurve::InOutQuad);
    QObject::connect(scaleUp, &QVariantAnimation::valueChanged, [node](const QVariant &value) {
        qreal s = value.toReal();
        node->setTransform(QTransform().scale(s, s));
    });
    QObject::connect(scaleDown, &QVariantAnimation::valueChanged, [node](const QVariant &value) {
        qreal s = value.toReal();
        node->setTransform(QTransform().scale(s, s));
    });
    animationGroup->addAnimation(scaleUp);
    animationGroup->addAnimation(scaleDown);
    animationGroup->setLoopCount(-1);
    flowAnimations[nodeIndex] = animationGroup;
    animationGroup->start();
}

void FlowViewManager::updateNodeAnimation(int nodeIndex, const QString &status)
{
    if (nodeIndex < 0 || nodeIndex >= flowNodes.size()) return;
    QGraphicsRectItem *node = flowNodes[nodeIndex];

    if (nodeIndex < flowAnimations.size() && flowAnimations[nodeIndex]) {
        flowAnimations[nodeIndex]->stop();
    }

    bool shouldAnimate = false;
    QColor glowColor;
    if (status == "active") { shouldAnimate = true; glowColor = QColor(85, 51, 230, 255); }
    else if (status == "processing") { shouldAnimate = true; glowColor = QColor(34, 211, 238, 255); }
    else if (status == "warning") { shouldAnimate = true; glowColor = QColor(255, 193, 7, 255); }

    if (shouldAnimate) {
        createBreathingAnimation(node, nodeIndex);
        // 此处可以按需添加发光效果（如DropShadow），简化为边框颜色控制
        node->setPen(QPen(glowColor, 2));
    } else {
        node->setPen(QPen(getStatusColor(status), 2));
    }
}


