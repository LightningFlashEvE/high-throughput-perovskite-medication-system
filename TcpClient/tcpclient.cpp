#include "tcpclient.h"
#include "ui_tcpclient.h"
#include <QMessageBox>
#include <QDateTime>
#include <QScrollBar>
#include <QDebug>
#include <QApplication>
#include <QClipboard>
#include <QRegularExpression>
#include <QRadioButton>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QTextEdit>
#include <utility>
// 包含协议系统
#include "tcpclient_crc.h"

TcpClient::TcpClient(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TcpClient)
    , m_tcpSocket(nullptr)
    , m_tcpServer(nullptr)
    , m_isConnected(false)
    , m_isServerMode(false)
    , m_updateTimer(new QTimer(this))
    , m_currentProtocol(nullptr)
    , m_crcHelper(new TcpClientCrc(this))
{
    ui->setupUi(this);

    // 设置connect
    setupConnections();
    
    // 初始化UI
    updateUI();
    
    // 延迟加载网络信息 - 使用定时器延迟执行
    QTimer::singleShot(200, this, &TcpClient::refreshLocalIPs);
    // 设置定时器 - 降低频率减少CPU占用
    m_updateTimer->setInterval(5000);
    connect(m_updateTimer, &QTimer::timeout, this, &TcpClient::updateLocalIPs);
    // 延迟启动定时器
    QTimer::singleShot(1000, this, [this]() { m_updateTimer->start(); });
    
    // 更新眉头
    initializeProtocolSystem();
    
    // 更新comBox2，3默认内容.其他内容默认够用了
    initializeFrameBuilder();
    
    // 设置默认模式
    ui->radioButton_client->setChecked(true);
    onModeChanged();
}

TcpClient::~TcpClient()
{
    if (m_tcpSocket) {
        m_tcpSocket->disconnectFromHost();
    }
    if (m_tcpServer) {
        m_tcpServer->close();
    }
    
    // 清理CRC辅助类
    delete m_crcHelper;
    
    delete ui;
}

void TcpClient::setupConnections()
{
    // 模式切换
    connect(ui->radioButton_client, &QRadioButton::toggled, this, &TcpClient::onModeChanged);
    connect(ui->radioButton_server, &QRadioButton::toggled, this, &TcpClient::onModeChanged);
    
    // 按钮连接
    connect(ui->pushButton_connect, &QPushButton::clicked, this, &TcpClient::onConnectClicked);
    connect(ui->pushButton_disconnect, &QPushButton::clicked, this, &TcpClient::onDisconnectClicked);
    connect(ui->pushButton_send, &QPushButton::clicked, this, &TcpClient::onSendClicked);
    connect(ui->pushButton_clear, &QPushButton::clicked, this, &TcpClient::onClearClicked);
    
    // 发送框回车键
    connect(ui->lineEdit_send, &QLineEdit::returnPressed, this, &TcpClient::onSendClicked);
    
    // 代理设置变化时更新网络配置
    connect(ui->checkBox_disable_proxy, &QCheckBox::toggled, this, [this]() {
        if (m_tcpSocket && m_tcpServer) {
            updateProxySettings();
        }
    });

    
    // 帧结构编辑器信号连接
    connect(ui->comboBox_protocol_type_frame, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TcpClient::onProtocolTypeChanged);
    connect(ui->comboBox1, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TcpClient::onFrameFieldChanged);
    connect(ui->comboBox2, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TcpClient::onFrameFieldChanged);
    connect(ui->comboBox3, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TcpClient::onFrameFieldChanged);
    connect(ui->lineEdit4, &QLineEdit::textChanged,
            this, &TcpClient::onFrameFieldChanged);
    // connect(ui->comboBox6, QOverload<int>::of(&QComboBox::currentIndexChanged),
    //         this, &TcpClient::onFrameFieldChanged);
    // connect(ui->pushButton_build_frame, &QPushButton::clicked,
    //         this, &TcpClient::onBuildFrameClicked);
    
    // 预览模式切换信号连接
    connect(ui->radioButton_ascii_display, &QRadioButton::toggled, 
            this, &TcpClient::onPreviewModeChanged);
    connect(ui->radioButton_hex_display, &QRadioButton::toggled,
            this, &TcpClient::onPreviewModeChanged);

    
    connect(ui->checkBox_real_time_build, &QCheckBox::toggled, this, [this](bool enabled) {
        if (enabled) {
            onFrameFieldChanged();
        }
    });
    
    // 网络对象的信号连接将在延迟初始化时设置
}

