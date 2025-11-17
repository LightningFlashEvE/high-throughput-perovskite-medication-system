// Box.cpp
#include "Box.h"
#include "Slot.h"
#include "ReagentBottle.h"
#include <QSettings>
#include <QDebug>

Box::Box(QObject *parent)
    : QObject(parent)
{
    resizeSlots(15);
}

Box::Box(int numSlots, QObject *parent)
    : QObject(parent)
{
    resizeSlots(numSlots);
}

Box::Box(int numSlots, const QString& boxName, QObject *parent)
    : QObject(parent)
    , m_boxName(boxName)
{
    resizeSlots(numSlots);
    loadCoordinatesFromConfig();  // 从配置文件加载坐标信息
}

void Box::clearAll() {
    for (int i = 0; i < m_slots.size(); ++i) {
        auto *s = m_slots[i];
        if (s) s->deleteLater();
    }
    m_slots.clear();
}

void Box::resizeSlots(int numSlots) {
    if (numSlots < 1) numSlots = 1;
    clearAll();
    m_slots.reserve(numSlots); // 预留空间
    for (int i = 0; i < numSlots; ++i) {
        auto *s = new Slot(i + 1, this);
        m_slots.push_back(s); // 添加到槽列表
    }
    emit structureReset();
}

Slot* Box::slotAt(int position1Based) const {
    int i = posToIndex(position1Based);
    if (i < 0) return nullptr;
    return m_slots[i];
}

bool Box::hasBottle(int position1Based) const {
    auto *s = slotAt(position1Based);
    return s ? s->hasBottle() : false;
}

ReagentBottle* Box::bottleAt(int position1Based) const {
    auto *s = slotAt(position1Based);
    return s ? s->bottle() : nullptr;
}

void Box::addReagentBottleToSlot(int position1Based, ReagentBottle *bottle) { // 将瓶子添加到槽中
    auto *s = slotAt(position1Based); // 获取槽
    if (!s) return;
    s->setBottle(bottle); // parent 将设为 slot
    emit slotChanged(position1Based); // 发射槽变化信号 告诉外界槽中有了瓶子
}

void Box::removeBottleFromSlot(int position1Based) {
    auto *s = slotAt(position1Based);
    if (!s || !s->hasBottle()) return;
    s->removeBottle();
    emit slotChanged(position1Based);
}

void Box::updateRemaining(int position1Based, double newRemaining) {
    auto *rb = bottleAt(position1Based);
    if (!rb) return;
    rb->setRemaining(newRemaining);
    emit slotChanged(position1Based);
}

void Box::loadCoordinatesFromConfig() {
    if (m_boxName.isEmpty()) {
        qWarning() << "Box名称为空，无法加载配置";
        return;
    }
    
    // 从 BoxData.ini 加载配置
    QSettings settings("BoxData.ini", QSettings::IniFormat);
    
    settings.beginGroup(m_boxName);
    
    // 读取坐标信息，如果不存在则使用默认值并写入
    if (!settings.contains("axisX")) {
        // 第一次创建，写入默认值
        settings.setValue("axisX", "00003A99");
        settings.setValue("axisY", "00005B53");
        settings.setValue("axisZ", "00041AC7");
        settings.setValue("gripperDepth", "0");
        settings.setValue("liquidExtractionDepth", "0");
        settings.setValue("solidDepth", "0");
        qDebug() << "首次创建Box配置:" << m_boxName << "，已写入默认值";
    }
    
    // 读取配置值
    m_axisX = settings.value("axisX", "00000000").toString(); // 这里的 00000000 是默认值，如果配置文件中没有对应的值，则使用默认值
    m_axisY = settings.value("axisY", "00000000").toString();
    m_axisZ = settings.value("axisZ", "00000000").toString();
    m_gripperDepth = settings.value("gripperDepth", "0").toString();
    m_liquidExtractionDepth = settings.value("liquidExtractionDepth", "0").toString();
    m_solidDepth = settings.value("solidDepth", "0").toString();
    
    settings.endGroup();
    
    qDebug() << "Box配置加载完成:" << m_boxName
             << "X:" << m_axisX
             << "Y:" << m_axisY
             << "Z:" << m_axisZ;
}
