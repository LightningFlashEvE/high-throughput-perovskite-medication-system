#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QTimer>
#include <QDateTime>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QGraphicsLineItem>
#include <QResizeEvent>
#include <QPainter>
#include <QPushButton>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QVariantAnimation>
#include <QSequentialAnimationGroup>
#include <QTransform>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    /*** 显示logo ***/
    QPixmap logo(":/main/pic/logo.png");
    if (!logo.isNull()) {
        // 缩小到200像素宽度，保持比例
        QPixmap scaledLogo = logo.scaledToWidth(200, Qt::SmoothTransformation);
        ui->labelLogo->setPixmap(scaledLogo);

        // 可选：设置标签大小适应缩放后的图片
        ui->labelLogo->setFixedSize(scaledLogo.size());
    }
    
    /*** 初始化时间更新定时器 ***/
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateTime);
    updateTime();
    timer->start(1000);
    
    // 初始化棋盘视图与棋子
    initChessView();
    
    // 初始化流程视图
    initFlowView();
}

MainWindow::~MainWindow()
{
    // 清理动画
    for (QSequentialAnimationGroup *animation : flowAnimations) {
        if (animation) {
            animation->stop();
            delete animation;
        }
    }
    flowAnimations.clear();
    
    delete ui;
}

void MainWindow::updateTime()
{
    QDateTime currentDateTime = QDateTime::currentDateTime();
    
    // 更新日期显示 (格式: 2025.10.1)
    QString dateStr = currentDateTime.toString("yyyy.M.d");
    ui->labelDate->setText(dateStr);
    
    // 更新时间显示 (格式: 12:33:21)
    QString timeStr = currentDateTime.toString("hh:mm:ss");
    ui->labelTime->setText(timeStr);
}

void MainWindow::initChessView()
{
    if (!ui->graphicsView) return;

    if (!chessScene) chessScene = new QGraphicsScene(this);
    ui->graphicsView->setScene(chessScene);

    // 加载棋盘
    QPixmap boardPixmap(":/main/pic/Chessboard.png");
    chessScene->clear();
    boardItem = chessScene->addPixmap(boardPixmap);
    boardItem->setZValue(0);
    chessScene->setSceneRect(boardItem->boundingRect());

    // 视图设置
    ui->graphicsView->setRenderHint(QPainter::SmoothPixmapTransform);
    ui->graphicsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->graphicsView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->graphicsView->fitInView(chessScene->sceneRect(), Qt::KeepAspectRatio);

    // 计算网格单元尺寸（以交点为基准，9列10行 -> 相邻间距按 cols-1/rows-1 计算）
    const QRectF br = boardItem->boundingRect();
    cellWidth = br.width() / (gridCols - 1);
    cellHeight = br.height() / (gridRows - 1);

    // 创建若干棋子示例（可扩展为32个）。这里先放两个演示：红帅(4,9) 与 黑将(4,0)
    chessPieces.clear();
    QPixmap piecePixmap(":/main/pic/ChessPiece.png");
    for (int i = 0; i < 2; ++i) {
        QGraphicsPixmapItem *item = chessScene->addPixmap(piecePixmap);
        item->setZValue(1);
        // 以图片中心对齐到网格交点
        item->setOffset(-piecePixmap.width() / 2.0, -piecePixmap.height() / 2.0);
        chessPieces.append(item);
    }
    if (chessPieces.size() >= 2) {
        chessPieces[0]->setPos(gridCenterToScene(4, 9));
        chessPieces[1]->setPos(gridCenterToScene(4, 0));
    }
}

void MainWindow::initFlowView()
{
    // 创建流程视图的GraphicsView
    QGraphicsView *flowView = new QGraphicsView(ui->frame_2);
    flowScene = new QGraphicsScene(this);
    flowView->setScene(flowScene);
    flowView->setRenderHint(QPainter::Antialiasing);
    flowView->setRenderHint(QPainter::TextAntialiasing);
    flowView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    flowView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    flowView->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
    flowView->setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing);
    
    // 设置透明背景
    flowScene->setBackgroundBrush(QBrush(Qt::transparent));
    
    // 创建布局
    QVBoxLayout *layout = new QVBoxLayout(ui->frame_2);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(flowView);
    
    // 初始化流程步骤
    createFlowStep("等待中", "等待中", 1, -1);  // 新增等待中节点
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
    
    // 渲染流程视图
    renderFlowNodes();
    renderFlowEdges();
    applyFlowStatuses();
    
    // 强制更新视图
    flowView->viewport()->update();
    
    // 设置初始状态
    setStepStatus("等待中", "active");  // 等待中状态为active，显示呼吸效果
    setCurrentStep("等待中");
}

