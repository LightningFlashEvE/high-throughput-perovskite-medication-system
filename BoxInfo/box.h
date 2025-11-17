// Box.h
#ifndef BOX_H
#define BOX_H

#include <QObject>
#include <QVector>
#include <QString>

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
     * @brief 默认构造函数 - 创建15个插槽的盒子，会初始化槽的数量m_slots
     * @param parent 父对象
     */
    explicit Box(QObject *parent = nullptr);
    
    /**
     * @brief 带参数构造函数 - 创建指定数量插槽的盒子，会初始化槽的数量m_slots
     * @param numSlots 插槽数量
     * @param parent 父对象
     */
    explicit Box(int numSlots, QObject *parent = nullptr);
    
    /**
     * @brief 带名称构造函数 - 创建指定数量插槽的盒子，并从配置文件加载坐标信息
     * @param numSlots 插槽数量
     * @param boxName 盒子名称（用于配置文件中的组名，如 "Box-Transfer-Area-Left"）
     * @param parent 父对象
     */
    explicit Box(int numSlots, const QString& boxName, QObject *parent = nullptr);

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
     * @brief 获取指定位置的试剂瓶  return m_slots[i];    QVector<Slot*> m_slots;
     * @param position1Based 基于1的位置编号
     * @return 试剂瓶指针，无瓶或位置无效返回nullptr
     */
    ReagentBottle* bottleAt(int position1Based) const;
    
    // ========== 坐标信息相关 ==========
    /**
     * @brief 获取盒子名称
     * @return 盒子名称
     */
    QString getBoxName() const { return m_boxName; }
    
    /**
     * @brief 获取X轴坐标
     * @return X轴坐标（十六进制字符串）
     */
    QString getAxisX() const { return m_axisX; }
    
    /**
     * @brief 获取Y轴坐标
     * @return Y轴坐标（十六进制字符串）
     */
    QString getAxisY() const { return m_axisY; }
    
    /**
     * @brief 获取Z轴坐标
     * @return Z轴坐标（十六进制字符串）
     */
    QString getAxisZ() const { return m_axisZ; }
    
    /**
     * @brief 获取夹爪深度
     * @return 夹爪深度
     */
    QString getGripperDepth() const { return m_gripperDepth; }
    
    /**
     * @brief 获取取液深度
     * @return 取液深度
     */
    QString getLiquidExtractionDepth() const { return m_liquidExtractionDepth; }
    
    /**
     * @brief 获取固体深度
     * @return 固体深度
     */
    QString getSolidDepth() const { return m_solidDepth; }

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
    
    // 盒子标识和坐标信息
    QString m_boxName;                  // 盒子名称（如 "Box-Transfer-Area-Left"）
    QString m_axisX;                    // X轴坐标
    QString m_axisY;                    // Y轴坐标
    QString m_axisZ;                    // Z轴坐标
    QString m_gripperDepth;             // 夹爪深度
    QString m_liquidExtractionDepth;    // 取液深度
    QString m_solidDepth;               // 固体深度

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
    
    /**
     * @brief 从配置文件加载坐标信息
     * 根据 m_boxName 从 BoxData.ini 文件中加载对应的坐标数据
     */
    void loadCoordinatesFromConfig();
};

#endif // BOX_H