void TcpClient::initializeNetworkObjects()
{
    if (m_tcpSocket && m_tcpServer) {
        return; // 已经初始化过了
    }
    
    // 创建网络对象
    if (!m_tcpSocket) {
        m_tcpSocket = new QTcpSocket(this);
    }
    if (!m_tcpServer) {
        m_tcpServer = new QTcpServer(this);
    }
    
    // 根据用户设置配置代理
    updateProxySettings();
    
    // 设置网络对象的信号连接
    connect(m_tcpSocket, &QTcpSocket::connected, this, &TcpClient::onClientConnected);
    connect(m_tcpSocket, &QTcpSocket::disconnected, this, &TcpClient::onClientDisconnected);
    connect(m_tcpSocket, &QTcpSocket::readyRead, this, &TcpClient::onDataReceived);
    
    // 兼容不同Qt版本的错误信号
    #if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(m_tcpSocket, &QAbstractSocket::errorOccurred,
            this, &TcpClient::onSocketError);
    #else
    connect(m_tcpSocket, static_cast<void(QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error),
            this, &TcpClient::onSocketError);
    #endif
    
    // TCP Server 信号
    connect(m_tcpServer, &QTcpServer::newConnection, this, &TcpClient::onNewConnection);
    
    qDebug() << "网络对象延迟初始化完成";
}

void TcpClient::updateProxySettings()
{
    if (!m_tcpSocket || !m_tcpServer) {
        return;
    }
    
    // 检查用户是否选择禁用代理
    bool disableProxy = ui->checkBox_disable_proxy->isChecked();
    
    if (disableProxy) {
        // 禁用代理
        m_tcpSocket->setProxy(QNetworkProxy::NoProxy);
        m_tcpServer->setProxy(QNetworkProxy::NoProxy);
        qDebug() << "网络代理已禁用";
    } else {
        // 使用默认代理设置
        m_tcpSocket->setProxy(QNetworkProxy::DefaultProxy);
        m_tcpServer->setProxy(QNetworkProxy::DefaultProxy);
        qDebug() << "使用默认网络代理设置";
    }
}

void TcpClient::onModeChanged()
{
    m_isServerMode = ui->radioButton_server->isChecked();
    updateUI();
    
    if (m_isServerMode) {
        ui->lineEdit_remote_ip->setEnabled(false);
        ui->spinBox_remote_port->setEnabled(false);
        ui->label_remote_ip->setText("客户端IP:");
        ui->label_remote_port->setText("客户端端口:");
    } else {
        ui->lineEdit_remote_ip->setEnabled(true);
        ui->spinBox_remote_port->setEnabled(true);
        ui->label_remote_ip->setText("远端IP:");
        ui->label_remote_port->setText("远端端口:");
    }
}

void TcpClient::onConnectClicked()
{
    // 延迟初始化网络对象
    if (!m_tcpSocket || !m_tcpServer) {
        initializeNetworkObjects();
    }
    
    if (m_isServerMode) {
        // 服务端模式
        if (m_tcpServer->isListening()) {
            return;
        }
        
        quint16 port = ui->spinBox_local_port->value();
        if (m_tcpServer->listen(QHostAddress::Any, port)) {
            m_isConnected = true;
            updateUI();
            updateConnectionInfo();
            appendMessage(QString("服务端启动成功，监听端口: %1").arg(port), "success");
        } else {
            appendMessage(QString("服务端启动失败: %1").arg(m_tcpServer->errorString()), "error");
        }
    } else {
        // 客户端模式
        if (m_tcpSocket->state() == QAbstractSocket::ConnectedState) {
            return;
        }
        
        QString host = ui->lineEdit_remote_ip->text();
        quint16 port = ui->spinBox_remote_port->value();
        
        // 先显示连接信息，再发起连接
        appendMessage(QString("正在连接到 %1:%2...").arg(host).arg(port), "info");
        m_tcpSocket->connectToHost(host, port);
    }
}

