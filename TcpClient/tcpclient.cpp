#include "tcpclient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QTextEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QGroupBox>
#include <QTcpSocket>
#include <QTimer>
#include <QDateTime>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QDebug>
#include <QMessageBox>
#include <QScrollBar>
#include <QFont>

TcpClient::TcpClient(QWidget *parent)
    : QWidget(parent)
    , m_socket(nullptr)
    , m_reconnectTimer(nullptr)
    , m_isConnected(false)
    , m_autoReconnect(false)
    , m_reconnectInterval(3000)
    , m_bytesSent(0)
    , m_bytesReceived(0)
    , m_lastRemoteHost("127.0.0.1")
    , m_lastRemotePort(8080)
    , m_lastLocalHost("")
    , m_lastLocalPort(0)
{
    setWindowTitle("TCP客户端调试工具");
    setMinimumSize(600, 500);
    resize(800, 600);
    
    // 初始化网络组件
    m_socket = new QTcpSocket(this);
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    
    // 连接信号
    connect(m_socket, &QTcpSocket::connected, this, &TcpClient::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &TcpClient::onSocketDisconnected);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &TcpClient::onSocketError);
    connect(m_socket, &QTcpSocket::readyRead, this, &TcpClient::onSocketDataReady);
    connect(m_reconnectTimer, &QTimer::timeout, this, &TcpClient::onReconnectTimer);
    
    // 设置UI
    setupUI();
    
    // 初始化状态
    updateConnectionStatus(false);
}

TcpClient::~TcpClient()
{
    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->disconnectFromHost();
    }
}

void TcpClient::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(10);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    
    createConnectionGroup();
    createControlGroup();
    createLogGroup();
    createStatusGroup();
    
    // 设置布局比例
    m_mainLayout->addWidget(m_connectionGroup);
    m_mainLayout->addWidget(m_controlGroup);
    m_mainLayout->addWidget(m_logGroup, 1); // 日志区域占据剩余空间
    m_mainLayout->addWidget(m_statusGroup);
}

void TcpClient::createConnectionGroup()
{
    m_connectionGroup = new QGroupBox("连接配置", this);
    QGridLayout *layout = new QGridLayout(m_connectionGroup);
    
    // 远端配置
    layout->addWidget(new QLabel("远端IP:"), 0, 0);
    m_remoteHostEdit = new QLineEdit(m_lastRemoteHost);
    m_remoteHostEdit->setPlaceholderText("目标服务器IP地址");
    layout->addWidget(m_remoteHostEdit, 0, 1);
    
    layout->addWidget(new QLabel("远端端口:"), 0, 2);
    m_remotePortSpin = new QSpinBox();
    m_remotePortSpin->setRange(1, 65535);
    m_remotePortSpin->setValue(m_lastRemotePort);
    layout->addWidget(m_remotePortSpin, 0, 3);
    
    // 本地配置
    layout->addWidget(new QLabel("本机IP:"), 1, 0);
    m_localHostEdit = new QLineEdit(m_lastLocalHost);
    m_localHostEdit->setPlaceholderText("本地IP(可选，留空自动选择)");
    layout->addWidget(m_localHostEdit, 1, 1);
    
    layout->addWidget(new QLabel("本机端口:"), 1, 2);
    m_localPortSpin = new QSpinBox();
    m_localPortSpin->setRange(0, 65535);
    m_localPortSpin->setValue(m_lastLocalPort);
    m_localPortSpin->setSpecialValueText("自动");
    layout->addWidget(m_localPortSpin, 1, 3);
    
    // 连接按钮和状态
    m_connectButton = new QPushButton("连接");
    m_connectButton->setMinimumHeight(40);
    connect(m_connectButton, &QPushButton::clicked, this, &TcpClient::onConnectClicked);
    layout->addWidget(m_connectButton, 2, 0, 1, 2);
    
    m_connectionStatusLabel = new QLabel("未连接");
    m_connectionStatusLabel->setAlignment(Qt::AlignCenter);
    m_connectionStatusLabel->setStyleSheet("QLabel { background-color: #ffcccc; border: 1px solid #ff6666; border-radius: 3px; padding: 5px; }");
    layout->addWidget(m_connectionStatusLabel, 2, 2, 1, 2);
}

