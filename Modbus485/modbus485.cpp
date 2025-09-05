#include "modbus485.h"
#include <QSerialPortInfo>
#include <QDebug>

/**
 * 构造函数
 * 功能：初始化Modbus485通信模块
 * 参数：parent - 父对象指针，用于Qt对象树管理
 * 初始化内容：
 * 1. 创建QSerialPort实例用于串口通信
 * 2. 创建心跳定时器用于连接状态检测  
 * 3. 连接串口的readyRead信号到数据接收处理
 * 4. 连接串口错误信号到错误处理
 * 5. 设置心跳定时器间隔为1秒
 */
Modbus485::Modbus485(QObject *parent)
    : QObject{parent}
{
    serialPort = new QSerialPort(this);
    heartbeatTimer = new QTimer(this);
    
    connect(serialPort, &QSerialPort::readyRead, this, &Modbus485::onReadyRead);
    connect(serialPort, QOverload<QSerialPort::SerialPortError>::of(&QSerialPort::errorOccurred),
            this, &Modbus485::onSerialError);
    
    // 心跳定时器，用于检测连接状态
    heartbeatTimer->setInterval(1000);
    connect(heartbeatTimer, &QTimer::timeout, this, [this](){
        if (serialPort && serialPort->isOpen()) {
            // 发送心跳包或检查连接状态
        }
    });
}

/**
 * 析构函数
 * 功能：清理资源，确保串口正确关闭
 * 自动调用：closePort() 关闭串口连接
 */
Modbus485::~Modbus485()
{
    closePort();
}

/**
 * 打开串口连接
 * 功能：配置并打开指定的串口，启动心跳检测
 * 参数：
 *   portName - 串口名称（如"COM3", "/dev/ttyUSB0"）
 *   baudRate - 波特率（如9600, 115200）
 * 返回值：
 *   true  - 串口打开成功
 *   false - 串口打开失败
 * 串口配置：
 *   数据位：8位
 *   校验位：无
 *   停止位：1位
 *   流控制：无
 * 副作用：
 *   成功时启动心跳定时器并发出connectionStatusChanged(true)信号
 *   失败时发出错误信息到errorOccurred信号
 */
bool Modbus485::openPort(const QString &portName, int baudRate)
{
    if (serialPort->isOpen()) {
        closePort();
    }
    
    serialPort->setPortName(portName);
    serialPort->setBaudRate(baudRate);
    serialPort->setDataBits(QSerialPort::Data8);
    serialPort->setParity(QSerialPort::NoParity);
    serialPort->setStopBits(QSerialPort::OneStop);
    serialPort->setFlowControl(QSerialPort::NoFlowControl);
    
    if (serialPort->open(QIODevice::ReadWrite)) {
        updateConnectionStatus(true);
        heartbeatTimer->start();
        emit errorOccurred(QString("串口 %1 打开成功，波特率：%2").arg(portName).arg(baudRate));
        return true;
    } else {
        updateConnectionStatus(false);
        emit errorOccurred(QString("串口 %1 打开失败：%2").arg(portName, serialPort->errorString()));
        return false;
    }
}

/**
 * 关闭串口连接
 * 功能：安全关闭串口连接，停止心跳检测
 * 操作步骤：
 *   1. 停止心跳定时器
 *   2. 关闭串口
 *   3. 更新连接状态为false
 *   4. 发出"串口已关闭"状态信息
 * 副作用：发出connectionStatusChanged(false)信号
 */
void Modbus485::closePort()
{
    if (serialPort && serialPort->isOpen()) {
        heartbeatTimer->stop();
        serialPort->close();
        updateConnectionStatus(false);
        emit errorOccurred("串口已关闭");
    }
}

/**
 * 检查串口连接状态
 * 功能：查询当前串口是否处于打开状态
 * 返回值：
 *   true  - 串口已打开且可用
 *   false - 串口未打开或不可用
 * 注意：同时检查serialPort指针有效性和isOpen()状态
 */
bool Modbus485::isPortOpen() const
{
    return serialPort && serialPort->isOpen();
}

/**
 * 获取系统可用串口列表
 * 功能：扫描系统中所有可用的串口设备，过滤掉蓝牙虚拟串口
 * 返回值：QStringList - 可用的物理串口名称列表（如["COM1", "COM3", "COM8"]）
 * 过滤规则：
 *   排除描述/制造商包含"bluetooth"或"蓝牙"的设备
 *   排除"standard serial over bluetooth"设备
 *   排除以"BTHMODEM"开头的端口名
 * 用途：填充UI中的串口选择下拉框
 */
