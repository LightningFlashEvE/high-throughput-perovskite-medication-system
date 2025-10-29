// Slot.cpp
#include "slot.h"
#include "ReagentBottle.h"

Slot::Slot(QObject *parent)
    : QObject(parent)
{
}

Slot::Slot(int index, QObject *parent)
    : QObject(parent), m_index(index)
{
}

void Slot::setIndex(int idx) {
    if (m_index == idx) return;
    m_index = idx;
    emit indexChanged(m_index);
}

void Slot::setBottle(ReagentBottle *b) {
    if (b == m_bottle) return; // 如果瓶子已经存在，则返回

    // 先清理旧瓶
    const bool had = hasBottle(); // 先检查是否已经有瓶子
    if (m_bottle) { m_bottle->deleteLater(); m_bottle = nullptr; } // 如果瓶子存在，则删除

    // 接管新瓶
    m_bottle = b;
    if (m_bottle && m_bottle->parent() != this)
        m_bottle->setParent(this);

    emit bottleReplaced(m_bottle);
    if (hasBottle() != had) emit hasBottleChanged(hasBottle());
}

void Slot::removeBottle() {
    if (!m_bottle) return;
    m_bottle->deleteLater();
    m_bottle = nullptr;
    emit hasBottleChanged(false);
}
