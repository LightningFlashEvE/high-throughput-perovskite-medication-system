#include "tcpCamera.h"
#include <QDebug>
#include <QNetworkProxy>

/**
 * @brief 构造函数
 * @details 初始化 TCP socket 对象，并连接相关信号槽：
 *          - readyRead: 接收数据
 *          - connected: 连接成功
 *          - disconnected: 断开连接
 *          - errorOccurred: 错误处理
 * @param parent 父对象指针
 */
tcpCamera::tcpCamera(QObject *parent)
    : QObject{parent}
    , m_socket(new QTcpSocket(this))
    , m_isConnected(false)
    , m_timer(new QTimer(this))
    , m_database(new DatabaseOnline(this))
{
    // 连接 socket 相关信号槽
    connect(m_socket, &QTcpSocket::readyRead, this, &tcpCamera::onReadyRead);
    connect(m_socket, &QTcpSocket::connected, this, &tcpCamera::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &tcpCamera::onDisconnected);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &tcpCamera::onErrorOccurred);

    // 连接定时器超时信号
    connect(m_timer, &QTimer::timeout, this, &tcpCamera::onTimerTimeout);

    // 初始化数据库连接
    if (m_database->connectToDatabase()) {
        qDebug() << "tcpCamera: 网页数据库连接成功";
    } else {
        qWarning() << "tcpCamera: 网页数据库连接失败:" << m_database->lastError();
    }

    // 监听数据库错误
    connect(m_database, &DatabaseOnline::errorOccurred, this, [](const QString &error) {
        qWarning() << "tcpCamera: 数据库错误:" << error;
    });
}

/**
 * @brief 析构函数
 * @details 断开 TCP 连接和数据库连接，释放资源
 */
tcpCamera::~tcpCamera()
{
    tcpCameraDisconnect();
    
    // 断开数据库连接
    if (m_database) {
        m_database->disconnectFromDatabase();
    }
}

/**
 * @brief 连接到相机服务器
 * @details 通过 TCP 协议连接到指定 IP 和端口的相机服务器。
 *          如果当前已连接，会先断开原有连接再重新连接。
 *          连接超时时间为 3000ms。
 * @param ip 服务器 IP 地址
 * @param port 服务器端口号
 * @param useProxy 是否使用代理（true=使用应用/系统代理，false=直连）
 * @return true: 连接成功; false: 连接失败
 */
bool tcpCamera::tcpCameraConnect(const QString &ip, quint16 port, bool useProxy)
{
    // 根据参数决定是否使用代理
    if (useProxy) {
        // 使用默认应用/系统代理
        m_socket->setProxy(QNetworkProxy::DefaultProxy);
    } else {
        // 禁用代理，直接连接相机
        m_socket->setProxy(QNetworkProxy::NoProxy);
    }

    // 如果已经连接，先断开
    if (m_isConnected) {
        tcpCameraDisconnect();
    }

    // 连接到服务器
    m_socket->connectToHost(ip, port);

    // 等待连接完成，超时时间 3000ms
    if (m_socket->waitForConnected(3000)) {
        qDebug() << "tcpCamera: Connected to" << ip << ":" << port;
        return true;
    } else {
        qDebug() << "tcpCamera: Failed to connect to" << ip << ":" << port
                 << ", error:" << m_socket->errorString();
        return false;
    }
}

/**
 * @brief 断开与相机服务器的连接
 * @details 安全地断开 TCP 连接，等待断开完成（超时 1000ms），
 *          并重置连接状态标志
 */
void tcpCamera::tcpCameraDisconnect()
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->waitForDisconnected(1000);
        }
    }
    m_isConnected = false;
}

/**
 * @brief 发送数据到相机服务器（QByteArray 版本）
 * @details 向已连接的相机服务器发送字节数据。
 *          发送前会检查连接状态，发送后等待写入完成（超时 1000ms）。
 * @param msg 要发送的字节数据
 * @return true: 发送成功; false: 发送失败（未连接/写入错误/超时）
 */
bool tcpCamera::tcpCameraSend(const QByteArray &msg)
{
    if (!m_isConnected) {
        qDebug() << "tcpCamera: Not connected, cannot send message";
        return false;
    }

    qint64 bytesWritten = m_socket->write(msg);
    if (bytesWritten == -1) {
        qDebug() << "tcpCamera: Failed to send message:" << m_socket->errorString();
        return false;
    }

    // 等待数据发送完成
    if (!m_socket->waitForBytesWritten(1000)) {
        qDebug() << "tcpCamera: Timeout waiting for bytes written";
        return false;
    }

    qDebug() << "tcpCamera: Sent" << bytesWritten << "bytes";
    return true;
}

