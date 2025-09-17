#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <QWidget>
#include <QTcpSocket>
#include <QTcpServer>
#include <QTimer>
#include <QNetworkInterface>
#include <QNetworkProxy>
#include <QMap>
#include <QJsonObject>
#include <QJsonDocument>
#include "protocolbase.h"

// 前向声明
class TcpClientCrc;

namespace Ui {
class TcpClient;
}

// 旧的协议结构已删除，使用新的模块化ProtocolBase系统

class TcpClient : public QWidget
{
    Q_OBJECT

public:
    explicit TcpClient(QWidget *parent = nullptr);
    ~TcpClient();

private slots:
    void onModeChanged();
    void onConnectClicked();
    void onDisconnectClicked();
    void onSendClicked();
    void onClearClicked();
    void onNewConnection();
    void onClientConnected();
    void onClientDisconnected();
    void onDataReceived();
    void onSocketError(QAbstractSocket::SocketError error);
    void updateLocalIPs();
    
    void onProtocolTypeChanged(); // 更换协议
    
    // 帧结构编辑器槽函数
    void onFrameFieldChanged();
    //void onBuildFrameClicked();
    void onPreviewModeChanged();
    
    // 数据解析功能已整合到模块化协议系统中

public:
    void setupConnections();
    void initializeNetworkObjects();
    void updateProxySettings();
    void updateUI();
    void appendMessage(const QString &message, const QString &type = "info");
    void updateConnectionInfo();
    QString getCurrentTimestamp();
    void refreshLocalIPs();
    
    // 旧协议管理方法已删除，使用新的模块化系统
    //quint16 calculateCRC16(const QByteArray &data);
    QString formatFrameForDisplay(const QByteArray &frame, bool hexDisplay = true);
    
    // 帧结构编辑器方法
    void initializeFrameBuilder();
    void syncFrameFields(); // 更新combox值, 初始化会调用，选择协议会调用
    QString buildFrameFromFields();
    //void updateCrcDisplay();
    QString formatFrameASCII(const QString &frame);
    QString formatFrameHex(const QString &frame);
    
    // UI控件访问器方法 (供CRC辅助类使用)
    QString getComboBox1Text() const;
    QString getComboBox2Text() const;
    QString getComboBox3Text() const;
    QString getComboBox3Data() const;
    QString getLineEdit4Text() const;
    void setLineEdit5Text(const QString &text);
    void clearLineEdit5();
    QString getCurrentProtocolName() const;
    
    void initializeProtocolSystem();
    void updateProtocolUI();// 更新眉头
    void populateProtocolComboBoxes();
    void applyProtocolFields();

private:
    Ui::TcpClient *ui;
    
    // 网络相关
    QTcpSocket *m_tcpSocket;
    QTcpServer *m_tcpServer;
    QList<QTcpSocket*> m_clientSockets;
    
    // 状态管理
    bool m_isConnected;
    bool m_isServerMode;
    QString m_currentConnectionInfo;
    
    // 定时器
    QTimer *m_updateTimer;
    
    // 协议管理 (新的模块化系统) 基类指针指向派生类。
    // 多态:基类指针指向基类对象时就使用基类的成员（包括成员函数和成员变量），指向派生类对象时就使用派生类的成员。
    ProtocolBase* m_currentProtocol; // 更换协议的时候会赋值
    QString m_currentProtocolName; // 更换协议的时候会赋值
    
    // CRC计算辅助类
    TcpClientCrc* m_crcHelper;
};

#endif // TCPCLIENT_H
