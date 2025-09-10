#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <QWidget>
#include <QTcpSocket>
#include <QTimer>
#include <QHostAddress>

QT_BEGIN_NAMESPACE
class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QLineEdit;
class QSpinBox;
class QTextEdit;
class QPushButton;
class QCheckBox;
class QLabel;
class QGroupBox;
QT_END_NAMESPACE

/**
 * TcpClient - 专业的TCP客户端调试工具
 * 
 * 功能特性：
 * - TCP连接管理（连接/断开/重连）
 * - 远端和本地IP/端口配置
 * - 实时连接状态显示
 * - 数据发送（支持16进制/字符串模式）
 * - 接收数据日志显示
 * - 连接状态可视化指示
 * - 自动重连功能
 * - 数据统计显示
 */
class TcpClient : public QWidget
{
    Q_OBJECT

public:
    explicit TcpClient(QWidget *parent = nullptr);
    ~TcpClient();

    // 公共接口
    bool connectToHost(const QString &host, quint16 port);
    bool connectToHost(const QString &host, quint16 port, const QString &localHost, quint16 localPort);
    void disconnectFromHost();
    bool isConnected() const;
    
    // 数据发送
    void sendData(const QByteArray &data);
    void sendText(const QString &text);
    void sendHexString(const QString &hexString);
    
    // 获取连接信息
    QString getRemoteAddress() const;
    quint16 getRemotePort() const;
    QString getLocalAddress() const;
    quint16 getLocalPort() const;
    
    // 日志管理
    void clearLog();
    QString getLogContent() const;
    
    // 设置选项
    void setAutoReconnect(bool enabled);
    void setReconnectInterval(int milliseconds);

signals:
    // 连接状态信号
    void connected();
    void disconnected();
    void connectionError(const QString &error);
    
    // 数据信号
    void dataReceived(const QByteArray &data);
    void dataSent(const QByteArray &data);
    
    // 状态信号
    void statusChanged(const QString &status);

private slots:
    // 内部槽函数
    void onConnectClicked();
    void onSendClicked();
    void onClearLogClicked();
    void onHexModeChanged(bool hexMode);
    
    // TCP事件处理
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void onSocketDataReady();
    
    // 重连定时器
    void onReconnectTimer();
    
    // UI更新
    void updateConnectionStatus(bool connected);
    void updateDataStatistics();

private:
    // UI组件
    void setupUI();
    void createConnectionGroup();
    void createControlGroup();
    void createLogGroup();
    void createStatusGroup();
    
    // 工具函数
    QString formatByteArray(const QByteArray &data, bool asHex = false) const;
    QByteArray parseHexString(const QString &hexString) const;
    void appendLog(const QString &message, const QString &prefix = "", const QColor &color = QColor());
    void setButtonStyle(QPushButton *button, bool success);
    
    // 网络组件
    QTcpSocket *m_socket;
    QTimer *m_reconnectTimer;
    
    // UI组件
    QVBoxLayout *m_mainLayout;
    
    // 连接配置组
    QGroupBox *m_connectionGroup;
    QLineEdit *m_remoteHostEdit;
    QSpinBox *m_remotePortSpin;
    QLineEdit *m_localHostEdit;
    QSpinBox *m_localPortSpin;
    QPushButton *m_connectButton;
    QLabel *m_connectionStatusLabel;
    
    // 控制组
    QGroupBox *m_controlGroup;
    QTextEdit *m_sendEdit;
    QPushButton *m_sendButton;
    QCheckBox *m_hexModeCheck;
    QPushButton *m_clearLogButton;
    
    // 日志组
    QGroupBox *m_logGroup;
    QTextEdit *m_logEdit;
    
    // 状态组
    QGroupBox *m_statusGroup;
    QLabel *m_statusLabel;
    QLabel *m_bytesSentLabel;
    QLabel *m_bytesReceivedLabel;
    QLabel *m_connectionTimeLabel;
    
    // 状态变量
    bool m_isConnected;
    bool m_autoReconnect;
    int m_reconnectInterval;
    qint64 m_bytesSent;
    qint64 m_bytesReceived;
    QDateTime m_connectionTime;
    
    // 配置
    QString m_lastRemoteHost;
    quint16 m_lastRemotePort;
    QString m_lastLocalHost;
    quint16 m_lastLocalPort;
};

#endif // TCPCLIENT_H