void TcpClient::onDisconnectClicked()
{
    if (!m_tcpSocket || !m_tcpServer) {
        return; // 网络对象未初始化
    }
    
    if (m_isServerMode) {
        // 服务端模式 - 关闭所有客户端连接
        for (QTcpSocket *client : m_clientSockets) {
            client->disconnectFromHost();
        }
        m_clientSockets.clear();
        m_tcpServer->close();
        m_isConnected = false;
        updateUI();
        updateConnectionInfo();
        appendMessage("服务端已关闭", "info");
    } else {
        // 客户端模式
        m_tcpSocket->disconnectFromHost();
    }
}

void TcpClient::onSendClicked()
{
    QString message = ui->lineEdit_send->text();
    if (message.isEmpty()) {
        return;
    }
    
    if (!m_tcpSocket || !m_tcpServer) {
        appendMessage("网络对象未初始化，请先点击连接", "error");
        return;
    }
    
    if (m_isServerMode) {
        // 服务端模式 - 发送给所有客户端
        if (m_clientSockets.isEmpty()) {
            appendMessage("没有连接的客户端", "warning");
            return;
        }
        
        QByteArray data;
        if (ui->checkBox_hex_mode->isChecked()) {
            data = QByteArray::fromHex(message.toUtf8());
        } else {
            data = message.toUtf8();
        }
        
        for (QTcpSocket *client : m_clientSockets) {
            client->write(data);
        }
        appendMessage(QString("发送给 %1 个客户端: %2").arg(m_clientSockets.size()).arg(message), "send");
    } else {
        // 客户端模式
        if (m_tcpSocket->state() != QAbstractSocket::ConnectedState) {
            appendMessage("未连接到服务器", "error");
            return;
        }
        
        QByteArray data;
        if (ui->checkBox_hex_mode->isChecked()) {
            data = QByteArray::fromHex(message.toUtf8());
        } else {
            data = message.toUtf8();
        }
        
        m_tcpSocket->write(data);
        appendMessage(QString("发送: %1").arg(message), "send");
    }
    
    ui->lineEdit_send->clear();
}

void TcpClient::onClearClicked()
{
    ui->textEdit_receive->clear();
}

void TcpClient::onNewConnection()
{
    QTcpSocket *clientSocket = m_tcpServer->nextPendingConnection();
    m_clientSockets.append(clientSocket);
    
    connect(clientSocket, &QTcpSocket::readyRead, this, &TcpClient::onDataReceived);
    connect(clientSocket, &QTcpSocket::disconnected, this, [this, clientSocket]() {
        m_clientSockets.removeAll(clientSocket);
        clientSocket->deleteLater();
        updateConnectionInfo();
        appendMessage(QString("客户端断开连接: %1:%2")
                     .arg(clientSocket->peerAddress().toString())
                     .arg(clientSocket->peerPort()), "info");
    });
    
    updateConnectionInfo();
    appendMessage(QString("新客户端连接: %1:%2")
                 .arg(clientSocket->peerAddress().toString())
                 .arg(clientSocket->peerPort()), "success");
}

void TcpClient::onClientConnected()
{
    m_isConnected = true;
    updateUI();
    updateConnectionInfo();
    appendMessage("已连接到服务器", "success");
}

void TcpClient::onClientDisconnected()
{
    m_isConnected = false;
    updateUI();
    updateConnectionInfo();
    appendMessage("与服务器断开连接", "info");
}

void TcpClient::onDataReceived()
{
    QTcpSocket *sender = qobject_cast<QTcpSocket*>(QObject::sender());
    if (!sender) return;
    
    QByteArray data = sender->readAll();
    QString message;
    
    if (ui->checkBox_hex_mode->isChecked()) {
        message = data.toHex(' ').toUpper();
    } else {
        message = QString::fromUtf8(data);
    }
    
    QString senderInfo;
    if (m_isServerMode) {
        senderInfo = QString("[%1:%2] ").arg(sender->peerAddress().toString()).arg(sender->peerPort());
    } else {
        senderInfo = "[服务器] ";
    }
    
    appendMessage(senderInfo + message, "receive");
}