void MainWindow::createFlowStep(const QString &id, const QString &label, int col, int row)
{
    FlowStep step;
    step.id = id;
    step.label = label;
    step.col = col;
    step.row = row;
    step.status = "waiting";
    flowSteps.append(step);
}

void MainWindow::createBreathingAnimation(QGraphicsRectItem *node, int nodeIndex)
{
    // 确保动画数组大小足够
    while (flowAnimations.size() <= nodeIndex) {
        flowAnimations.append(nullptr);
    }
    
    // 如果已有动画，先停止并删除
    if (flowAnimations[nodeIndex]) {
        flowAnimations[nodeIndex]->stop();
        delete flowAnimations[nodeIndex];
    }
    
    // 创建新的动画组
    QSequentialAnimationGroup *animationGroup = new QSequentialAnimationGroup();
    
    // 创建缩放动画1：从1.0到1.03
    QVariantAnimation *scaleUp = new QVariantAnimation();
    scaleUp->setDuration(450);
    scaleUp->setStartValue(1.0);
    scaleUp->setEndValue(1.03);
    scaleUp->setEasingCurve(QEasingCurve::InOutQuad);
    
    // 创建缩放动画2：从1.03回到1.0
    QVariantAnimation *scaleDown = new QVariantAnimation();
    scaleDown->setDuration(450);
    scaleDown->setStartValue(1.03);
    scaleDown->setEndValue(1.0);
    scaleDown->setEasingCurve(QEasingCurve::InOutQuad);
    
    // 连接动画信号到节点变换
    connect(scaleUp, &QVariantAnimation::valueChanged, [node](const QVariant &value) {
        qreal scale = value.toReal();
        node->setTransform(QTransform().scale(scale, scale));
    });
    
    connect(scaleDown, &QVariantAnimation::valueChanged, [node](const QVariant &value) {
        qreal scale = value.toReal();
        node->setTransform(QTransform().scale(scale, scale));
    });
    
    // 添加到动画组
    animationGroup->addAnimation(scaleUp);
    animationGroup->addAnimation(scaleDown);
    
    // 设置循环
    animationGroup->setLoopCount(-1); // 无限循环
    
    // 存储动画组
    flowAnimations[nodeIndex] = animationGroup;
    
    // 启动动画
    animationGroup->start();
}

/**
 * 根据节点状态控制动画显示
 * 
 * 这是动画系统的核心方法，根据传入的状态字符串决定节点的动画效果
 * 
 * @param nodeIndex 节点在flowNodes数组中的索引
 * @param status 节点状态字符串
 * 
 * 实现逻辑：
 * 1. 参数验证：检查nodeIndex是否有效
 * 2. 停止当前动画：如果节点已有动画，先停止
 * 3. 状态判断：根据status决定是否需要动画和发光效果
 * 4. 应用效果：创建动画和/或发光效果
 * 
 * 状态映射表：
 * - "active"     → 蓝色发光(#5533e6ff) + 呼吸动画
 * - "processing" → 青色发光(#22d3ee) + 呼吸动画
 * - "warning"    → 黄色发光(#ffc107) + 呼吸动画
 * - 其他状态     → 无动画，无发光
 * 
 * 扩展说明：
 * 如需添加新状态，只需在此方法中添加新的else if分支即可
 */
