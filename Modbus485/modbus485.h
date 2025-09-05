#ifndef MODBUS485_H
#define MODBUS485_H

#include <QObject>
#include <QSerialPort>
#include <QTimer>
#include <QStringList>

class Modbus485 : public QObject
{
    Q_OBJECT
public:
    explicit Modbus485(QObject *parent = nullptr);
    ~Modbus485();

    // 串口操作
    bool openPort(const QString &portName, int baudRate);
    void closePort();
    bool isPortOpen() const;
    
    // 获取可用串口列表
    QStringList availablePorts() const;
    
    // 发送数据
    void sendData(const QByteArray &data);
    void sendData(const QString &text);

signals:
    // 连接状态变化
    void connectionStatusChanged(bool connected);
    // 接收到数据
    void dataReceived(const QByteArray &data);
    void textReceived(const QString &text);
    // 错误信息
    void errorOccurred(const QString &error);

private slots:
    void onReadyRead();
    void onSerialError(QSerialPort::SerialPortError error);

private:
    QSerialPort *serialPort = nullptr;
    QTimer *heartbeatTimer = nullptr;
    bool connected = false;
    
    void updateConnectionStatus(bool status);
};

#endif // MODBUS485_H
