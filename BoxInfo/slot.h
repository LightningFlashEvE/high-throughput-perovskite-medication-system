// Slot.h
#ifndef SLOT_H
#define SLOT_H

#include <QObject>

class ReagentBottle;

/**
 * @brief 插槽类 - 单个试剂瓶的容器
 * 
 * 功能说明：
 * - 每个插槽可以存放一个试剂瓶（ReagentBottle）
 * - 管理试剂瓶的生命周期（所有权转移）
 * - 提供插槽状态查询功能
 * - 支持Qt属性系统，可用于QML绑定
 */
class Slot : public QObject   //  /slɑːt/  槽
{
    Q_OBJECT
    // Qt属性系统：插槽索引，支持读写和变化通知
    Q_PROPERTY(int  index READ index WRITE setIndex NOTIFY indexChanged)
    // Qt属性系统：是否有试剂瓶，只读属性
    Q_PROPERTY(bool hasBottle READ hasBottle NOTIFY hasBottleChanged)

public:
    /**
     * @brief 默认构造函数 - 创建索引为1的插槽
     * @param parent 父对象
     */
    explicit Slot(QObject *parent = nullptr);
    
    /**
     * @brief 带索引构造函数 - 创建指定索引的插槽
     * @param index 插槽索引（从1开始）
     * @param parent 父对象
     */
    explicit Slot(int index, QObject *parent = nullptr);

    /**
     * @brief 获取插槽索引
     * @return 插槽索引（从1开始）
     */
    int  index() const { return m_index; }
    
    /**
     * @brief 检查是否有试剂瓶
     * @return true表示有瓶，false表示无瓶
     */
    bool hasBottle() const { return m_bottle != nullptr; }

    /**
     * @brief 获取当前试剂瓶
     * @return 试剂瓶指针，无瓶返回nullptr
     */
    ReagentBottle* bottle() const { return m_bottle; }

public slots:
    /**
     * @brief 设置插槽索引
     * @param idx 新的索引值
     */
    void setIndex(int idx);
    
    /**
     * @brief 放入试剂瓶
     * @param b 试剂瓶指针
     * 
     * 功能说明：
     * - 如果插槽已有试剂瓶，旧瓶会被删除
     * - 新瓶的所有权转移到本插槽（parent设为this）
     * - 会发射bottleReplaced信号
     */
    void setBottle(ReagentBottle* b);
    
    /**
     * @brief 移除当前试剂瓶
     * 
     * 功能说明：
     * - 删除当前试剂瓶对象
     * - 将m_bottle设为nullptr
     * - 会发射hasBottleChanged信号
     */
    void removeBottle();

signals:
    /**
     * @brief 索引变化信号
     * @param newIndex 新的索引值
     */
    void indexChanged(int);
    
    /**
     * @brief 试剂瓶状态变化信号
     * @param hasBottle 是否有试剂瓶
     */
    void hasBottleChanged(bool);
    
    /**
     * @brief 试剂瓶替换信号
     * @param newBottle 新放入的试剂瓶指针
     * 触发时机：调用setBottle时，无论是否已有旧瓶
     */
    void bottleReplaced(ReagentBottle* newBottle);

private:
    int m_index = 1;                 // 插槽索引（1..N）
    ReagentBottle* m_bottle = nullptr; // 试剂瓶指针，QObject子对象，parent=this
};

#endif // SLOT_H