void MainWindow::updateNodeAnimation(int nodeIndex, const QString &status)
{
    // 参数验证：确保节点索引有效
    if (nodeIndex < 0 || nodeIndex >= flowNodes.size()) {
        return;
    }
    
    QGraphicsRectItem *node = flowNodes[nodeIndex];
    
    // 停止当前动画（如果存在）
    if (nodeIndex < flowAnimations.size() && flowAnimations[nodeIndex]) {
        flowAnimations[nodeIndex]->stop();
    }
    
    // 根据状态决定是否显示动画和发光效果
    bool shouldAnimate = false;
    QColor glowColor;
    
    // 状态映射：定义哪些状态需要动画效果
    if (status == "active") {
        shouldAnimate = true;
        glowColor = QColor(85, 51, 230, 255); // #5533e6ff 蓝色发光
    } else if (status == "processing") {
        shouldAnimate = true;
        glowColor = QColor(34, 211, 238, 255); // #22d3ee 青色发光
    } else if (status == "warning") {
        shouldAnimate = true;
        glowColor = QColor(255, 193, 7, 255); // #ffc107 黄色发光
    }
    // 可以在此处添加更多状态类型
    // else if (status == "error") {
    //     shouldAnimate = true;
    //     glowColor = QColor(239, 68, 68, 255); // #ef4444 红色发光
    // }
    
    if (shouldAnimate) {
        // 创建呼吸动画效果
        createBreathingAnimation(node, nodeIndex);
        
        // 添加外发光效果（模拟box-shadow）
        QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect();
        shadow->setBlurRadius(24);        // 模糊半径，对应QML中的radius
        shadow->setColor(glowColor);      // 发光颜色
        shadow->setOffset(0, 0);          // 偏移量，0表示居中发光
        node->setGraphicsEffect(shadow);
    } else {
        // 移除发光效果，恢复默认外观
        node->setGraphicsEffect(nullptr);
    }
}

/**
 * 批量更新所有节点的动画状态
 * 
 * 遍历所有流程步骤，根据每个步骤的当前状态更新对应节点的动画效果
 * 
 * 使用场景：
 * 1. 程序启动时初始化所有节点状态
 * 2. 从外部程序加载流程状态后同步动画
 * 3. 重置流程时批量更新所有节点
 * 4. 从配置文件或数据库恢复流程状态
 * 
 * 工作流程：
 * 1. 遍历flowSteps数组中的所有步骤
 * 2. 对每个步骤调用updateNodeAnimation
 * 3. 根据步骤的status属性设置对应的动画效果
 * 
 * 使用示例：
 * ```cpp
 * // 程序启动时初始化
 * mainWindow->updateAllNodeAnimations();
 * 
 * // 从外部数据恢复状态后同步
 * loadFlowStateFromDatabase();
 * mainWindow->updateAllNodeAnimations();
 * 
 * // 重置流程时
 * resetAllStepsToWaiting();
 * mainWindow->updateAllNodeAnimations();
 * ```
 * 
 * 性能说明：
 * - 会遍历所有节点，适合初始化或批量更新场景
 * - 对于单个节点状态更新，建议使用setStepStatus
 */
void MainWindow::updateAllNodeAnimations()
{
    // 遍历所有流程步骤，批量更新动画状态
    for (int i = 0; i < flowSteps.size(); ++i) {
        // 根据每个步骤的当前状态更新对应节点的动画
        updateNodeAnimation(i, flowSteps[i].status);
    }
}