void TcpClient::createControlGroup()
{
    m_controlGroup = new QGroupBox("数据发送", this);
    QVBoxLayout *layout = new QVBoxLayout(m_controlGroup);
    
    // 发送数据输入
    m_sendEdit = new QTextEdit();
    m_sendEdit->setMaximumHeight(80);
    m_sendEdit->setPlaceholderText("输入要发送的数据...");
    layout->addWidget(m_sendEdit);
    
    // 控制按钮行
    QHBoxLayout *controlLayout = new QHBoxLayout();
    
    m_hexModeCheck = new QCheckBox("16进制发送");
    m_hexModeCheck->setToolTip("勾选时以16进制格式发送数据，取消勾选时以字符串发送");
    connect(m_hexModeCheck, &QCheckBox::toggled, this, &TcpClient::onHexModeChanged);
    controlLayout->addWidget(m_hexModeCheck);
    
    controlLayout->addStretch();
    
    m_sendButton = new QPushButton("发送");
    m_sendButton->setEnabled(false);
    connect(m_sendButton, &QPushButton::clicked, this, &TcpClient::onSendClicked);
    controlLayout->addWidget(m_sendButton);
    
    m_clearLogButton = new QPushButton("清空日志");
    connect(m_clearLogButton, &QPushButton::clicked, this, &TcpClient::onClearLogClicked);
    controlLayout->addWidget(m_clearLogButton);
    
    layout->addLayout(controlLayout);
}

void TcpClient::createLogGroup()
{
    m_logGroup = new QGroupBox("通信日志", this);
    QVBoxLayout *layout = new QVBoxLayout(m_logGroup);
    
    m_logEdit = new QTextEdit();
    m_logEdit->setReadOnly(true);
    m_logEdit->setFont(QFont("Consolas", 9));
    m_logEdit->setStyleSheet("QTextEdit { background-color: #f5f5f5; }");
    layout->addWidget(m_logEdit);
}

void TcpClient::createStatusGroup()
{
    m_statusGroup = new QGroupBox("状态信息", this);
    QHBoxLayout *layout = new QHBoxLayout(m_statusGroup);
    
    m_statusLabel = new QLabel("就绪");
    layout->addWidget(new QLabel("状态:"));
    layout->addWidget(m_statusLabel);
    
    layout->addStretch();
    
    m_bytesSentLabel = new QLabel("0");
    layout->addWidget(new QLabel("已发送:"));
    layout->addWidget(m_bytesSentLabel);
    layout->addWidget(new QLabel("字节"));
    
    layout->addStretch();
    
    m_bytesReceivedLabel = new QLabel("0");
    layout->addWidget(new QLabel("已接收:"));
    layout->addWidget(m_bytesReceivedLabel);
    layout->addWidget(new QLabel("字节"));
    
    layout->addStretch();
    
    m_connectionTimeLabel = new QLabel("--");
    layout->addWidget(new QLabel("连接时长:"));
    layout->addWidget(m_connectionTimeLabel);
}

bool TcpClient::connectToHost(const QString &host, quint16 port)
{
    return connectToHost(host, port, "", 0);
}

bool TcpClient::connectToHost(const QString &host, quint16 port, const QString &localHost, quint16 localPort)
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        appendLog("正在断开现有连接...", "[系统]", QColor(255, 165, 0));
        m_socket->disconnectFromHost();
    }
    
    // 保存连接参数
    m_lastRemoteHost = host;
    m_lastRemotePort = port;
    m_lastLocalHost = localHost;
    m_lastLocalPort = localPort;
    
    // 绑定本地地址（如果指定）
    if (!localHost.isEmpty() && localPort > 0) {
        bool bindResult = m_socket->bind(QHostAddress(localHost), localPort);
        if (!bindResult) {
            QString error = QString("绑定本地地址失败: %1:%2").arg(localHost).arg(localPort);
            appendLog(error, "[错误]", Qt::red);
            emit connectionError(error);
            return false;
        }
        appendLog(QString("已绑定本地地址: %1:%2").arg(localHost).arg(localPort), "[系统]", QColor(0, 128, 255));
    }
    
    appendLog(QString("正在连接到 %1:%2...").arg(host).arg(port), "[系统]", QColor(0, 128, 255));
    m_socket->connectToHost(host, port);
    
    return true;
}

void TcpClient::disconnectFromHost()
{
    m_reconnectTimer->stop();
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        appendLog("正在断开连接...", "[系统]", QColor(255, 165, 0));
        m_socket->disconnectFromHost();
    }
}

bool TcpClient::isConnected() const
{
    return m_isConnected;
}

void TcpClient::sendData(const QByteArray &data)
{
    if (!m_isConnected) {
        appendLog("发送失败: 未连接", "[错误]", Qt::red);
        return;
    }
    
    qint64 written = m_socket->write(data);
    if (written == -1) {
        appendLog("发送失败: " + m_socket->errorString(), "[错误]", Qt::red);
        return;
    }
    
    m_bytesSent += written;
    updateDataStatistics();
    
    QString hexStr = data.toHex(' ').toUpper();
    QString textStr = QString::fromUtf8(data);
    
    appendLog(QString("HEX: %1").arg(hexStr), "[发送]", QColor(0, 128, 0));
    if (data.isPrint()) {
        appendLog(QString("TXT: %1").arg(textStr), "[发送]", QColor(0, 128, 0));
    }
    
    emit dataSent(data);
}

