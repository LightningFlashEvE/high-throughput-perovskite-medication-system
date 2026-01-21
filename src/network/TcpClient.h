#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <QString>
#include <QTcpSocket>

class CommuInfoDialog;

class TcpClient : public QTcpSocket {
    Q_OBJECT
public:
    static TcpClient* getInstance();
    TcpClient(const QString& ip, int port);
    ~TcpClient();

    //void init(const QString& ip, int port);
    void setCommuInfoDialog(CommuInfoDialog* dialog);

    void connectToHost();
    void sendCommand(const QString& cmd);

private slots:
    void onConnected();
    void onConnectionError();

private:
    static TcpClient* m_instance;
    CommuInfoDialog* m_commuInfoDialog{};
    QString m_ip;
    int m_port{0};
};

#endif // TCPCLIENT_H
