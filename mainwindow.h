#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QPointF>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QTimer;
class QGraphicsScene;
class QGraphicsPixmapItem;
class QResizeEvent;
class QGraphicsRectItem;
class QGraphicsTextItem;
class QGraphicsLineItem;
class QSequentialAnimationGroup;
class QVariantAnimation;

/**
 * 流程步骤结构体
 * 
 * 用于定义流程图中的每个步骤节点，包含位置、显示和状态信息
 * 
 * 成员说明：
 * - id: 步骤的唯一标识符，用于状态更新和查找
 * - label: 显示在节点上的文本标签
 * - col: 节点在流程图中的列位置（0-2）
 * - row: 节点在流程图中的行位置（-1到11）
 * - status: 当前状态，决定节点的外观和动画效果
 * 
 * 状态类型说明：
 * - "waiting": 等待状态，灰色边框，无动画
 * - "active": 激活状态，蓝色发光 + 呼吸动画
 * - "processing": 处理状态，青色发光 + 呼吸动画
 * - "warning": 警告状态，黄色发光 + 呼吸动画
 * - "done": 完成状态，绿色边框，无动画
 * - "error": 错误状态，红色边框，无动画
 * - "skip": 跳过状态，灰色边框，半透明
 * - "disabled": 禁用状态，深灰色边框，无动画
 * 
 * 使用示例：
 * FlowStep step;
 * step.id = "配方解析";
 * step.label = "配方解析";
 * step.col = 1;
 * step.row = 0;
 * step.status = "waiting";
 */
struct FlowStep {
    QString id;      ///< 步骤唯一标识符
    QString label;   ///< 显示文本
    int col;         ///< 列位置
    int row;         ///< 行位置
    QString status;  ///< 当前状态
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void updateTime();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    Ui::MainWindow *ui;
    QTimer *timer;

    // Graphics view / scene for chessboard rendering
    QGraphicsScene *chessScene = nullptr;
    QGraphicsPixmapItem *boardItem = nullptr;
    QVector<QGraphicsPixmapItem*> chessPieces;

    // Grid config (Chinese chess 9x10)
    int gridCols = 9;
    int gridRows = 10;
    qreal cellWidth = 0.0;
    qreal cellHeight = 0.0;

    // Flow view components
    // 用于流程图显示的场景与缓存结构（节点与连线图元）
    QGraphicsScene *flowScene = nullptr;
    QVector<FlowStep> flowSteps;
    QVector<QGraphicsRectItem*> flowNodes;
    QVector<QGraphicsLineItem*> flowEdges;
    QVector<QSequentialAnimationGroup*> flowAnimations;  // 存储动画组
    QString currentStepId;
    
    // Flow view constants
    const int colCount = 3;
    const int colWidth = 300;
    const int rowHeight = 100;
    const int marginLeft = 60;
    const int marginTop = 16;
    const int nodeWidth = 220;
    const int nodeHeight = 64;

    void initChessView();
    QPointF gridCenterToScene(int col, int row) const;
    
    // Flow view methods
    /**
     * 初始化流程图视图，将流程场景挂载到 `frame_2` 中，
     * 并构建默认的步骤集合与首次渲染。
     */
    void initFlowView();

    /**
     * 渲染流程节点：
     * - 依据 `flowSteps` 计算布局尺寸
     * - 按列/行生成矩形节点，文本作为子项居中显示
     */
    void renderFlowNodes();

    /**
     * 渲染流程连线：
     * - 同列上下相邻步骤之间绘制竖直连线
     * - 按需求绘制跨列虚线连接
     */
    void renderFlowEdges();

    /**
     * 应用节点状态（waiting/active/done/error/skip/disabled），
     * 根据状态更新节点边框与填充效果。
     */
    void applyFlowStatuses();

    /** 将当前步骤设置为 stepId，并刷新节点高亮状态。 */
    void setCurrentStep(const QString &stepId);

    /**
     * 设置指定步骤的状态，并自动更新对应的动画效果
     * @param stepId 步骤的唯一标识符（如"等待中"、"配方解析"等）
     * @param status 要设置的状态字符串
     * 
     * 功能说明：
     * - 根据stepId查找对应的流程步骤
     * - 更新步骤的status属性
     * - 自动调用updateNodeAnimation更新动画效果
     * 
     * 支持的状态类型（与updateNodeAnimation一致）：
     * - "active": 蓝色发光 + 呼吸动画
     * - "processing": 青色发光 + 呼吸动画
     * - "warning": 黄色发光 + 呼吸动画
     * - "done": 绿色边框，无动画
     * - "error": 红色边框，无动画
     * - "waiting": 灰色边框，无动画
     * 
     * 使用示例：
     * setStepStatus("等待中", "active");        // 激活等待中节点的呼吸动画
     * setStepStatus("配方解析", "processing");  // 激活配方解析的处理动画
     * setStepStatus("拿取空瓶", "done");        // 完成拿取空瓶，停止动画
     * 
     * 外部程序集成：
     * 当外部程序需要更新流程状态时，只需调用此方法即可：
     * mainWindow->setStepStatus("当前步骤ID", "目标状态");
     */
    void setStepStatus(const QString &stepId, const QString &status);

    /** 根据状态字符串返回对应的主题颜色。 */
    QColor getStatusColor(const QString &status);

    /** 便捷创建一个流程步骤并追加到 `flowSteps`。 */
    void createFlowStep(const QString &id, const QString &label, int col, int row);
    
    /**
     * 为指定节点创建呼吸动画效果
     * @param node 目标节点对象
     * @param nodeIndex 节点在flowNodes数组中的索引
     * 
     * 实现细节：
     * - 创建450ms的缩放动画（1.0 ↔ 1.03）
     * - 使用InOutQuad缓动曲线
     * - 无限循环播放
     * - 通过QVariantAnimation和信号槽实现
     */
    void createBreathingAnimation(QGraphicsRectItem *node, int nodeIndex);
    
    /**
     * 根据节点状态控制动画显示
     * @param nodeIndex 节点在flowNodes数组中的索引
     * @param status 节点状态字符串
     * 
     * 支持的状态类型：
     * - "active": 蓝色发光(#5533e6ff) + 呼吸动画
     * - "processing": 青色发光(#22d3ee) + 呼吸动画  
     * - "warning": 黄色发光(#ffc107) + 呼吸动画
     * - "error": 红色发光(#ef4444) + 呼吸动画 (可扩展)
     * - "success": 绿色发光(#22c55e) + 呼吸动画 (可扩展)
     * - 其他状态: 无动画，无发光效果
     * 
     * 使用示例：
     * updateNodeAnimation(0, "active");     // 激活第0个节点的呼吸动画
     * updateNodeAnimation(1, "processing"); // 激活第1个节点的处理动画
     * updateNodeAnimation(2, "done");       // 停止第2个节点的动画
     */
    void updateNodeAnimation(int nodeIndex, const QString &status);
    
    /**
     * 批量更新所有节点的动画状态
     * 
     * 功能：
     * - 遍历所有flowSteps，根据每个步骤的status更新对应节点的动画
     * - 通常在初始化或重置流程时调用
     * 
     * 使用场景：
     * - 程序启动时初始化所有节点状态
     * - 从外部程序加载流程状态后同步动画
     * - 重置流程时批量更新所有节点
     */
    void updateAllNodeAnimations();

public:
    // Move piece by index to board grid(col,row)
    void moveChessPiece(int pieceIndex, int col, int row);
};
#endif // MAINWINDOW_H