void TcpClient::updateLocalIPs()
{
    // 这个方法可以定期更新本机IP列表
    // 目前保持简单实现
}

void TcpClient::updateUI()
{
    ui->pushButton_connect->setEnabled(!m_isConnected);
    ui->pushButton_disconnect->setEnabled(m_isConnected);
    ui->pushButton_send->setEnabled(m_isConnected);
    
    if (m_isConnected) {
        ui->label_status->setText("状态: 已连接");
        ui->label_status->setStyleSheet("QLabel { color: #28a745; font-weight: bold; padding: 4px 8px; background-color: #d4edda; border-radius: 4px; }");
    } else {
        ui->label_status->setText("状态: 未连接");
        ui->label_status->setStyleSheet("QLabel { color: #666; font-weight: bold; padding: 4px 8px; background-color: #f0f0f0; border-radius: 4px; }");
    }
}

void TcpClient::appendMessage(const QString &message, const QString &type)
{
    QString timestamp = ui->checkBox_show_timestamp->isChecked() ? getCurrentTimestamp() : "";
    QString formattedMessage = QString("[%1] %2").arg(timestamp).arg(message);
    
    // 根据消息类型设置颜色
    QString color;
    if (type == "error") color = "#dc3545";
    else if (type == "success") color = "#28a745";
    else if (type == "warning") color = "#ffc107";
    else if (type == "send") color = "#007bff";
    else if (type == "receive") color = "#6f42c1";
    else color = "#6c757d";
    
    ui->textEdit_receive->setTextColor(QColor(color));
    ui->textEdit_receive->append(formattedMessage);
    
    // 自动滚动
    if (ui->checkBox_auto_scroll->isChecked()) {
        QScrollBar *scrollBar = ui->textEdit_receive->verticalScrollBar();
        scrollBar->setValue(scrollBar->maximum());
    }
}

void TcpClient::updateConnectionInfo()
{
    if (!m_tcpSocket || !m_tcpServer) {
        ui->label_connection_info->setText("连接信息: 网络对象未初始化");
        return;
    }
    
    if (m_isServerMode) {
        if (m_isConnected) {
            ui->label_connection_info->setText(QString("连接信息: 服务端运行中，%1 个客户端连接").arg(m_clientSockets.size()));
        } else {
            ui->label_connection_info->setText("连接信息: 服务端未启动");
        }
    } else {
        if (m_isConnected) {
            ui->label_connection_info->setText(QString("连接信息: 已连接到 %1:%2")
                                             .arg(m_tcpSocket->peerAddress().toString())
                                             .arg(m_tcpSocket->peerPort()));
        } else {
            ui->label_connection_info->setText("连接信息: 未连接");
        }
    }
}

QString TcpClient::getCurrentTimestamp()
{
    return QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
}

void TcpClient::refreshLocalIPs()
{
    // 获取本机所有IP地址
    QStringList ipList;
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    
    for (const QNetworkInterface &interface : interfaces) {
        if (interface.flags().testFlag(QNetworkInterface::IsUp) && 
            !interface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            
            QList<QNetworkAddressEntry> entries = interface.addressEntries();
            for (const QNetworkAddressEntry &entry : entries) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    ipList.append(entry.ip().toString());
                }
            }
        }
    }
    
    // 更新本机IP下拉框（如果需要的话）
    // 这里可以添加IP选择功能
}

void TcpClient::onProtocolTypeChanged() // 更换协议
{
    // 获取选中的协议名称
    QString protocolName = ui->comboBox_protocol_type_frame->currentData().toString();
    if (protocolName.isEmpty())
    {
        protocolName = ui->comboBox_protocol_type_frame->currentText();
    }
    
    // 切换协议
    if (protocolName != m_currentProtocolName)
    {
        if (m_currentProtocol)
        {
            delete m_currentProtocol;
        }
        
        m_currentProtocolName = protocolName;
        m_currentProtocol = ProtocolFactory::instance().createProtocol(protocolName);
        
        if (m_currentProtocol)
        {
            updateProtocolUI();
            syncFrameFields();
        }
    }
    
    qDebug() << "协议切换到:" << m_currentProtocolName;
}

