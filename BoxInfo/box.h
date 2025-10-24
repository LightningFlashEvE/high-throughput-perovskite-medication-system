// Box.h
#ifndef BOX_H
#define BOX_H

#include <QObject>
#include <QVector>

class Slot;
class ReagentBottle;

/**
 * @brief 试剂盒类 - 管理多个插槽的容器
 * 
 * 功能说明：
 * - 管理一组插槽（Slot），每个插槽可以存放一个试剂瓶
 * - 提供基于1的位置编号（用户友好）
 * - 支持动态调整插槽数量
 * - 提供试剂瓶的增删改查功能
 */
class Box : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief 默认构造函数 - 创建15个插槽的盒子
     * @param parent 父对象
     */
    explicit Box(QObject *parent = nullptr);
    
    /**
     * @brief 带参数构造函数 - 创建指定数量插槽的盒子
     * @param numSlots 插槽数量
     * @param parent 父对象
     */
    explicit Box(int numSlots, QObject *parent = nullptr);

    /**
     * @brief 获取插槽总数
     * @return 插槽数量
     */
    int  slotsCount() const { return m_slots.size(); }
    
    /**
     * @brief 根据位置编号获取插槽对象
     * @param position1Based 基于1的位置编号（从1开始）
     * @return 插槽对象指针，越界返回nullptr
     */
    Slot* slotAt(int position1Based) const;

    /**
     * @brief 检查指定位置是否有试剂瓶
     * @param position1Based 基于1的位置编号
     * @return true表示有瓶，false表示无瓶或位置无效
     */
    bool hasBottle(int position1Based) const;
    
    /**
     * @brief 获取指定位置的试剂瓶
     * @param position1Based 基于1的位置编号
     * @return 试剂瓶指针，无瓶或位置无效返回nullptr
     */
    ReagentBottle* bottleAt(int position1Based) const;

public slots:
    /**
     * @brief 重新设置插槽数量（会清空所有现有数据）
     * @param numSlots 新的插槽数量
     */
    void resizeSlots(int numSlots);

    /**
     * @brief 在指定插槽放入试剂瓶
     * @param position1Based 基于1的位置编号
     * @param bottle 试剂瓶指针（Box将接管所有权）
     */
    void addReagentBottleToSlot(int position1Based, ReagentBottle* bottle);
    
    /**
     * @brief 从指定插槽移除试剂瓶
     * @param position1Based 基于1的位置编号
     */
    void removeBottleFromSlot(int position1Based);

    /**
     * @brief 更新指定位置试剂瓶的剩余量
     * @param position1Based 基于1的位置编号
     * @param newRemaining 新的剩余量
     * 注意：如果该位置没有试剂瓶，则无操作
     */
    void updateRemaining(int position1Based, double newRemaining);

signals:
    /**
     * @brief 插槽状态变化信号
     * @param position1Based 发生变化的插槽位置
     * 触发时机：放入瓶、取出瓶、更新瓶信息时
     */
    void slotChanged(int position1Based);
    
    /**
     * @brief 结构重置信号
     * 触发时机：清空所有数据或重建插槽结构时
     */
    void structureReset();

private:
    QVector<Slot*> m_slots;  // 插槽对象数组

    /**
     * @brief 将基于1的位置编号转换为基于0的数组索引
     * @param position1Based 基于1的位置编号
     * @return 数组索引，无效位置返回-1
     */
    int posToIndex(int position1Based) const
    {
        if (position1Based < 1 || position1Based > m_slots.size()) return -1;
        return position1Based - 1;
    }
    
    /**
     * @brief 清空所有插槽（内部使用）
     * 删除所有插槽对象并清空数组
     */
    void clearAll();
};

#endif // BOX_H