void MainWindow::renderFlowNodes()
{
    // 停止所有动画
    for (QSequentialAnimationGroup *animation : flowAnimations) {
        if (animation) {
            animation->stop();
            delete animation;
        }
    }
    flowAnimations.clear();
    
    // 完全清除场景中的所有项目
    flowScene->clear();
    flowNodes.clear();
    
    // 获取frame_2的大小
    QSize frameSize = ui->frame_2->size();
    if (frameSize.isEmpty()) {
        frameSize = QSize(400, 300); // 默认大小
    }
    
    // 计算场景大小，适应框体
    int maxRow = 0;
    int minRow = 0;
    for (const FlowStep &step : flowSteps) {
        maxRow = qMax(maxRow, step.row);
        minRow = qMin(minRow, step.row);
    }
    
    // 动态计算间距，让流程图适应框体大小
    int availableWidth = frameSize.width() - 20; // 减少边距
    int availableHeight = frameSize.height() - 20;
    
    // 设置最小尺寸，确保流程图不会太小
    int minColWidth = 120;
    int minRowHeight = 50;
    int minNodeWidth = 100;
    int minNodeHeight = 40;
    
    int dynamicColWidth = qMax(minColWidth, availableWidth / colCount);
    int dynamicRowHeight = qMax(minRowHeight, availableHeight / (maxRow - minRow + 2));
    int dynamicNodeWidth = qMin(160, dynamicColWidth - 10);
    int dynamicNodeHeight = qMin(40, dynamicRowHeight - 10);
    
    // 确保节点不会太小
    dynamicNodeWidth = qMax(minNodeWidth, dynamicNodeWidth);
    dynamicNodeHeight = qMax(minNodeHeight, dynamicNodeHeight);
    
    // 重新计算左边距，让流程图居中
    int totalWidth = colCount * dynamicColWidth;
    int newMarginLeft = (availableWidth - totalWidth) / 2;
    
    int sceneHeight = marginTop + (maxRow - minRow + 3) * dynamicRowHeight;
    int sceneWidth = availableWidth;
    flowScene->setSceneRect(0, 0, sceneWidth, sceneHeight);
    
    // 创建节点
    for (const FlowStep &step : flowSteps) {
        int x = newMarginLeft + step.col * dynamicColWidth + (dynamicColWidth - dynamicNodeWidth) / 2;
        int y = marginTop + (step.row - minRow) * dynamicRowHeight;
        
        // 确保坐标在合理范围内
        x = qMax(0, x);
        y = qMax(0, y);
        
        // 关键改动：节点局部坐标从(0,0)开始，随后用 setPos 放到(x,y)
        QGraphicsRectItem *node = flowScene->addRect(0, 0, dynamicNodeWidth, dynamicNodeHeight,
                                                    QPen(QColor(95, 139, 152)),
                                                    QBrush(QColor(11, 30, 44, 191)));
        node->setPos(x, y);
        node->setData(0, step.id); // 存储步骤ID
        node->setFlag(QGraphicsItem::ItemIsSelectable);
        node->setFlag(QGraphicsItem::ItemClipsChildrenToShape, true);
        node->setAcceptHoverEvents(true);
        
        // 添加文本，确保文本是节点的子项
        QGraphicsTextItem *text = new QGraphicsTextItem(step.label, node);
        text->setDefaultTextColor(QColor(232, 251, 255));
        
        // 根据节点大小调整字体
        int fontSize = qMin(10, qMax(8, dynamicNodeHeight / 4));
        text->setFont(QFont("Microsoft YaHei", fontSize, QFont::Bold));
        
        // 居中文本 - 相对于父节点
        QRectF textRect = text->boundingRect();
        text->setPos((dynamicNodeWidth - textRect.width()) / 2, 
                    (dynamicNodeHeight - textRect.height()) / 2);
        text->setZValue(1);
        
        flowNodes.append(node);
    }
}

