#ifndef NETWORKSETTINGSDIALOG_H
#define NETWORKSETTINGSDIALOG_H

#include <QDialog>

class QLineEdit;
class QPushButton;
class AppSqlDatabase;
class TcpClientCore;

class NetworkSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit NetworkSettingsDialog(AppSqlDatabase *dbm,
                                   TcpClientCore  *tcpCore,
                                   TcpClientCore  *tcpBalanceCore,
                                   QWidget        *parent = nullptr);

private slots:
    void onDbConnect();
    void onDbDisconnect();
    void onTcpCoreConnect();
    void onTcpCoreDisconnect();
    void onTcpBalanceConnect();
    void onTcpBalanceDisconnect();
    void onAutoFindLocalIP();

private:
    void loadFromIni();
    void saveDbToIni();
    void saveTcpToIni();

    // 数据库字段
    QLineEdit *m_dbHost;
    QLineEdit *m_dbPort;
    QLineEdit *m_dbDatabase;
    QLineEdit *m_dbUser;
    QLineEdit *m_dbPassword;

    // TCP字段
    QLineEdit *m_localIP;
    QLineEdit *m_tcpCoreRemoteIP;
    QLineEdit *m_tcpCoreRemotePort;
    QLineEdit *m_tcpBalanceRemoteIP;
    QLineEdit *m_tcpBalanceRemotePort;

    AppSqlDatabase *m_dbm;
    TcpClientCore  *m_tcpCore;
    TcpClientCore  *m_tcpBalanceCore;
};

#endif // NETWORKSETTINGSDIALOG_H