// quint16 TcpClient::calculateCRC16(const QByteArray &data)
// {
//     quint16 crc = 0xFFFF;  // 初始值
//     for (int i = 0; i < data.length(); ++i) {
//         crc ^= static_cast<quint8>(data[i]);  // XOR字节到CRC

//         for (int j = 0; j < 8; ++j) {  // 处理8位
//             if (crc & 0x0001) {
//                 crc >>= 1;
//                 crc ^= 0xA001;  // Modbus多项式（反向）
//             } else {
//                 crc >>= 1;
//             }
//         }
//     }

//     return crc;
// }

QString TcpClient::formatFrameForDisplay(const QByteArray &frame, bool hexDisplay)
{
    if (hexDisplay) {
        QString hexStr;
        for (int i = 0; i < frame.length(); ++i) {
            if (i > 0) hexStr += " ";
            hexStr += QString("%1").arg(static_cast<quint8>(frame[i]), 2, 16, QChar('0')).toUpper();
        }
        return hexStr;
    } else {
        return QString::fromUtf8(frame);
    }
}

void TcpClient::onFrameFieldChanged()
{
    // 总是更新CRC显示
    if (m_crcHelper) {
        m_crcHelper->updateCrcDisplay(); // 这里会更新crc
    }
    
    // 如果启用实时构建，更新发送框
    if (ui->checkBox_real_time_build->isChecked()) {
        QString frame = buildFrameFromFields();  // 这里也会更新crc
        if (!frame.isEmpty()) {
            ui->lineEdit_send->setText(frame);
            
            // 显示构建的帧到消息区域
            QString displayMode = ui->radioButton_hex_display->isChecked() ? "十六进制" : "ASCII";
            QString displayFrame = ui->radioButton_hex_display->isChecked() ? 
                formatFrameHex(frame) : formatFrameASCII(frame);
            appendMessage(QString("实时构建[%1]: %2").arg(displayMode).arg(displayFrame), "info");
        }
    }
}


QString TcpClient::getCurrentProtocolName() const
{
    return m_currentProtocolName;
}

void TcpClient::onPreviewModeChanged()
{
    // 显示模式改变时，如果有当前帧则重新显示
    if (ui->checkBox_real_time_build->isChecked()) {
        onFrameFieldChanged();
    }
}

QString TcpClient::formatFrameASCII(const QString &frame)
{
    QString result = frame;
    // 将不可见字符替换为可见的表示
    result.replace('\r', "<CR>");
    result.replace('\n', "<LF>");
    result.replace('\0', "<NULL>");
    
    return result;
}

QString TcpClient::formatFrameHex(const QString &frame)
{
    QByteArray frameBytes = frame.toUtf8();
    QString hexStr;
    
    for (int i = 0; i < frameBytes.length(); ++i) {
        if (i > 0) hexStr += " ";
        hexStr += QString("%1").arg(static_cast<quint8>(frameBytes[i]), 2, 16, QChar('0')).toUpper();
    }
    
    return hexStr;
}

void TcpClient::initializeProtocolSystem()
{
    // 协议已通过REGISTER_PROTOCOL宏自动注册
    // 注释：协议类在编译时通过宏自动注册到ProtocolFactory中，无需手动注册
    
    // 填充【协议下拉框】
    populateProtocolComboBoxes();
    // 注释：调用函数将已注册的协议添加到UI的下拉框组件中
    
    // 设置默认协议
    // 注释：从协议工厂获取所有可用的协议名称列表
    QStringList protocols = ProtocolFactory::instance().availableProtocols(); // 注册器的键
    // 检查是否有可用协议
    if (!protocols.isEmpty()) {
        // 注释：如果协议列表不为空，则设置第一个协议为默认协议
        
        // 设置当前协议名称为第一个可用协议
        m_currentProtocolName = protocols.first();
        // 注释：将协议名称保存到成员变量中，用于后续协议切换
        
        // 创建【当前】协议的实例对象
        m_currentProtocol = ProtocolFactory::instance().createProtocol(m_currentProtocolName);
        // 注释：通过协议工厂创建协议对象实例，用于实际的协议处理
        
        // 更新眉头
        updateProtocolUI();
    }
    
    // 输出调试信息，显示初始化结果
    qDebug() << "协议系统初始化完成，可用协议:" << protocols;
    // 注释：在调试模式下输出所有可用的协议名称，便于调试和验证
}

