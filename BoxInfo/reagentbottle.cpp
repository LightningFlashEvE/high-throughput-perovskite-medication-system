#include "reagentbottle.h"

ReagentBottle::ReagentBottle(QObject *parent)
    : QObject{parent}
{}

void ReagentBottle::setName(const QString &v) {
    if (m_name == v) return;
    m_name = v;
    emit nameChanged(m_name); // nameChanged 信号用于通知界面等相关联的对象瓶名被修改，通常会与界面上对应label或text控件的槽函数关联，如 setText 或自定义刷新函数，实现界面同步更新
}
void ReagentBottle::setInitial(double v) {
    if (m_initial == v) return;
    m_initial = v; emit initialChanged(m_initial);
}
void ReagentBottle::setRemaining(double v) {
    if (m_remaining == v) return;
    m_remaining = v; emit remainingChanged(m_remaining);
}
void ReagentBottle::setHeight(double v) {
    if (m_height == v) return;
    m_height = v; emit heightChanged(m_height);
}
void ReagentBottle::setX(double v) {
    if (m_x == v) return;
    m_x = v; emit positionChanged(m_x, m_y);
}
void ReagentBottle::setY(double v) {
    if (m_y == v) return;
    m_y = v; emit positionChanged(m_x, m_y);
}
void ReagentBottle::setPos(double x, double y) {
    bool ch = (m_x != x) || (m_y != y);
    m_x = x; m_y = y;
    if (ch) emit positionChanged(m_x, m_y);
}
void ReagentBottle::setLot(const QString &v) {
    if (m_lot == v) return;
    m_lot = v; emit lotChanged(m_lot);
}
void ReagentBottle::setExpiry(const QDate &v) {
    if (m_expiry == v) return;
    m_expiry = v; emit expiryChanged(m_expiry);
}
void ReagentBottle::setBarcode(const QString &v) {
    if (m_barcode == v) return;
    m_barcode = v; emit barcodeChanged(m_barcode);
}
void ReagentBottle::setNote(const QString &v) {
    if (m_note == v) return;
    m_note = v; emit noteChanged(m_note);
}
