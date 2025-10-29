// Box.cpp
#include "Box.h"
#include "Slot.h"
#include "ReagentBottle.h"

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
