#ifndef MYDELEGATE_H
#define MYDELEGATE_H

#include <QStyledItemDelegate>

class MyDelegate : public QStyledItemDelegate
{
public:
    MyDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem newOption = option;
        QString str = index.data().toString();
        if (str == "失败") {
            newOption.palette.setColor(QPalette::Text, QColor(Qt::red));
        } else if (str == "进行中"){
            newOption.palette.setColor(QPalette::Text, QColor(Qt::blue));
        }

        QStyledItemDelegate::paint(painter, newOption, index);
    }
};
#endif // MYDELEGATE_H