void TcpClient::populateProtocolComboBoxes()
{
    // 清空现有项
    ui->comboBox_protocol_type_frame->clear();
    
    // 添加所有可用协议
    QStringList protocols = ProtocolFactory::instance().availableProtocols();
    for (const QString &protocolName : std::as_const(protocols)) {
        QString displayName = ProtocolFactory::instance().getDisplayName(protocolName);
        qDebug() << "===添加协议类型===" << displayName << protocolName;
        ui->comboBox_protocol_type_frame->addItem(displayName, protocolName);
    }
}


















/*********************************** 下面是被动查看 ***********************************/

void TcpClient::updateProtocolUI()
{
    if (!m_currentProtocol) {
        qWarning() << "当前协议为空";
        return;
    }

    // 更新协议选择框的当前项
    for (int i = 0; i < ui->comboBox_protocol_type_frame->count(); ++i)
    {
        if (ui->comboBox_protocol_type_frame->itemData(i).toString() == m_currentProtocolName)
        {
            ui->comboBox_protocol_type_frame->setCurrentIndex(i);
            break;
        }
    }

    // 更新眉头：应用协议字段配置
    applyProtocolFields();

    qDebug() << "已切换到协议:" << m_currentProtocolName;
}

// UI控件访问器方法实现
QString TcpClient::getComboBox1Text() const
{
    return ui->comboBox1->currentText();
}

QString TcpClient::getComboBox2Text() const
{
    return ui->comboBox2->currentText();
}

QString TcpClient::getComboBox3Text() const
{
    return ui->comboBox3->currentText();
}

QString TcpClient::getComboBox3Data() const
{
    return ui->comboBox3->currentData().toString();
}

QString TcpClient::getLineEdit4Text() const
{
    return ui->lineEdit4->text();
}

void TcpClient::setLineEdit5Text(const QString &text)
{
    ui->lineEdit5->setText(text);
    //qDebug() << "222222222222222" << text;
}

void TcpClient::clearLineEdit5()
{
    ui->lineEdit5->clear();
}

