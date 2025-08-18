#ifndef FLOWVIEWMANAGER_H
#define FLOWVIEWMANAGER_H

#include <QObject>
#include <QVector>
#include <QColor>
#include <QString>

class QWidget;
class QGraphicsView;
class QGraphicsScene;
class QGraphicsRectItem;
class QGraphicsLineItem;
class QSequentialAnimationGroup;

// 流程步骤数据结构
struct FlowStep {
    QString id;
    QString label;
    int col;
    int row;
    QString status; // waiting/active/processing/warning/done/error/skip/disabled
};

class FlowViewManager : public QObject {
    Q_OBJECT
public:
    explicit FlowViewManager(QObject *parent = nullptr);
    ~FlowViewManager() override;

    // 挂载到容器并初始化默认流程
    void init(QWidget *containerWidget);

    // 容器尺寸变化时重排
    void relayout();

    // 对外接口
    void setCurrentStep(const QString &stepId);
    void setStepStatus(const QString &stepId, const QString &status);
    void updateAllNodeAnimations();

private:
    // 视图/场景
    QWidget *container = nullptr;
    QGraphicsView *flowView = nullptr;
    QGraphicsScene *flowScene = nullptr;

    // 数据与图元
    QVector<FlowStep> flowSteps;
    QVector<QGraphicsRectItem*> flowNodes;
    QVector<QGraphicsLineItem*> flowEdges;
    QVector<QSequentialAnimationGroup*> flowAnimations;
    QString currentStepId;

    // 布局常量
    const int colCount = 3;
    const int marginTop = 16;

    // 初始化与渲染
    void initDefaultSteps();
    void renderFlowNodes();
    void renderFlowEdges();
    void applyFlowStatuses();

    // 工具与动画
    void createFlowStep(const QString &id, const QString &label, int col, int row);
    void createBreathingAnimation(QGraphicsRectItem *node, int nodeIndex);
    void updateNodeAnimation(int nodeIndex, const QString &status);
    QColor getStatusColor(const QString &status);
};

#endif // FLOWVIEWMANAGER_H


