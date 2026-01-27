#include "Common.h"
#include <QDateTime>

namespace Common {

int showMessageBox(const QString& title, const QString& text)
{
    // 创建消息框
    QMessageBox msgBox;
    msgBox.setWindowTitle(title);
    msgBox.setText(text);
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);

    return msgBox.exec();
}

QString getCurDateTime() {
    return QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
}

}