void TcpClient::onSocketError(QAbstractSocket::SocketError error)
{
    // 使用参数获取更详细的错误信息
    QString errorTypeString;
    switch (error) {
    case QAbstractSocket::ConnectionRefusedError:
        errorTypeString = "连接被拒绝";
        break;
    case QAbstractSocket::RemoteHostClosedError:
        errorTypeString = "远程主机关闭连接";
        break;
    case QAbstractSocket::HostNotFoundError:
        errorTypeString = "找不到主机";
        break;
    case QAbstractSocket::SocketTimeoutError:
        errorTypeString = "连接超时";
        break;
    case QAbstractSocket::NetworkError:
        errorTypeString = "网络错误";
        break;
    case QAbstractSocket::SocketAccessError:
        errorTypeString = "访问权限错误";
        break;
    case QAbstractSocket::SocketResourceError:
        errorTypeString = "资源不足";
        break;
    case QAbstractSocket::DatagramTooLargeError:
        errorTypeString = "数据包过大";
        break;
    case QAbstractSocket::UnsupportedSocketOperationError:
        errorTypeString = "不支持的Socket操作";
        break;
    case QAbstractSocket::UnfinishedSocketOperationError:
        errorTypeString = "Socket操作未完成";
        break;
    case QAbstractSocket::AddressInUseError:
        errorTypeString = "地址已被使用";
        break;
    case QAbstractSocket::SocketAddressNotAvailableError:
        errorTypeString = "Socket地址不可用";
        break;
    case QAbstractSocket::ProxyAuthenticationRequiredError:  // 错误代码 12
        errorTypeString = "代理认证错误";
        break;
    case QAbstractSocket::SslHandshakeFailedError:           // 错误代码 13
        errorTypeString = "SSL握手失败";
        break;
    case QAbstractSocket::ProxyConnectionRefusedError:      // 错误代码 14
        errorTypeString = "代理连接被拒绝";
        break;
    case QAbstractSocket::ProxyConnectionClosedError:       // 错误代码 15
        errorTypeString = "代理连接已关闭";
        break;
    case QAbstractSocket::ProxyConnectionTimeoutError:      // 错误代码 16
        errorTypeString = "代理连接超时";
        break;
    case QAbstractSocket::ProxyNotFoundError:               // 错误代码 17
        errorTypeString = "找不到代理服务器";
        break;
    case QAbstractSocket::ProxyProtocolError:               // 错误代码 18
        errorTypeString = "代理协议错误";
        break;
    case QAbstractSocket::OperationError:                   // 错误代码 19
        errorTypeString = "操作错误";
        break;
    case QAbstractSocket::SslInternalError:                 // 错误代码 20
        errorTypeString = "SSL内部错误";
        break;
    case QAbstractSocket::SslInvalidUserDataError:          // 错误代码 21
        errorTypeString = "SSL用户数据错误";
        break;
    case QAbstractSocket::TemporaryError:                   // 错误代码 22
        errorTypeString = "临时错误";
        break;
    case QAbstractSocket::UnknownSocketError:               // 错误代码 -1
        errorTypeString = "未知Socket错误";
        break;
    default:
        errorTypeString = QString("未知错误(代码:%1)").arg(error);
        break;
    }

    QString detailedError = m_tcpSocket ? m_tcpSocket->errorString() : "无详细信息";
    appendMessage(QString("连接错误: %1 - %2").arg(errorTypeString).arg(detailedError), "error");

    if (m_isConnected) {
        m_isConnected = false;
        updateUI();
        updateConnectionInfo();
    }
}

void TcpClient::applyProtocolFields()
{
    if (!m_currentProtocol) return;

    // 获取帧结构字段
    QList<ProtocolField> frameFields = m_currentProtocol->frameFields();
    //QList<ProtocolField> parseFields = m_currentProtocol->parseFields();

    // 更新帧结构编辑器标签
    if (frameFields.size() >= 6) {
        ui->label_frame_header->setText(QString("1.%1").arg(frameFields[0].name));  // 1.帧头
        ui->label_frame_header_desc->setText(frameFields[0].description);           // （2字符）
        ui->comboBox1->clear();                                                     // comboBox里面的内容
        ui->comboBox1->addItems(frameFields[0].options);
        if (!frameFields[0].options.isEmpty()) {
            ui->comboBox1->setCurrentText(frameFields[0].placeholder);
        }

        ui->label_slave_address_frame->setText(QString("2.%1").arg(frameFields[1].name));
        ui->label_slave_address_desc->setText(frameFields[1].description);
        ui->comboBox2->clear();
        ui->comboBox2->addItems(frameFields[1].options);

        ui->label_function_code_frame->setText(QString("3.%1").arg(frameFields[2].name));
        ui->label_function_code_desc->setText(frameFields[2].description);
        ui->comboBox3->clear();
        ui->comboBox3->addItems(frameFields[2].options);

        // ui->label_command_data_frame->setText(QString("4.%1").arg(frameFields[3].name));
        // ui->label_command_data_desc->setText(frameFields[3].description);
        // ui->lineEdit4->setText(frameFields[3].placeholder);

        ui->label_crc_frame->setText(QString("5.%1").arg(frameFields[4].name));
        ui->label_crc_desc->setText(frameFields[4].description);
        ui->lineEdit5->setPlaceholderText(frameFields[4].placeholder);

        ui->label_frame_tail->setText(QString("6.%1").arg(frameFields[5].name));
        ui->label_frame_tail_desc->setText(frameFields[5].description);
        ui->comboBox6->clear();
        ui->comboBox6->addItems(frameFields[5].options);
    }

}


void TcpClient::initializeFrameBuilder()
{

    // 同步协议配置到帧编辑器
    syncFrameFields();
}

