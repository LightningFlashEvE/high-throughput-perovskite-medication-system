#include "tcpclientcore.h"
#include <QHostAddress>
#include <QDebug>
#include <QRegularExpression>
#include <QRegularExpressionMatch>


TcpClientCore::TcpClientCore(QObject *parent)
    : QObject{parent}
    , m_tcpSocket(nullptr)
    , m_pollTimer(nullptr)
    , m_isPolling(false)
    , m_pollEventLoop(nullptr)
    , m_isProcessingQueue(false)
    , m_queueTimer(nullptr)
    , m_isWaitingForResponse(false)
    , m_expectedWeight(0.0)

{
    // 创建 TCP Socket
    m_tcpSocket = new QTcpSocket(this);
}

void TcpClientCore::initializeConnectionsAndTimers()
{
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
    
    // 创建轮询定时器
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(300);  // 0.3秒
    connect(m_pollTimer, &QTimer::timeout, this, &TcpClientCore::pollMotorPosition);
    
    // 创建队列处理定时器（单次触发）
    m_queueTimer = new QTimer(this);
    m_queueTimer->setSingleShot(true);
    connect(m_queueTimer, &QTimer::timeout, this, &TcpClientCore::processMessageQueue);
}










void TcpClientCore::initializeConnectionsForBalance()
{
    // 连接信号和槽（天平专用版本）
    connect(m_tcpSocket, &QTcpSocket::connected, this, &TcpClientCore::onBalanceConnected);
    connect(m_tcpSocket, &QTcpSocket::disconnected, this, &TcpClientCore::onBalanceDisconnected);
    connect(m_tcpSocket, &QTcpSocket::readyRead, this, &TcpClientCore::onBalanceReadyRead);

    // 兼容不同Qt版本的错误信号
    #if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(m_tcpSocket, &QAbstractSocket::errorOccurred, this, &TcpClientCore::onBalanceSocketError);
    #else
    connect(m_tcpSocket, static_cast<void(QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error),
            this, &TcpClientCore::onBalanceSocketError);
    #endif
}

void TcpClientCore::disconnectConnectionsForBalance()
{
    if (!m_tcpSocket) {
        return;
    }
    
    // 断开信号和槽（天平专用版本）
    disconnect(m_tcpSocket, &QTcpSocket::connected, this, &TcpClientCore::onBalanceConnected);
    disconnect(m_tcpSocket, &QTcpSocket::disconnected, this, &TcpClientCore::onBalanceDisconnected);
    disconnect(m_tcpSocket, &QTcpSocket::readyRead, this, &TcpClientCore::onBalanceReadyRead);

    // 兼容不同Qt版本的错误信号
    #if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    disconnect(m_tcpSocket, &QAbstractSocket::errorOccurred, this, &TcpClientCore::onBalanceSocketError);
    #else
    disconnect(m_tcpSocket, static_cast<void(QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error),
               this, &TcpClientCore::onBalanceSocketError);
    #endif
}

void TcpClientCore::connectReceiveForBalance()
{
    if (m_balancePrintEnabled) {
        return;
    }
    m_balancePrintEnabled = true;
    qDebug() << "Balance receive print enabled";
}

void TcpClientCore::disconnectReceiveForBalance()
{
    if (!m_balancePrintEnabled) {
        return;
    }
    m_balancePrintEnabled = false;
    qDebug() << "Balance receive print disabled";
}


