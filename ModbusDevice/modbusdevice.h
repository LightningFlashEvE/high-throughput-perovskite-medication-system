#ifndef MODBUSDEVICE_H
#define MODBUSDEVICE_H

#include <QObject>

class ModbusDevice : public QObject
{
    Q_OBJECT
public:
    explicit ModbusDevice(QObject *parent = nullptr);

signals:
};

#endif // MODBUSDEVICE_H
