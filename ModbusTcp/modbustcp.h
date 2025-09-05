#ifndef MODBUSTCP_H
#define MODBUSTCP_H

#include <QObject>

class ModbusTcp : public QObject
{
    Q_OBJECT
public:
    explicit ModbusTcp(QObject *parent = nullptr);

signals:
};

#endif // MODBUSTCP_H