void TcpClient::sendText(const QString &text)
{
    sendData(text.toUtf8());
}

void TcpClient::sendHexString(const QString &hexString)
{
    QByteArray data = parseHexString(hexString);
    if (data.isEmpty() && !hexString.isEmpty()) {
        appendLog("发送失败: 16进制格式错误", "[错误]", Qt::red);
        return;
    }
    sendData(data);
}

QString TcpClient::getRemoteAddress() const
{
    return m_socket->peerAddress().toString();
}

quint16 TcpClient::getRemotePort() const
{
    return m_socket->peerPort();
}

QString TcpClient::getLocalAddress() const
{
    return m_socket->localAddress().toString();
}

quint16 TcpClient::getLocalPort() const
{
    return m_socket->localPort();
}

void TcpClient::clearLog()
{
    m_logEdit->clear();
}

QString TcpClient::getLogContent() const
{
    return m_logEdit->toPlainText();
}

void TcpClient::setAutoReconnect(bool enabled)
{
    m_autoReconnect = enabled;
}

void TcpClient::setReconnectInterval(int milliseconds)
{
    m_reconnectInterval = milliseconds;
}

void TcpClient::onConnectClicked()
{
    if (m_isConnected) {
        disconnectFromHost();
    } else {
        QString host = m_remoteHostEdit->text().trimmed();
        quint16 port = static_cast<quint16>(m_remotePortSpin->value());
        QString localHost = m_localHostEdit->text().trimmed();
        quint16 localPort = static_cast<quint16>(m_localPortSpin->value());
        
        if (host.isEmpty()) {
            QMessageBox::warning(this, "错误", "请输入远端IP地址");
            return;
        }
        
        connectToHost(host, port, localHost, localPort);
    }
}

void TcpClient::onSendClicked()
{
    QString text = m_sendEdit->toPlainText();
    if (text.isEmpty()) {
        return;
    }
    
    if (m_hexModeCheck->isChecked()) {
        sendHexString(text);
    } else {
        sendText(text);
    }
    
    // 清空发送框
    m_sendEdit->clear();
}

void TcpClient::onClearLogClicked()
{
    clearLog();
    m_bytesSent = 0;
    m_bytesReceived = 0;
    updateDataStatistics();
}

void TcpClient::onHexModeChanged(bool hexMode)
{
    if (hexMode) {
        m_sendEdit->setPlaceholderText("输入16进制数据 (如: 48 65 6C 6C 6F)...");
    } else {
        m_sendEdit->setPlaceholderText("输入要发送的文本...");
    }
}

void TcpClient::onSocketConnected()
{
    m_isConnected = true;
    m_connectionTime = QDateTime::currentDateTime();
    updateConnectionStatus(true);
    
    QString localInfo = QString("%1:%2").arg(getLocalAddress()).arg(getLocalPort());
    QString remoteInfo = QString("%1:%2").arg(getRemoteAddress()).arg(getRemotePort());
    appendLog(QString("连接成功! 本地: %1, 远端: %2").arg(localInfo).arg(remoteInfo), "[系统]", QColor(0, 128, 0));
    
    emit connected();
    emit statusChanged("已连接");
}

void TcpClient::onSocketDisconnected()
{
    m_isConnected = false;
    updateConnectionStatus(false);
    
    appendLog("连接已断开", "[系统]", QColor(255, 165, 0));
    
    if (m_autoReconnect) {
        appendLog(QString("将在 %1 秒后自动重连...").arg(m_reconnectInterval / 1000), "[系统]", QColor(0, 128, 255));
        m_reconnectTimer->start(m_reconnectInterval);
    }
    
    emit disconnected();
    emit statusChanged("已断开");
}

void TcpClient::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    QString errorString = m_socket->errorString();
    appendLog("连接错误: " + errorString, "[错误]", Qt::red);
    
    m_isConnected = false;
    updateConnectionStatus(false);
    
    emit connectionError(errorString);
    emit statusChanged("错误: " + errorString);
}

void TcpClient::onSocketDataReady()
{
    QByteArray data = m_socket->readAll();
    if (data.isEmpty()) return;
    
    m_bytesReceived += data.size();
    updateDataStatistics();
    
    QString hexStr = data.toHex(' ').toUpper();
    QString textStr = QString::fromUtf8(data);
    
    appendLog(QString("HEX: %1").arg(hexStr), "[接收]", QColor(0, 0, 255));
    if (data.isPrint()) {
        appendLog(QString("TXT: %1").arg(textStr), "[接收]", QColor(0, 0, 255));
    }
    
    emit dataReceived(data);
}

