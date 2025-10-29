#include "tcpclientcore.h"
#include <QHostAddress>
#include <QDebug>

TcpClientCore::TcpClientCore(QObject *parent)
    : QObject{parent}
    , m_tcpSocket(nullptr)
{
    // 创建 TCP Socket
    m_tcpSocket = new QTcpSocket(this);
    
    // 连接信号和槽
    connect(m_tcpSocket, &QTcpSocket::connected, this, &TcpClientCore::onConnected);
    connect(m_tcpSocket, &QTcpSocket::disconnected, this, &TcpClientCore::onDisconnected);
    connect(m_tcpSocket, &QTcpSocket::readyRead, this, &TcpClientCore::onReadyRead);
    
    // 兼容不同Qt版本的错误信号
    #if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(m_tcpSocket, &QAbstractSocket::errorOccurred, this, &TcpClientCore::onSocketError);
    #else
    connect(m_tcpSocket, static_cast<void(QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error),
            this, &TcpClientCore::onSocketError);
    #endif
}

TcpClientCore::~TcpClientCore()
{
    if (m_tcpSocket) {
        if (m_tcpSocket->state() == QAbstractSocket::ConnectedState) {
            m_tcpSocket->disconnectFromHost();
            m_tcpSocket->waitForDisconnected(1000);
        }
    }
}

// TCP连接接口
bool TcpClientCore::connectToTcp(const QString& localIP, const QString& remoteIP, quint16 remotePort, bool proxyDisabled)
{
    if (!m_tcpSocket) {
        qWarning() << "TCP Socket 未初始化";
        return false;
    }
    
    // 如果已经连接，先断开
    if (m_tcpSocket->state() == QAbstractSocket::ConnectedState) {
        qDebug() << "已经连接，先断开";
        m_tcpSocket->disconnectFromHost();
        m_tcpSocket->waitForDisconnected(1000);
    }
    
    // 设置代理
    if (proxyDisabled) {
        m_tcpSocket->setProxy(QNetworkProxy::NoProxy);
        qDebug() << "网络代理已禁用";
    } else {
        m_tcpSocket->setProxy(QNetworkProxy::DefaultProxy);
        qDebug() << "使用默认网络代理设置";
    }
    
    // 如果指定了本地IP，绑定本地地址
    if (!localIP.isEmpty()) {
        QHostAddress localAddress(localIP);
        if (!localAddress.isNull()) {
            m_tcpSocket->bind(localAddress);
            qDebug() << "绑定本地地址:" << localIP;
        }
    }
    
    // 连接到远程服务器
    qDebug() << "正在连接到" << remoteIP << ":" << remotePort;
    m_tcpSocket->connectToHost(remoteIP, remotePort);
    
    // 等待连接建立（最多3秒）
    if (m_tcpSocket->waitForConnected(3000)) {
        qDebug() << "TCP连接成功";
        return true;
    } else {
        qWarning() << "TCP连接失败:" << m_tcpSocket->errorString();
        return false;
    }
}