void MainWindow::renderFlowEdges()
{
    // 清除现有边线
    flowEdges.clear();
    
    // 获取动态尺寸
    QSize frameSize = ui->frame_2->size();
    if (frameSize.isEmpty()) {
        frameSize = QSize(400, 300);
    }
    
    int availableWidth = frameSize.width() - 20;
    int availableHeight = frameSize.height() - 20;
    int maxRow = 0;
    int minRow = 0;
    for (const FlowStep &step : flowSteps) {
        maxRow = qMax(maxRow, step.row);
        minRow = qMin(minRow, step.row);
    }
    
    // 设置最小尺寸，确保流程图不会太小
    int minColWidth = 120;
    int minRowHeight = 50;
    int minNodeHeight = 40;
    
    int dynamicColWidth = qMax(minColWidth, availableWidth / colCount);
    int dynamicRowHeight = qMax(minRowHeight, availableHeight / (maxRow - minRow + 2));
    int dynamicNodeHeight = qMin(40, dynamicRowHeight - 10);
    dynamicNodeHeight = qMax(minNodeHeight, dynamicNodeHeight);
    
    // 重新计算左边距，让流程图居中
    int totalWidth = colCount * dynamicColWidth;
    int newMarginLeft = (availableWidth - totalWidth) / 2;
    
    // 创建边线 - 实现流水效果
    for (int i = 0; i < flowSteps.size(); ++i) {
        const FlowStep &step = flowSteps[i];
        
        // 查找同一列的下一个步骤
        for (int j = 0; j < flowSteps.size(); ++j) {
            const FlowStep &nextStep = flowSteps[j];
            if (nextStep.col == step.col && nextStep.row == step.row + 1) {
                // 创建垂直连接线
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
    
    // 创建特殊连接线 - 实现流水效果
    // 等待中 -> 配方解析
    int x1 = newMarginLeft + 1 * dynamicColWidth + dynamicColWidth / 2; // 等待中
    int y1 = marginTop + (-1 - minRow) * dynamicRowHeight + dynamicNodeHeight;
    int x2 = newMarginLeft + 1 * dynamicColWidth + dynamicColWidth / 2; // 配方解析
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

void MainWindow::applyFlowStatuses()
{
    for (int i = 0; i < flowSteps.size(); ++i) {
        const FlowStep &step = flowSteps[i];
        QGraphicsRectItem *node = flowNodes[i];
        
        QString status = (step.id == currentStepId) ? "active" : step.status;
        QColor borderColor = getStatusColor(status);
        
        node->setPen(QPen(borderColor, 2));
        node->setBrush(QBrush(QColor(11, 30, 44, 191)));
        
        // 使用新的动画控制方法
        updateNodeAnimation(i, status);
    }
}

QColor MainWindow::getStatusColor(const QString &status)
{
    if (status == "waiting") return QColor(51, 74, 87);
    if (status == "active") return QColor(14, 197, 233);
    if (status == "done") return QColor(34, 197, 94);
    if (status == "error") return QColor(239, 68, 68);
    if (status == "skip") return QColor(100, 116, 139);
    if (status == "disabled") return QColor(43, 59, 69);
    return QColor(51, 74, 87); // default waiting
}

void MainWindow::setCurrentStep(const QString &stepId)
{
    currentStepId = stepId;
    applyFlowStatuses();
}

/**
 * 设置指定步骤的状态，并自动更新对应的动画效果
 * 
 * 这是外部程序集成的主要接口，通过步骤ID来更新状态和动画
 * 
 * @param stepId 步骤的唯一标识符（如"等待中"、"配方解析"等）
 * @param status 要设置的状态字符串
 * 
 * 工作流程：
 * 1. 根据stepId在flowSteps数组中查找对应的步骤
 * 2. 更新步骤的status属性
 * 3. 调用updateNodeAnimation更新对应节点的动画效果
 * 
 * 外部程序集成示例：
 * ```cpp
 * // 激活等待中节点的呼吸动画
 * mainWindow->setStepStatus("等待中", "active");
 * 
 * // 激活配方解析的处理动画
 * mainWindow->setStepStatus("配方解析", "processing");
 * 
 * // 完成拿取空瓶，停止动画
 * mainWindow->setStepStatus("拿取空瓶", "done");
 * 
 * // 设置错误状态
 * mainWindow->setStepStatus("液体瓶抓取", "error");
 * ```
 * 
 * 注意事项：
 * - stepId必须与createFlowStep时设置的id完全匹配
 * - 如果找不到对应的stepId，不会产生任何效果
 * - 状态更新是即时的，动画会立即开始或停止
 */
void MainWindow::setStepStatus(const QString &stepId, const QString &status)
{
    // 遍历所有流程步骤，查找匹配的stepId
    for (int i = 0; i < flowSteps.size(); ++i) {
        FlowStep &step = flowSteps[i];
        if (step.id == stepId) {
            // 找到匹配的步骤，更新状态
            step.status = status;
            
            // 直接更新该节点的动画状态
            // 这里使用索引i，确保动画应用到正确的节点
            updateNodeAnimation(i, status);
            break; // 找到后立即退出循环
        }
    }
    // 如果没有找到匹配的stepId，不会产生任何效果
}



QPointF MainWindow::gridCenterToScene(int col, int row) const
{
    if (!boardItem) return QPointF();
    const QRectF br = boardItem->boundingRect();
    qreal x = br.left() + col * (br.width() / (gridCols - 1));
    qreal y = br.top() + row * (br.height() / (gridRows - 1));
    return QPointF(x, y);
}

void MainWindow::moveChessPiece(int pieceIndex, int col, int row)
{
    if (pieceIndex < 0 || pieceIndex >= chessPieces.size()) return;
    chessPieces[pieceIndex]->setPos(gridCenterToScene(col, row));
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (ui->graphicsView && chessScene) {
        ui->graphicsView->fitInView(chessScene->sceneRect(), Qt::KeepAspectRatio);
    }
    
    // 重新渲染流程视图以适应新的框体大小
    if (flowScene) {
        renderFlowNodes();
        renderFlowEdges();
        applyFlowStatuses();
    }
}