void TcpClient::onReconnectTimer()
{
    if (!m_isConnected) {
        appendLog("自动重连中...", "[系统]", QColor(0, 128, 255));
        connectToHost(m_lastRemoteHost, m_lastRemotePort, m_lastLocalHost, m_lastLocalPort);
    }
}

void TcpClient::updateConnectionStatus(bool connected)
{
    if (connected) {
        m_connectButton->setText("断开");
        setButtonStyle(m_connectButton, true);
        m_connectionStatusLabel->setText("已连接");
        m_connectionStatusLabel->setStyleSheet("QLabel { background-color: #ccffcc; border: 1px solid #66cc66; border-radius: 3px; padding: 5px; color: #006600; }");
        m_sendButton->setEnabled(true);
        
        // 禁用连接配置编辑
        m_remoteHostEdit->setEnabled(false);
        m_remotePortSpin->setEnabled(false);
        m_localHostEdit->setEnabled(false);
        m_localPortSpin->setEnabled(false);
    } else {
        m_connectButton->setText("连接");
        setButtonStyle(m_connectButton, false);
        m_connectionStatusLabel->setText("未连接");
        m_connectionStatusLabel->setStyleSheet("QLabel { background-color: #ffcccc; border: 1px solid #ff6666; border-radius: 3px; padding: 5px; color: #cc0000; }");
        m_sendButton->setEnabled(false);
        
        // 启用连接配置编辑
        m_remoteHostEdit->setEnabled(true);
        m_remotePortSpin->setEnabled(true);
        m_localHostEdit->setEnabled(true);
        m_localPortSpin->setEnabled(true);
        
        m_connectionTimeLabel->setText("--");
    }
}

void TcpClient::updateDataStatistics()
{
    m_bytesSentLabel->setText(QString::number(m_bytesSent));
    m_bytesReceivedLabel->setText(QString::number(m_bytesReceived));
    
    if (m_isConnected) {
        qint64 seconds = m_connectionTime.secsTo(QDateTime::currentDateTime());
        int hours = seconds / 3600;
        int minutes = (seconds % 3600) / 60;
        int secs = seconds % 60;
        m_connectionTimeLabel->setText(QString("%1:%2:%3")
                                      .arg(hours, 2, 10, QChar('0'))
                                      .arg(minutes, 2, 10, QChar('0'))
                                      .arg(secs, 2, 10, QChar('0')));
    }
}

QString TcpClient::formatByteArray(const QByteArray &data, bool asHex) const
{
    if (asHex) {
        return data.toHex(' ').toUpper();
    } else {
        return QString::fromUtf8(data);
    }
}

QByteArray TcpClient::parseHexString(const QString &hexString) const
{
    QString cleaned = hexString;
    cleaned.remove(QRegExp("[^0-9A-Fa-f]")); // 移除非16进制字符
    
    if (cleaned.length() % 2 != 0) {
        return QByteArray(); // 长度必须是偶数
    }
    
    QByteArray result;
    for (int i = 0; i < cleaned.length(); i += 2) {
        QString byteString = cleaned.mid(i, 2);
        bool ok;
        quint8 byte = static_cast<quint8>(byteString.toUInt(&ok, 16));
        if (!ok) {
            return QByteArray(); // 解析失败
        }
        result.append(static_cast<char>(byte));
    }
    
    return result;
}

void TcpClient::appendLog(const QString &message, const QString &prefix, const QColor &color)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString fullMessage = QString("[%1] %2 %3").arg(timestamp).arg(prefix).arg(message);
    
    // 保存当前滚动位置
    QScrollBar *scrollBar = m_logEdit->verticalScrollBar();
    bool shouldScrollToBottom = scrollBar->value() == scrollBar->maximum();
    
    // 添加带颜色的文本
    QTextCursor cursor = m_logEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    
    QTextCharFormat format;
    format.setForeground(color);
    cursor.setCharFormat(format);
    cursor.insertText(fullMessage + "\n");
    
    // 如果之前在底部，继续滚动到底部
    if (shouldScrollToBottom) {
        scrollBar->setValue(scrollBar->maximum());
    }
}

void TcpClient::setButtonStyle(QPushButton *button, bool success)
{
    if (success) {
        button->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; border: none; padding: 8px; border-radius: 4px; }"
                             "QPushButton:hover { background-color: #45a049; }"
                             "QPushButton:pressed { background-color: #3d8b40; }");
    } else {
        button->setStyleSheet(""); // 恢复默认样式
    }
}