// 计算CRC16
quint16 TcpClientCore::calculateCrc16(const QByteArray& data)
{
    quint16 crc = 0xFFFF;  // 初始值
    
    for (int i = 0; i < data.length(); ++i) {
        crc ^= static_cast<quint8>(data[i]);  // XOR字节到CRC
        
        for (int j = 0; j < 8; ++j) {  // 处理8位
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;  // Modbus多项式（反向）
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc;
}

// 发送消息接口
bool TcpClientCore::sendMessage(const QByteArray& content, bool asciiOrHex)
{
    if (!m_tcpSocket) {
        qWarning() << "TCP Socket 未初始化";
        return false;
    }
    
    if (m_tcpSocket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "未连接到服务器，无法发送消息";
        return false;
    }
    
    if (content.isEmpty()) {
        qWarning() << "发送内容为空";
        return false;
    }
    
    QByteArray dataToSend;

    if (asciiOrHex) {
        // ASCII模式：直接发送
        dataToSend = content;
    } else {
        // 十六进制模式：需要从十六进制字符串转换为字节数组
        dataToSend = QByteArray::fromHex(content);
    }
    
    qint64 bytesWritten = m_tcpSocket->write(dataToSend);
    
    if (bytesWritten == -1) {
        qWarning() << "发送失败:" << m_tcpSocket->errorString();
        return false;
    }
    
    m_tcpSocket->flush();
    // qDebug() << "发送成功，字节数:" << bytesWritten;
    
    return true;
}

// 断开连接
void TcpClientCore::disconnectFromTcp()
{
    if (m_tcpSocket && m_tcpSocket->state() == QAbstractSocket::ConnectedState) {
        m_tcpSocket->disconnectFromHost();
        qDebug() << "正在断开连接...";
    }
}

// 检查连接状态
bool TcpClientCore::isConnected() const
{
    return m_tcpSocket && (m_tcpSocket->state() == QAbstractSocket::ConnectedState);
}

// 私有槽函数实现
void TcpClientCore::onConnected()
{
    qDebug() << "TCP连接已建立";
    emit connected();
}

void TcpClientCore::onDisconnected()
{
    qDebug() << "TCP连接已断开";
    emit disconnected();
}

void TcpClientCore::onReadyRead()
{
    if (!m_tcpSocket) {
        return;
    }
    
    QByteArray data = m_tcpSocket->readAll();
    qDebug() << "收到数据:" << " " << QString::fromUtf8(data) << data;
    emit dataReceived(data);
}

void TcpClientCore::onSocketError(QAbstractSocket::SocketError error)
{
    QString errorMsg = m_tcpSocket ? m_tcpSocket->errorString() : "未知错误";
    qWarning() << "TCP Socket错误:" << error << "-" << errorMsg;
    emit errorOccurred(errorMsg);
}

// 构建带CRC的消息
QString TcpClientCore::buildMessageWithCrc(const QString& data)
{
    // 将QString转为QByteArray进行CRC计算
    QByteArray dataBytes = data.toUtf8();
    
    // 直接调用已有的 CRC16 计算函数
    quint16 crc = calculateCrc16(dataBytes);
    
    // 转换为4位十六进制字符串（大写，补0）
    QString crcHex = QString("%1").arg(crc, 4, 16, QChar('0')).toUpper();
    
    // 拼接原始数据和CRC
    QString fullMessage = data + crcHex;
    
    qDebug() << "构建消息:" << data << "-> CRC:" << crcHex << "-> 完整消息:" << fullMessage;
    
    return fullMessage;
}

// 电爪专用：构建带ModBus CRC的消息
QString TcpClientCore::buildGripperMessageWithCrc(const QString& data)
{
    // 将十六进制字符串转换为字节数组
    QByteArray dataBytes = QByteArray::fromHex(data.toUtf8());
    
    // 计算ModBus CRC16（电爪专用）
    quint16 crc = 0xFFFF;  // 初始值
    
    for (int i = 0; i < dataBytes.length(); ++i) {
        crc ^= static_cast<quint8>(dataBytes[i]);  // XOR字节到CRC
        
        for (int j = 0; j < 8; ++j) {  // 处理8位
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;  // ModBus多项式（反向）
            } else {
                crc >>= 1;
            }
        }
    }
    
    // ModBus CRC格式：低字节在前，高字节在后
    QString crcLow = QString("%1").arg(crc & 0xFF, 2, 16, QChar('0')).toUpper();
    QString crcHigh = QString("%1").arg((crc >> 8) & 0xFF, 2, 16, QChar('0')).toUpper();
    QString crcStr = crcLow + crcHigh;
    
    // 拼接原始数据和CRC
    QString fullMessage = data + crcStr;
    
    qDebug() << "电爪消息构建:" << data << "-> CRC:" << ("0x" + crcStr) << "-> 完整消息:" << fullMessage;
    
    return fullMessage;
}

// 构建设备命令（自动添加CRC）- 完整版本
QString TcpClientCore::buildDeviceCommand(const QString& deviceNum, const QString& functionCode, const QString& commandData)
{
    /**
     * xyz电机序号：01 02 03 04 06 08 09 0A
     * 夹爪电机序号：05 0B 0C
     *   移液器序号：07
     */
    
    // 验证设备编号
    if (deviceNum == "01" || deviceNum == "02" || deviceNum == "03" ||
        deviceNum == "04" || deviceNum == "06" || deviceNum == "08" ||
        deviceNum == "09" || deviceNum == "0A")
    {
        // XYZ电机：构建原始命令 > + 设备号 + 功能码 + 数据
        QString rawCommand = ">" + deviceNum + functionCode + commandData;

        // 例如：输入 ">0AA" -> 返回 ">0AAA3FD"
        return buildMessageWithCrc(rawCommand);
    }
    else if(deviceNum == "07")
    {
        // 移液器处理逻辑（待实现）
        QString rawCommand = ">" + deviceNum + functionCode + commandData;
        return buildMessageWithCrc(rawCommand);
    }
    else
    {
        qWarning() << "设备编号错误:" << deviceNum;
        return "";
    }
}

// 电爪专用：构建设备命令（重载版本，4个参数：ID + 功能码 + 寄存器地址 + 寄存器数据）
QString TcpClientCore::buildDeviceCommand(const QString& deviceNum, 
                                          const QString& functionCode, 
                                          const QString& registerAddress, 
                                          const QString& registerData)
{
    // 电爪使用ModBus RTU协议，数据格式：ID + 功能码 + 寄存器地址 + 寄存器数据
    QString rawCommand = deviceNum + functionCode + registerAddress + registerData;
    
    // 使用电爪专用的ModBus CRC计算
    QString result = buildGripperMessageWithCrc(rawCommand);
    
    return result;
}

// 电爪专用：构建设备命令（重载版本，5个参数：支持十进制数据和位宽）
QString TcpClientCore::buildDeviceCommand(const QString& deviceNum, 
                                          const QString& functionCode, 
                                          const QString& registerAddress, 
                                          qint64 decimalData,
                                          int dataWidth)
{
    // 检查数据是否超出范围
    qint64 maxValue = (1LL << (dataWidth * 4)) - 1;  // 例如：4位 = 0xFFFF = 65535
    if (decimalData < 0 || decimalData > maxValue) {
        qWarning() << "数据超出范围！数值:" << decimalData 
                   << "最大值:" << maxValue 
                   << "(" << dataWidth << "位)";
    }
    
    // 将十进制数据转换为指定位宽的十六进制字符串
    QString hexData = QString("%1").arg(decimalData, dataWidth, 16, QChar('0')).toUpper();
    
    qDebug() << "十进制转换:" << decimalData << "-> 十六进制(" << dataWidth << "位):" << hexData;
    
    // 调用4参数版本
    return buildDeviceCommand(deviceNum, functionCode, registerAddress, hexData);
}