void TcpClient::syncFrameFields()
{
    // 在 initializeProtocolSystem() 已经被赋值: 创建【当前】协议的实例对象
    if (!m_currentProtocol) {
        qWarning() << "当前协议为空，无法同步字段";
        return;
    }

    // 获取协议对应的基础包 frameFields
    QList<ProtocolField> frameFields = m_currentProtocol->frameFields();

    // 填充comBox2
    ui->comboBox2->clear();
    if (frameFields.size() > 1 && !frameFields[1].options.isEmpty())
    {
        ui->comboBox2->addItems(frameFields[1].options);
    }
    else
    {
        // 如果没有预定义选项，添加默认地址范围
        for (int i = 1; i <= 9; ++i) {
            ui->comboBox2->addItem(QString("%1").arg(i, 2, 10, QChar('0')));
        }
    }

    // 填充comBox3
    ui->comboBox3->clear();
    if (frameFields.size() > 2)
    {
        for (const QString &option : std::as_const(frameFields[2].options)) {
            // 获取选项的描述
            QString description = m_currentProtocol->getFieldDescription("功能代码", option);
            QString displayText = QString("%1:%2").arg(option, description);
            ui->comboBox3->addItem(displayText, option);
        }
    }


    // 如果启用实时构建，立即更新
    if (ui->checkBox_real_time_build->isChecked()) {
        onFrameFieldChanged();
    }
}

QString TcpClient::buildFrameFromFields() // 勾选自动的时候调用
{
    // 1) 获取并处理 data（原帧头）
    const QString dataHeadRaw = ui->comboBox1->currentText();
    const QString dataHead    = (dataHeadRaw == "3E") ? QString(">") : dataHeadRaw;


    // 2) 获取并处理 data2（原从机地址）
    const QString data1Raw = ui->comboBox2->currentText();
    const QString data1    = data1Raw; // 此处无需转换

    // 3) 获取并处理 data2（原功能码：优先 itemData，其次解析 "X:描述" -> "X"）
    QString data2 = ui->comboBox3->currentData().toString();
    if (data2.isEmpty()) {
        const QString display = ui->comboBox3->currentText();
        const int sep = display.indexOf(':');
        data2 = (sep > -1) ? display.left(sep) : display;
    }

    // 4) 获取并处理 data4（原命令数据：十进制 -> 保持前导零个数的HEX大写；否则原样）
    const QString data3Raw = ui->lineEdit4->text();
    QString data3 = data3Raw;
    static const QRegularExpression kOnlyDigitsRe(QStringLiteral("^[0-9]+$"));
    if (kOnlyDigitsRe.match(data3Raw).hasMatch()) {
        bool ok = false;
        const int dec = data3Raw.toInt(&ok, 10);
        if (ok && dec >= 0) {
            // 先转换为基础HEX
            QString baseHex = QString("%1").arg(dec, 0, 16).toUpper();
            
            // 计算原输入的前导零个数：去掉前导零后的长度差
            QString trimmed = data3Raw;
            while (trimmed.startsWith('0') && trimmed.length() > 1) {
                trimmed.remove(0, 1);
            }
            const int leadingZeros = data3Raw.length() - trimmed.length();
            
            // 在HEX前补上相同数量的前导零
            data3 = QString("0").repeated(leadingZeros) + baseHex;
        }
    }

    // 5) 获取并处理 data5（原帧尾）
    const QString data4Raw = ui->comboBox6->currentText();
    const QString data4    = (data4Raw == "0D0A") ? QString("\r\n") : data4Raw;

    // 6) 构建内容并获取CRC
    const QString frameContent = dataHead + data1 + data2 + data3;
    
    // 复用已计算的CRC（来自updateCrcDisplay），避免重复计算
    QString crcStr = ui->lineEdit5->text();

    if (crcStr.isEmpty() && m_crcHelper)
    {
        // 如果CRC未计算，则计算一次
        crcStr = m_crcHelper->calculateAndFormatCrc(frameContent);
    }

    // 7) 拼接完整帧并返回
    return frameContent + crcStr + data4;
}