/**
 * @brief 发送数据到相机服务器（QString 版本）
 * @details 将字符串转换为 UTF-8 编码的字节数据后发送
 * @param msg 要发送的字符串
 * @return true: 发送成功; false: 发送失败
 */
bool tcpCamera::tcpCameraSend(const QString &msg)
{
    return tcpCameraSend(msg.toUtf8());
}

/**
 * @brief 获取当前连接状态
 * @return true: 已连接; false: 未连接
 */
bool tcpCamera::isConnected() const
{
    return m_isConnected;
}

/**
 * @brief 启动/停止定时器
 * @param intervalMs 定时周期（毫秒）
 * @param enabled true 启动定时器；false 关闭定时器
 *
 * 使用示例：
 *     tcpCamera.startTime(1000, true);  // 每 1000ms 触发一次 onTimerTimeout()
 */
void tcpCamera::startTime(int intervalMs, bool enabled)
{
    if (!m_timer) {
        return;
    }

    if (!enabled) {
        // 关闭定时器
        if (m_timer->isActive()) {
            m_timer->stop();
        }
        return;
    }

    // 启动定时器（循环触发）
    m_timer->setInterval(intervalMs);
    if (!m_timer->isActive()) {
        m_timer->start();
    }
}

/**
 * @brief 数据接收槽函数
 * @details 当 socket 有数据可读时被调用，读取所有可用数据，
 *          并进行基础解析（按 ';' 分割）
 * 
 * Tray1,3,PbI2:10000;,
 * Tray3,3,001:0;,
 * Tray1,2,DMF:895;,
 * Tray3,2,004:0;,
 * Tray3,0,003:0;,
 * Tray1,0,NMP:5225;,
 * Tray4,7,002:0;,
 * Tray1,5,NMP:5225;,
 * Tray1,4,CsI:1500;,T
 * ray1,1,PbCl2:752;,
 * Tray1,6,dddd:TEG:5655;
 */
void tcpCamera::onReadyRead()
{
    // 读取所有可用数据
    QByteArray data = m_socket->readAll();
    if (!data.isEmpty()) {
        qDebug() << "tcpCamera: Received" << data.size() << "bytes";

        // 将数据转为字符串
        const QString msg = QString::fromUtf8(data);

        // 按 ';' 分割，去掉空段
        const QStringList segments = msg.split(QLatin1Char(';'), Qt::SkipEmptyParts);

        // 先简单打印每一段，后续按你的指令再做进一步处理  
        for (const QString &seg : segments) {
            qDebug() << "tcpCamera: segment =" << seg;

        }
        // 去更新临时表，本地数据库

        // 如果之后需要把解析后的结果传给外部，可以在这里发射自定义信号
        // emit dataReceived(data); // 暂时先不需要
    }
}

/**
 * @brief 连接成功槽函数
 * @details 当 TCP 连接建立成功时被调用，更新连接状态标志，
 *          并发射 connected 信号通知外部
 */
void tcpCamera::onConnected()
{
    m_isConnected = true;
    qDebug() << "tcpCamera: Connection established";
    emit connected();
}

/**
 * @brief 断开连接槽函数
 * @details 当 TCP 连接断开时被调用，更新连接状态标志，
 *          并发射 disconnected 信号通知外部
 */
void tcpCamera::onDisconnected()
{
    m_isConnected = false;
    qDebug() << "tcpCamera: Connection closed";
    emit disconnected();
}

/**
 * @brief 错误处理槽函数
 * @details 当 socket 发生错误时被调用，获取错误信息，
 *          并通过 errorOccurred 信号将错误消息发射出去
 * @param socketError socket 错误类型枚举
 */
void tcpCamera::onErrorOccurred(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError)
    QString errorMsg = m_socket->errorString();
    qDebug() << "tcpCamera: Socket error:" << errorMsg;
    emit errorOccurred(errorMsg);
}

/**
 * @brief 定时器超时槽函数
 * @details 定时器每次触发时会调用此函数。
 *          你可以在这里编写需要周期执行的代码。
 */
void tcpCamera::onTimerTimeout()
{
    // 定时发送字符串 "photo"
    // 内部会检查连接状态，未连接时会返回 false 并打印日志
    tcpCameraSend(QStringLiteral("photo"));
}