TcpClientCore::~TcpClientCore()
{
    // 停止轮询
    stopPolling();
    
    // 如果事件循环正在运行，先退出它
    if (m_pollEventLoop && m_pollEventLoop->isRunning()) {
        m_pollEventLoop->quit();
    }
    
    // 停止队列处理
    if (m_queueTimer) {
        m_queueTimer->stop();
    }
    m_messageQueue.clear();
    m_isProcessingQueue = false;
    m_isWaitingForResponse = false;
    
    // 断开TCP连接（非阻塞方式）
    if (m_tcpSocket) {
        if (m_tcpSocket->state() == QAbstractSocket::ConnectedState) {
            m_tcpSocket->disconnectFromHost();
            // 不等待，避免阻塞析构
            // 使用 QTimer 延迟删除，让事件循环处理断开
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
    const QAbstractSocket::SocketState currentState = m_tcpSocket->state();
    if (currentState == QAbstractSocket::ConnectedState || currentState == QAbstractSocket::ClosingState) {
        qDebug() << "已经连接，先断开";
        m_tcpSocket->disconnectFromHost();

        if (m_tcpSocket->state() != QAbstractSocket::UnconnectedState) {
            if (!m_tcpSocket->waitForDisconnected(1000)) {
                qWarning() << "等待上一次连接断开超时，强制中止";
                m_tcpSocket->abort();
            }
        }
    } else if (currentState != QAbstractSocket::UnconnectedState) {
        // 处于其他状态（比如 Connecting），直接强制中止，避免 waitForDisconnected 警告
        qWarning() << "检测到异常状态" << currentState << "，强制中止";
        m_tcpSocket->abort();
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

// 直接发送命令，不经过队列
void TcpClientCore::writeBalanceTareCommand(const QString& data, int mode)
{
    if (!m_tcpSocket) {
        qWarning() << "TCP Socket 未初始化";
        return;
    }
    
    if (m_tcpSocket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "未连接到服务器，无法发送命令";
        return;
    }
    
    if (data.isEmpty()) {
        qWarning() << "发送内容为空";
        return;
    }
    
    // 数据格式转换
    QByteArray dataToSend;
    switch (mode) {
        case StringMode:  // 字符串模式（默认）：将 QString 转换为 UTF-8 字节数组
            dataToSend = data.toUtf8();
            break;
        case AsciiMode:  // ASCII模式：将 QString 转换为 Latin-1 字节数组
            dataToSend = data.toLatin1();
            break;
        case HexMode:    // 十六进制模式：从十六进制字符串转换为字节
            dataToSend = QByteArray::fromHex(data.toUtf8());
            break;
        default:
            qWarning() << "未知的发送模式，使用默认字符串模式";
            dataToSend = data.toUtf8();
            break;
    }
    
    // 通过TCP发送数据，检查发送结果
    qint64 bytesWritten = m_tcpSocket->write(dataToSend);
    
    if (bytesWritten == -1) {
        qWarning() << "发送失败:" << m_tcpSocket->errorString();
        return;
    }
    
    m_tcpSocket->flush();
    qDebug() << "发送成功，字节数:" << bytesWritten;
}

// 设置期望重量值（用于称重对比）
void TcpClientCore::setExpectedWeight(double weight)
{
    m_expectedWeight = weight;
    qDebug() << QString("设置期望重量值：%1g").arg(weight);
}

// 获取期望重量值
double TcpClientCore::getExpectedWeight() const
{
    return m_expectedWeight;
}

// 清除期望重量值（恢复为0）
void TcpClientCore::clearExpectedWeight()
{
    m_expectedWeight = 0.0;
    qDebug() << "已清除期望重量值";
}

// 发送消息接口
bool TcpClientCore::sendMessage(const QByteArray& content, bool asciiOrHex)
{
    /*
     * ============================
     * 1. 参数和状态验证
     * ============================
     * 检查 TCP Socket 是否初始化
     * 检查连接状态
     * 检查发送内容是否为空
     */
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
    
    /*
     * ============================
     * 2. 检测是否需要轮询
     * ============================
     * 情况1：XYZ电机查询到位命令（ASCII模式）
     *   格式：>06d + 4位CRC 或 >0Ad + 4位CRC
     * 情况2：电爪初始化状态查询（Hex模式，ModBus RTU）
     *   格式：05 03 02 00 00 01 + CRC (Hex字符串)
     */
    QString contentStr = QString::fromUtf8(content);
    bool isPositionQuery = false;
    bool isGripperInitQuery = false;
    QString deviceNum;
    
    if (asciiOrHex && contentStr.startsWith(">") && contentStr.length() >= 4) {
        // ASCII模式：XYZ电机查询到位
        QChar functionCode = contentStr.at(3);  // 提取功能码（第4个字符，索引为3）
        
        if (functionCode == 'd') {
            isPositionQuery = true;
            deviceNum = contentStr.mid(1, 2);  // 提取设备编号（位置1-2）
            qDebug() << "检测到查询到位命令，设备编号:" << deviceNum  << contentStr;
        }
    } else if (!asciiOrHex && contentStr.length() >= 12) {
        // Hex模式：可能是电爪初始化查询
        // 格式：设备号(2位) + 03(读取) + 0200(寄存器地址) + 0001(数量) + CRC(4位)
        // 示例：05 03 02 00 00 01 85 B2 -> "05030200000185B2"
        QString functionCode = contentStr.mid(2, 2);    // 功能码（位置2-3）
        QString registerAddr = contentStr.mid(4, 4);    // 寄存器地址（位置4-7）
        
        if (functionCode.toUpper() == "03" && (registerAddr.toUpper() == "0200" || registerAddr.toUpper() == "0201" || registerAddr.toUpper() == "0202" )) {
            isGripperInitQuery = true;
            deviceNum = contentStr.left(2);  // 提取设备编号（前2位）
            qDebug() << "检测到电爪初始化查询命令，设备编号:" << deviceNum;
        }
    }
    
    /*
     * ============================
     * 3. 数据格式转换
     * ============================
     * ASCII模式（asciiOrHex = true）：直接发送字节
     * Hex模式（asciiOrHex = false）：先从十六进制字符串转换为字节数组
     */
    QByteArray dataToSend;
    
    if (asciiOrHex) {
        dataToSend = content;  // ASCII模式：直接发送
    } else {
        dataToSend = QByteArray::fromHex(content);  // 十六进制模式：转换后发送
    }
    
    /*
     * ============================
     * 4. 通过TCP发送数据
     * ============================
     * 写入Socket，检查发送结果，刷新缓冲区
     */
    qint64 bytesWritten = m_tcpSocket->write(dataToSend);
    
    if (bytesWritten == -1) {
        qWarning() << "发送失败:" << m_tcpSocket->errorString();
        return false;
    }
    
    m_tcpSocket->flush();
    // qDebug() << "发送成功，字节数:" << bytesWritten;
    
    /*
     * ============================
     * 5. 轮询机制
     * ============================
     * 情况1：XYZ电机查询到位
     * 情况2：电爪初始化状态查询
     * 共同流程：
     *   - 启动0.3秒轮询定时器（pollMotorPosition）
     *   - 创建事件循环阻塞等待
     *   - 收到目标响应后（在 onReadyRead 中），退出循环继续执行
     */
    if (isPositionQuery || isGripperInitQuery) {
        if (isPositionQuery) {
            qDebug() << "(ง •_•)ง启动轮询，等待设备" << deviceNum << "到位...";
        } else if (isGripperInitQuery) {
            qDebug() << "(ง •_•)ง启动轮询，等待设备" << deviceNum << "电爪初始化完成...";
        }
        
        // 启动轮询定时器
        startPolling(deviceNum, contentStr);
        
        // 创建事件循环，阻塞等待到位信号
        if (!m_pollEventLoop) {
            m_pollEventLoop = new QEventLoop(this);
        }
        
        qDebug() << "进入阻塞等待...";
        m_pollEventLoop->exec();  // 阻塞在这里，直到收到目标响应（在 onReadyRead 中退出）
        qDebug() << "退出阻塞，Start the next step\n\n\n";
    }
    
    return true;
}

// 异步发送消息（队列方式）
void TcpClientCore::sendMessageAsync(const QByteArray& content, bool asciiOrHex)
{
    // 检查连接状态
    if (!m_tcpSocket || m_tcpSocket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "未连接到服务器，无法发送消息";
        return;
    }
    
    // 检查发送内容是否为空
    if (content.isEmpty()) {
        qWarning() << "发送内容为空";
        return;
    }
    
    // 如果定时器未初始化，自动初始化（向后兼容）
    if (!m_queueTimer) {
        initializeConnectionsAndTimers();
    }
    
    // 将消息加入队列
    MessageQueueItem item(content, asciiOrHex);
    m_messageQueue.enqueue(item);
    
    // 如果当前没有在处理队列，启动处理
    if (!m_isProcessingQueue) {
        // 参数0表示定时器将在0毫秒后超时，即在事件循环的下一个迭代中"立即"触发超时事件
        m_queueTimer->start(0);  // 立即触发   
    }
    else
    {
        qDebug() << "有在处理队列";
    }
}

// 异步发送消息（队列方式）- 统一期望接收值（设备+指令+数据；未知可传 "-----"）
void TcpClientCore::sendMessageAsync(const QByteArray& content, bool asciiOrHex, const QString& expectedSignature)
{
    // 基础校验保持一致
    if (!m_tcpSocket || m_tcpSocket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "未连接到服务器，无法发送消息";
        return;
    }
    if (content.isEmpty()) {
        qWarning() << "发送内容为空";
        return;
    }

    // 如果定时器未初始化，自动初始化（向后兼容）
    if (!m_queueTimer) {
        initializeConnectionsAndTimers();
    }

    MessageQueueItem item(content, asciiOrHex, expectedSignature);
    m_messageQueue.enqueue(item);

    if (!m_isProcessingQueue) {
        m_queueTimer->start(0);
    }

}

// 断开连接
void TcpClientCore::disconnectFromTcp()
{
    qDebug() << "开始断开TCP连接并清理资源...";
    
    // 1. 停止轮询
    stopPolling();
    
    // 2. 如果事件循环正在运行，先退出它
    if (m_pollEventLoop && m_pollEventLoop->isRunning()) {
        qDebug() << "退出事件循环...";
        m_pollEventLoop->quit();
    }
    
    // 3. 停止队列处理定时器
    if (m_queueTimer) {
        m_queueTimer->stop();
    }
    
    // 4. 清空消息队列
    if (!m_messageQueue.isEmpty()) {
        qDebug() << "清空消息队列，未发送消息数:" << m_messageQueue.size();
        m_messageQueue.clear();
    }
    
    // 5. 重置状态标志
    m_isProcessingQueue = false;
    m_isWaitingForResponse = false;
    
    // 6. 断开TCP连接
    if (m_tcpSocket) {
        const QAbstractSocket::SocketState state = m_tcpSocket->state();
        if (state == QAbstractSocket::ConnectedState || state == QAbstractSocket::ClosingState) {
            qDebug() << "正在断开TCP连接...";
            m_tcpSocket->disconnectFromHost();

            if (m_tcpSocket->state() != QAbstractSocket::UnconnectedState) {
                if (!m_tcpSocket->waitForDisconnected(1000)) {
                    qWarning() << "TCP断开超时，强制中止连接";
                    m_tcpSocket->abort();
                }
            }
        } else if (state != QAbstractSocket::UnconnectedState) {
            qWarning() << "TCP处于状态" << state << "，直接强制中止连接";
            m_tcpSocket->abort();
        }
    }
    
    qDebug() << "TCP连接已断开，资源清理完成";
}

// 检查连接状态
bool TcpClientCore::isConnected() const
{
    return m_tcpSocket && (m_tcpSocket->state() == QAbstractSocket::ConnectedState);
}

// 仅复位内部状态（不主动断开连接）
void TcpClientCore::resetState()
{
    // 停止轮询并清空相关状态
    stopPolling();

    // 停止队列处理定时器
    if (m_queueTimer) {
        m_queueTimer->stop();
    }

    // 清空消息队列
    if (!m_messageQueue.isEmpty()) {
        m_messageQueue.clear();
    }

    // 复位标志
    m_isProcessingQueue = false;
    m_isWaitingForResponse = false;
    m_currentExpectedNormalized.clear();
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

/**
 *
 *
 * 接收内容
 *
 *
 *
 *
 */
void TcpClientCore::onReadyRead()
{
    if (!m_tcpSocket) {
        return;
    }
    
    QByteArray data = m_tcpSocket->readAll();

    qDebug() << "<<<<<<<<收到数据:" << QString::fromUtf8(data) << " " << QString(data.toHex().toUpper());
    
    // 如果正在轮询，检查是否收到目标响应
    if (m_isPolling) {
        bool conditionMet = false;

        // 优先：按期望片段包含匹配（若提供）
        if (!m_currentExpectedNormalized.isEmpty()) {
            const QString plain = QString::fromUtf8(data);
            const QString hexUpper = QString(data.toHex().toUpper());

            if (m_currentAsciiMode) {
                // ASCII：包含匹配（大小写敏感，d/D 区分）
                if (plain.contains(m_currentExpectedNormalized)) {
                    conditionMet = true;
                }
            } else {
                // HEX：包含匹配（使用大写）
                if (hexUpper.contains(m_currentExpectedNormalized)) {
                    conditionMet = true;
                }
            }
        }

        // 兼容旧逻辑：若没提供期望，按既有的到位判断
        if (!conditionMet && m_currentExpectedNormalized.isEmpty()) {
            // 检查1：XYZ电机到位响应
            if (checkIfReachedPosition(data)) {
                conditionMet = true;
            }
            // 检查2：电爪初始化完成响应
            else if (checkIfGripperInitialized(data)) {
                conditionMet = true;
            }
        }
        
        if (conditionMet) {
            qDebug() << "匹配成功，匹配成功的命令。" << data;
            qDebug() << "结束轮询。";
            // 停止轮询
            stopPolling();
            m_currentExpectedNormalized.clear();
            
            // 发射到位信号
            emit motorReachedPosition(m_pollingDeviceNum); // 预留
            
            // 如果有事件循环在等待，退出它
            if (m_pollEventLoop && m_pollEventLoop->isRunning()) {
                m_pollEventLoop->quit();
            }
            
            // 通知队列处理：响应已收到
            if (m_isWaitingForResponse) {
                m_isWaitingForResponse = false;
                // 继续处理队列
                QTimer::singleShot(0, this, &TcpClientCore::processMessageQueue);
            }
        }
    }
    
    // 解析"06E"命令返回的Z轴坐标
    QString dataStr = QString::fromUtf8(data);
    if (dataStr.startsWith("06E", Qt::CaseInsensitive) || dataStr.startsWith(">06E", Qt::CaseInsensitive)) {
        // 提取"06E"后面的8位十六进制坐标
        // 格式可能是：">06E12345678XXXX" 或 "06E12345678XXXX"（XXXX是CRC）
        QString coordinateStr;
        
        if (dataStr.startsWith(">06E", Qt::CaseInsensitive)) {
            // 如果以">06E"开头，跳过">06E"（4个字符），取8位坐标
            if (dataStr.length() >= 12) {  // ">06E" + 8位坐标 + 至少4位CRC
                coordinateStr = dataStr.mid(4, 8);  // 从索引4开始取8个字符
            }
        } else if (dataStr.startsWith("06E", Qt::CaseInsensitive)) {
            // 如果以"06E"开头，跳过"06E"（3个字符），取8位坐标
            if (dataStr.length() >= 11) {  // "06E" + 8位坐标 + 至少4位CRC
                coordinateStr = dataStr.mid(3, 8);  // 从索引3开始取8个字符
            }
        }
        
        if (!coordinateStr.isEmpty()) {
            // 将8位十六进制字符串转换为整数坐标
            bool ok;
            int zCoordinate = coordinateStr.toInt(&ok, 16);  // 16进制转10进制
            
            if (ok) {
                qDebug() << "解析到6号电机Z轴坐标:" << zCoordinate << "(十六进制:" << coordinateStr << ")";
                
                // 发出信号通知坐标已更新
                emit z6CoordinateReceived(zCoordinate);
            } else {
                qWarning() << "无法解析坐标字符串:" << coordinateStr;
            }
        } else {
            qWarning() << "响应数据格式不正确，无法提取坐标。数据:" << dataStr;
        }
    }
    
    emit dataReceived(data);
}

void TcpClientCore::onSocketError(QAbstractSocket::SocketError error)
{
    QString errorMsg = m_tcpSocket ? m_tcpSocket->errorString() : "未知错误";
    qWarning() << "TCP Socket错误:" << error << "-" << errorMsg;
    emit errorOccurred(errorMsg);
}

// ========== 天平专用槽函数 ==========

void TcpClientCore::onBalanceConnected()
{
    qDebug() << "天平TCP连接已建立";
    emit connected();
}

void TcpClientCore::onBalanceDisconnected()
{
    qDebug() << "天平TCP连接已断开";
    emit disconnected();
}

/**
 * 天平数据接收处理
 * 天平版本简化处理，不需要轮询机制，直接接收数据并发出信号
 */
void TcpClientCore::onBalanceReadyRead()
{
    if (!m_tcpSocket) {
        return;
    }

    QByteArray data = m_tcpSocket->readAll();
    QString dataStr = QString::fromUtf8(data);
    //qDebug() << "天平收到数据:" << dataStr << QString(data.toHex().toUpper());

    // 正则表达式：匹配数字（包括负数、浮动点数字）
    // 支持格式：22.1074 g、-22.1074 g、N     -  22.1074 g 等
    // 允许符号和数字之间有空格
    QRegularExpression regex("([+-]?)\\s*(\\d+\\.?\\d*)\\s*"); // g已经删除，后期需要保留
    QRegularExpressionMatchIterator iter = regex.globalMatch(dataStr);

    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        QString sign = match.captured(1);      // 获取符号（+、- 或空）
        QString numberStr = match.captured(2); // 获取数字部分
        QString weightStr = sign + numberStr;   // 组合符号和数字
        
        bool ok;
        double weight = weightStr.toDouble(&ok);

        if (ok) {
            if (m_balancePrintEnabled) {
                qDebug() << QString("----- %1g / %2g -----").arg(weight).arg(m_expectedWeight);
            }
            
            // 如果设置了期望重量值（不为0），进行对比
            if (m_expectedWeight != 0.0) {
                if (m_balancePrintEnabled) {
                    //qDebug() << "当前重量：" << weight << "期望重量：" << m_expectedWeight;
                }
                
                // 判断当前值是否大于等于期望值
                if (weight >= m_expectedWeight) {
                    if (m_balancePrintEnabled) {
                        qDebug() << "重量达标！！！！发送命令 >01K0EE65" << "当前重量：" << weight << "期望重量：" << m_expectedWeight;
                    }
                    // 发送信号 重量达标带重量值 重量值
                    emit weightReached(weight);

                } else {
                    if (m_balancePrintEnabled) {
                        //qDebug() << QString("重量未达标，当前值 %1g 小于期望值 %2g").arg(weight).arg(m_expectedWeight);
                    }
                }

            }
            else
            {
                // if (m_balancePrintEnabled) {
                //     qDebug() << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" << m_balancePrintEnabled << m_expectedWeight;
                // }
            }
        } else {
            if (m_balancePrintEnabled) {
                qDebug() << "天平无法解析的重量值：" << weightStr;
            }
        }
    }
}

void TcpClientCore::onBalanceSocketError(QAbstractSocket::SocketError error)
{
    QString errorMsg = m_tcpSocket ? m_tcpSocket->errorString() : "未知错误";
    qWarning() << "天平TCP Socket错误:" << error << "-" << errorMsg;
    emit errorOccurred(errorMsg);
}

// 将整数转换为十六进制字符串（支持负数处理）
QString TcpClientCore::int2hex(qint64 value, int width)
{
    QString hexData;
    
    // 处理负数：如果 value < 0，需要特殊处理
    if (value < 0) {
        // 负数处理：先-1，然后转16进制，再取反
        qint64 absValue = -value;  // 取绝对值
        qint64 processedValue = absValue - 1;     // -1
        
        // 取反（二进制取反）
        qint64 mask = (1LL << (width * 4)) - 1;  // 根据位宽确定掩码
        qint64 invertedValue = (~processedValue) & mask;
        
        // 转换回十六进制，保持指定位宽
        hexData = QString("%1").arg(invertedValue, width, 16, QChar('0')).toUpper();
    } else {
        // 正数：直接转换为指定位宽的十六进制字符串
        hexData = QString("%1").arg(value, width, 16, QChar('0')).toUpper();
    }
    
    return hexData;
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
    
    qDebug() << "构建消息:" << data << "+" << crcHex << "完整消息:" << fullMessage;
    
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
    
    qDebug() << "构建消息:" << data << "-> CRC:" << ("0x" + crcStr) << "-> 完整消息:" << fullMessage;
    
    return fullMessage;
}











// 构建设备命令（重载版本，接受数字和位数）
QString TcpClientCore::buildDeviceCommand(const QString& deviceNum, const QString& functionCode, qint64 commandData, int width)
{
    /**
     * xyz电机序号：01 02 03 04 06 08 09 0A
     * 夹爪电机序号：05 0B 0C
     *   移液器序号：07
     */
    
    // 如果位数为0，命令数据为空字符串
    QString processedCommandData;
    if (width == 0) {
        processedCommandData = "";
    } else {
        // 使用 int2hex 函数将数字转换为十六进制字符串（自动处理负数）
        processedCommandData = int2hex(commandData, width);
    }
    
    // 验证设备编号并构建命令
    if (deviceNum == "01" || deviceNum == "02" || deviceNum == "03" ||
        deviceNum == "04" || deviceNum == "06" || deviceNum == "08" ||
        deviceNum == "09" || deviceNum == "0A")
    {
        // XYZ电机：构建原始命令 > + 设备号 + 功能码 + 数据
        QString rawCommand = ">" + deviceNum + functionCode + processedCommandData;

        // 例如：输入 ">0AA" -> 返回 ">0AAA3FD"
        return buildMessageWithCrc(rawCommand);
    }
    else if(deviceNum == "07" || deviceNum == "05")
    {
        // 移液器处理逻辑（待实现）
        QString rawCommand = ">" + deviceNum + functionCode + processedCommandData;
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
    // 如果位宽为0，忽略 decimalData 参数，只使用 registerAddress
    if (dataWidth == 0) {
        return buildDeviceCommand(deviceNum, functionCode, registerAddress, "");
    }
    
    // 使用 int2hex 函数将数字转换为十六进制字符串（自动处理负数）
    QString hexData = int2hex(decimalData, dataWidth);
    
    // 检查正数是否超出范围
    if (decimalData >= 0) {
        qint64 maxValue = (1LL << (dataWidth * 4)) - 1;  // 例如：4位 = 0xFFFF = 65535
        if (decimalData > maxValue) {
            qWarning() << "数据超出范围！数值:" << decimalData 
                       << "最大值:" << maxValue 
                       << "(" << dataWidth << "位)";
        }
    }
    
    // qDebug() << "检查各个元素:" << deviceNum << functionCode << registerAddress << hexData;

    // 调用4参数版本
    return buildDeviceCommand(deviceNum, functionCode, registerAddress, hexData);
}














// ========== 轮询机制相关函数 ==========

// 启动轮询机制
void TcpClientCore::startPolling(const QString& deviceNum, const QString& command)
{
    if (m_isPolling) {
        qWarning() << "已经在轮询中，无法启动新的轮询";
        return;
    }
    
    // 如果定时器未初始化，自动初始化（向后兼容）
    if (!m_pollTimer) {
        initializeConnectionsAndTimers();
    }
    
    m_isPolling = true;
    m_pollingDeviceNum = deviceNum;
    m_pollingCommand = command;
    
    // 启动定时器（不打印日志，避免刷屏）
    m_pollTimer->start();
}

// 停止轮询机制
void TcpClientCore::stopPolling()
{
    if (!m_isPolling) {
        return;
    }
    
    m_isPolling = false;
    m_pollTimer->stop();
    m_pollingDeviceNum.clear();
    m_pollingCommand.clear();
    
    // 如果有事件循环正在运行，退出它
    if (m_pollEventLoop && m_pollEventLoop->isRunning()) {
        m_pollEventLoop->quit();
    }
    
    // 不打印日志，避免刷屏
}

// 轮询定时器槽函数
void TcpClientCore::pollMotorPosition()
{
    if (!m_isPolling || m_pollingCommand.isEmpty()) {
        return;
    }
    
    // 不打印轮询发送信息，避免每0.3秒刷屏
    
    /*
     * 根据命令格式判断发送模式：
     * - 以 ">" 开头：XYZ电机（ASCII模式，直接发送）
     * - 不以 ">" 开头：电爪ModBus（Hex模式，需要转换）
     */
    QByteArray dataToSend;
    
    if (m_pollingCommand.startsWith(">")) {
        // ASCII模式：XYZ电机，直接发送
        dataToSend = m_pollingCommand.toUtf8();
    } else {
        // Hex模式：电爪ModBus，先从十六进制字符串转换为字节数组
        dataToSend = QByteArray::fromHex(m_pollingCommand.toUtf8());
    }
    
    if (m_tcpSocket && m_tcpSocket->state() == QAbstractSocket::ConnectedState) {
        m_tcpSocket->write(dataToSend);
        // qDebug() << "轮询发送:" << QString(dataToSend.toHex().toUpper());
        m_tcpSocket->flush();
    } else {
        qWarning() << "TCP未连接，停止轮询";
        stopPolling();
        
        if (m_pollEventLoop && m_pollEventLoop->isRunning()) {
            m_pollEventLoop->quit();
        }
    }
}

// XYZ电机否是到位响应
bool TcpClientCore::checkIfReachedPosition(const QByteArray& data)
{
    // 将接收到的数据转换为字符串
    QString dataStr = QString::fromUtf8(data);
    
    // 期望格式：>06d01 + 4位CRC  或  >0Ad01 + 4位CRC
    // 其中 01 表示已到达预定位置
    
    // 检查数据长度是否合理（至少需要 >XXdYY + ZZZZ = 11个字符）
    if (dataStr.length() < 11) {
        return false;
    }
    
    // 检查是否以 > 开头
    if (!dataStr.startsWith(">")) {
        return false;
    }
    
    // 提取设备编号（位置1-2）
    QString deviceNum = dataStr.mid(1, 2);
    
    // 检查设备编号是否匹配
    if (deviceNum.toUpper() != m_pollingDeviceNum.toUpper()) {
        return false;
    }
    
    // 检查功能码是否是 'd'
    QChar functionCode = dataStr.at(3);
    if (functionCode != 'd') {
        return false;
    }
    
    // 提取状态码（位置4-5）
    QString statusCode = dataStr.mid(4, 2);
    
    // 状态码 01 表示到达预定位置
    if (statusCode == "01") {
        return true;
    }
    
    // 其他状态码的含义（不打印，避免刷屏）：
    // 00: 运行中
    // 02: 异常撞击
    // 03: 检测到液位
    // 04: 未检测到液位到达最大距离
    // 05: 力矩模式下运行到极限位置
    // 08: 紧急停止
    
    return false;
}

// 电爪否是到位响应
bool TcpClientCore::checkIfGripperInitialized(const QByteArray& data)
{
    /*
     * ModBus RTU 响应格式：
     * 字节0: 设备ID（如 0x05）
     * 字节1: 功能码（0x03 = 读取保持寄存器）
     * 字节2: 字节计数（0x02 = 2字节数据）
     * 字节3-4: 数据（0x00 0x01 = 初始化完成）
     * 字节5-6: CRC16校验
     * 
     * 示例：05 03 02 00 01 79 84
     */
    
    // 检查数据长度（至少7字节：ID + FC + BC + 2字节数据 + 2字节CRC）
    if (data.length() < 7) {
        return false;
    }
    
    // 提取设备ID（字节0）
    quint8 deviceId = static_cast<quint8>(data[0]);
    QString deviceIdStr = QString("%1").arg(deviceId, 2, 16, QChar('0')).toUpper();
    
    // 检查设备ID是否匹配
    if (deviceIdStr != m_pollingDeviceNum.toUpper()) {
        return false;
    }
    
    // 检查功能码（字节1）是否是 0x03
    quint8 functionCode = static_cast<quint8>(data[1]);
    if (functionCode != 0x03) {
        return false;
    }
    
    // 检查字节计数（字节2）是否是 0x02
    quint8 byteCount = static_cast<quint8>(data[2]);
    if (byteCount != 0x02) {
        return false;
    }
    
    // 提取状态数据（字节3-4）
    quint16 statusData = (static_cast<quint8>(data[3]) << 8) | static_cast<quint8>(data[4]);
    if (statusData == 0x0001 || statusData == 0x0002) {
        return true;
    }


    
    // 如果是其他状态，可以打印调试信息
    // qDebug() << "电爪状态:" << QString("0x%1").arg(statusData, 4, 16, QChar('0')).toUpper();
    
    return false;
}

// 处理消息队列
void TcpClientCore::processMessageQueue()
{
    // 如果队列为空，停止处理
    if (m_messageQueue.isEmpty()) {
        m_isProcessingQueue = false;
        return;
    }
    
    // 如果正在等待响应，不继续处理
    if (m_isWaitingForResponse) {
        return;
    }
    
    // 标记为正在处理
    m_isProcessingQueue = true;
    
    // 取出队列中的第一条消息
    MessageQueueItem item = m_messageQueue.dequeue();
    
    // 检查是否是AA0、AA1或AA2命令（天平命令）
    QString contentStr = QString::fromUtf8(item.content);
    if (item.asciiOrHex && contentStr == "AA0") {
        // 检测到AA0命令（关闭天平打印），不发送，而是发出信号
        qDebug() << "检测到AA0命令，发出天平打印关闭信号";
        emit balancePrintOffRequested();
        // 继续处理下一条消息
        m_isProcessingQueue = false;
        QTimer::singleShot(0, this, &TcpClientCore::processMessageQueue);
        return;
    }
    if (item.asciiOrHex && contentStr == "AA1") {
        // 检测到AA1命令（打开天平打印），不发送，而是发出信号
        qDebug() << "检测到AA1命令，发出天平打印打开信号";
        emit balancePrintOnRequested();
        // 继续处理下一条消息
        m_isProcessingQueue = false;
        QTimer::singleShot(0, this, &TcpClientCore::processMessageQueue);
        return;
    }
    if (item.asciiOrHex && contentStr == "AA2") {
        // 检测到AA2命令（天平去皮），不发送，而是发出信号
        qDebug() << "检测到AA2命令，发出天平去皮信号";
        emit balanceTareRequested();
        // 继续处理下一条消息
        m_isProcessingQueue = false;
        QTimer::singleShot(0, this, &TcpClientCore::processMessageQueue);
        return;
    }
    /*
    * AA0  天平打印关
    * AA1  天平打印开
    * AA2  天平去皮
    * AAcloseShakeBed 关摇床
    * AAopenShakeBed  开摇床
    * AArecordShakeBedTime     记录摇床需要的时间
    * 第二步骤
    */
    if (item.asciiOrHex && contentStr == "AArecordShakeBedTime") {
        // 检测到AArecordShakeBedTime命令（记录摇床时间），不发送，而是发出信号
        qDebug() << "检测到AArecordShakeBedTime命令，发出记录摇床时间信号";
        emit recordShakeBedTimeRequested();
        // 继续处理下一条消息
        m_isProcessingQueue = false;
        QTimer::singleShot(0, this, &TcpClientCore::processMessageQueue);
        return;
    }
    if (item.asciiOrHex && contentStr == "AAopenShakeBed") {
        // 检测到AAopenShakeBed命令（启动摇床），不发送，而是发出信号
        qDebug() << "检测到AAopenShakeBed命令，发出启动摇床信号";
        emit openShakeBedRequested();
        // 继续处理下一条消息
        m_isProcessingQueue = false;
        QTimer::singleShot(0, this, &TcpClientCore::processMessageQueue);
        return;
    }
    if (item.asciiOrHex && contentStr == "AAcloseShakeBed") {
        // 检测到AAcloseShakeBed命令（关闭摇床），不发送，而是发出信号
        qDebug() << "检测到AAcloseShakeBed命令，发出关闭摇床信号";
        emit closeShakeBedRequested();
        // 继续处理下一条消息
        m_isProcessingQueue = false;
        QTimer::singleShot(0, this, &TcpClientCore::processMessageQueue);
        return;
    }
    if (item.asciiOrHex && contentStr == "AAemptyBottleAreaCurrentIndexPlusOne") {
        // 检测到AAemptyBottleAreaCurrentIndexPlusOne命令（空瓶区currentIndex加1），不发送，而是发出信号
        qDebug() << "检测到AAemptyBottleAreaCurrentIndexPlusOne命令，发出空瓶区currentIndex加1信号";
        emit emptyBottleAreaCurrentIndexPlusOneRequested();
        // 继续处理下一条消息
        m_isProcessingQueue = false;
        QTimer::singleShot(0, this, &TcpClientCore::processMessageQueue);
        return;
    }
    if (item.asciiOrHex && contentStr == "AAtipsHeadAreaCurrentIndexPlusOne") {
        // 检测到AAtipsHeadAreaCurrentIndexPlusOne命令（tips头区currentIndex加1），不发送，而是发出信号
        qDebug() << "检测到AAtipsHeadAreaCurrentIndexPlusOne命令，发出tips头区currentIndex加1信号";
        emit tipsHeadAreaCurrentIndexPlusOneRequested();
        // 继续处理下一条消息
        m_isProcessingQueue = false;
        QTimer::singleShot(0, this, &TcpClientCore::processMessageQueue);
        return;
    }

    // 检查是否需要等待响应（优先依据期望接收值，"-----" 或 空 表示不等待）
    bool needsWait = (!item.expectedSignature.isEmpty() && item.expectedSignature != "-----")
                     ? true
                     : needsWaitForResponse(item.content, item.asciiOrHex);
    
    if (needsWait) {
        // 需要等待响应（如查询到位命令），设置等待标志
        m_isWaitingForResponse = true;

        // 使用简化匹配：保存用户提供的期望片段（ASCII原样、HEX转大写）
        m_currentExpectedNormalized.clear();
        m_currentAsciiMode = item.asciiOrHex;

        QString contentStr = QString::fromUtf8(item.content);
        QString deviceNum;
        if (item.asciiOrHex && contentStr.startsWith(">") && contentStr.length() >= 4) {
            deviceNum = contentStr.mid(1, 2);
        } else if (!item.asciiOrHex && contentStr.length() >= 2) {
            deviceNum = contentStr.left(2);
        }

        const QString& sig = item.expectedSignature;
        if (!sig.isEmpty() && sig != "-----") {
            if (item.asciiOrHex) {
                // ASCII：保留用户提供片段，大小写敏感匹配
                m_currentExpectedNormalized = sig;
            } else {
                // HEX：将片段转为大写，进行HEX包含匹配
                m_currentExpectedNormalized = sig.toUpper();
            }
        }

        // 打印：当前发送命令 与 期待回复
        QString contentStrLog = QString::fromUtf8(item.content);
        qDebug() << "当前发送的命令是:" << contentStrLog << "，期待回复:" << (sig.isEmpty() || sig == "-----" ? "" : sig);
    }
    
    // 在发送消息之前，非阻塞延时40ms
    QTimer::singleShot(40, this, [=, item = item, needsWait = needsWait]() {

        //qDebug() << "读取队列信息:" << QString::fromUtf8(item.content) << "，期待回复:" << item.expectedSignature;
        // 发送消息（使用非阻塞方式）
        sendMessageInternal(item.content, item.asciiOrHex, needsWait);
        
        // 如果不需要等待响应，立即处理下一条消息
        if (!needsWait) {
            // 递归处理下一条消息（通过定时器异步调用，避免阻塞）
            QTimer::singleShot(0, this, &TcpClientCore::processMessageQueue);
        }
        // 如果需要等待响应，会在 onReadyRead 中接收到响应后继续处理
    });
}

// 检查消息是否需要等待响应
bool TcpClientCore::needsWaitForResponse(const QByteArray& content, bool asciiOrHex)
{
    QString contentStr = QString::fromUtf8(content);
    
    // ASCII模式：查询命令（'d' 功能码）需要等待
    if (asciiOrHex && contentStr.startsWith(">") && contentStr.length() >= 4) {
        QChar functionCode = contentStr.at(3);
        if (functionCode == 'd') {
            return true;
        }
    }
    // Hex模式：电爪初始化查询（功能码 03 + 寄存器 0200/0201/0202）需要等待
    else if (!asciiOrHex && contentStr.length() >= 12) {
        QString functionCode = contentStr.mid(2, 2);
        QString registerAddr = contentStr.mid(4, 4);
        
        if (functionCode.toUpper() == "03" && 
            (registerAddr.toUpper() == "0200" || registerAddr.toUpper() == "0201" || registerAddr.toUpper() == "0202")) {
            return true;
        }
    }
    
    return false;
}

// 发送消息内部实现（非阻塞版本）
void TcpClientCore::sendMessageInternal(const QByteArray& content, bool asciiOrHex, bool shouldWait)
{
    // 检查连接状态
    if (!m_tcpSocket || m_tcpSocket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "未连接到服务器，无法发送消息";
        return;
    }
    
    // 数据格式转换
    QByteArray dataToSend;
    
    if (asciiOrHex) {
        dataToSend = content;  // ASCII模式：直接发送
    } else {
        dataToSend = QByteArray::fromHex(content);  // 十六进制模式：转换后发送
    }
    
    // 通过TCP发送数据
    qint64 bytesWritten = m_tcpSocket->write(dataToSend);
    
    if (bytesWritten == -1) {
        qWarning() << "发送失败:" << m_tcpSocket->errorString();
        return;
    }
    
    m_tcpSocket->flush();
    
    // 如果需要等待响应，启动轮询
    if (shouldWait) {
        QString contentStr = QString::fromUtf8(content);
        QString deviceNum;
        
        if (asciiOrHex && contentStr.startsWith(">") && contentStr.length() >= 4) {
            deviceNum = contentStr.mid(1, 2);
        } else if (!asciiOrHex && contentStr.length() >= 12) {
            deviceNum = contentStr.left(2);
        }
        
        startPolling(deviceNum, contentStr);
    }
}

