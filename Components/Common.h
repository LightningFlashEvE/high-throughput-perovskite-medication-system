#ifndef COMMON_H
#define COMMON_H

#include <QString>
#include <QMessageBox>

namespace Common {

    int showMessageBox(const QString& title, const QString& text);

    QString getCurDateTime();
}



#endif // COMMON_H
