#ifndef TCPCAMERA_H
#define TCPCAMERA_H

#include <QObject>
#include <QTcpSocket>
#include <QString>
#include <QByteArray>
#include <QTimer>
#include "DatabaseOnline.h"

// 前向声明
class DatabaseOnline;

class tcpCamera : public QObject
{
    Q_OBJECT
public:
    explicit tcpCamera(QObject *parent = nullptr);
    ~tcpCamera();

    // 连接到相机服务器
    // 默认为 ip = "192.168.10.30", port = 4567, useProxy = false（不使用代理）
    bool tcpCameraConnect(const QString &ip = QStringLiteral("192.168.10.30"),
                          quint16 port = 4567,
                          bool useProxy = false);

    // 断开连接
    void tcpCameraDisconnect();

    // 发送消息
    bool tcpCameraSend(const QByteArray &msg);
    bool tcpCameraSend(const QString &msg);

    // 获取连接状态
    bool isConnected() const;

    // 启动/停止定时器
    // intervalMs: 定时时间（毫秒），enabled: true 启动 / false 关闭
    void startTime(int intervalMs, bool enabled);

signals:
    // 收到数据信号
    void dataReceived(const QByteArray &data);

    // 连接状态变化信号
    void connected();
    void disconnected();

    // 错误信号
    void errorOccurred(const QString &errorMsg);

private slots:
    // 接收数据槽函数
    void onReadyRead();

    // 连接成功槽函数
    void onConnected();

    // 断开连接槽函数
    void onDisconnected();

    // 错误处理槽函数
    void onErrorOccurred(QAbstractSocket::SocketError socketError);

    // 定时器超时槽函数（定时器执行内容你可以在 cpp 中填写）
    void onTimerTimeout();

private:
    QTcpSocket *m_socket;
    bool m_isConnected;
    QTimer *m_timer{nullptr};
    DatabaseOnline *m_database{nullptr};  // 数据库操作对象
};

#endif // TCPCAMERA_H