QStringList Modbus485::availablePorts() const
{
    QStringList ports;
    const auto serialPortInfos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &portInfo : serialPortInfos) {
        // 过滤掉蓝牙串口设备
        QString description = portInfo.description().toLower();
        QString manufacturer = portInfo.manufacturer().toLower();
        
        // 跳过蓝牙相关设备
        if (description.contains("bluetooth") || 
            description.contains("蓝牙") ||
            manufacturer.contains("bluetooth") ||
            manufacturer.contains("蓝牙") ||
            description.contains("standard serial over bluetooth") ||
            portInfo.portName().startsWith("BTHMODEM")) {
            continue;
        }
        
        ports << portInfo.portName();
    }
    return ports;
}

/**
 * 发送原始字节数据
 * 功能：通过串口发送字节数组数据
 * 参数：data - 要发送的原始字节数组（已包含CRC或其他校验码）
 * 前置条件：串口必须在打开状态
 * 操作步骤：
 *   1. 检查串口是否可用
 *   2. 写入数据到串口
 *   3. 检查写入字节数是否完整
 * 副作用：
 *   成功时输出调试信息到控制台
 *   失败时发出errorOccurred信号
 */
void Modbus485::sendData(const QByteArray &data)
{
    if (serialPort && serialPort->isOpen()) {
        qint64 written = serialPort->write(data);
        if (written == data.size()) {
            qDebug() << "[485发送]" << data.toHex();
        } else {
            emit errorOccurred("数据发送不完整");
        }
    } else {
        emit errorOccurred("串口未打开，无法发送数据");
    }
}

/**
 * 发送文本数据（重载函数）
 * 功能：将文本转换为UTF-8编码的字节数组后发送
 * 参数：text - 要发送的文本字符串
 * 内部操作：调用sendData(const QByteArray&)重载函数
 * 用途：方便发送简单的文本命令或测试数据
 */
void Modbus485::sendData(const QString &text)
{
    sendData(text.toUtf8());
}

/**
 * 串口数据接收处理槽函数
 * 功能：当串口有数据到达时自动调用，读取并处理数据
 * 触发条件：QSerialPort::readyRead信号被发出时
 * 处理步骤：
 *   1. 检查serialPort指针有效性
 *   2. 读取串口中的所有可用数据
 *   3. 输出16进制调试信息到控制台
 *   4. 发出dataReceived信号（原始字节）
 *   5. 发出textReceived信号（UTF-8文本）
 * 注意：使用readAll()一次性读取所有数据，适合短帧通信
 */
void Modbus485::onReadyRead()
{
    if (!serialPort) return;
    
    QByteArray data = serialPort->readAll();
    if (!data.isEmpty()) {
        qDebug() << "[485接收]" << data.toHex();
        emit dataReceived(data);
        emit textReceived(QString::fromUtf8(data));
    }
}

/**
 * 串口错误处理槽函数
 * 功能：当串口发生错误时自动调用，处理各种错误情况
 * 参数：error - QSerialPort错误类型枚举值
 * 触发条件：QSerialPort::errorOccurred信号被发出时
 * 错误处理：
 *   ResourceError - 串口资源错误，自动关闭连接
 *   其他错误    - 发出错误信息但保持连接
 *   NoError      - 忽略（正常状态）
 * 副作用：发出errorOccurred信号带有错误描述
 */
void Modbus485::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::ResourceError) {
        closePort();
        emit errorOccurred("串口资源错误，已自动断开");
    } else if (error != QSerialPort::NoError) {
        emit errorOccurred(QString("串口错误：%1").arg(serialPort->errorString()));
    }
}

/**
 * 更新连接状态
 * 功能：内部状态管理函数，当连接状态发生变化时发出信号
 * 参数：status - 新的连接状态（true=已连接，false=已断开）
 * 返回值：无
 * 逻辑：
 *   只有当新状态与当前状态不同时才更新并发出信号
 *   避免重复的状态变化通知
 * 副作用：发出connectionStatusChanged信号
 * 调用者：openPort()、closePort()、onSerialError()
 */
void Modbus485::updateConnectionStatus(bool status)
{
    if (connected != status) {
        connected = status;
        emit connectionStatusChanged(status);
    }
}
