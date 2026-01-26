#ifndef STATUSREQUEST_H
#define STATUSREQUEST_H

#include <QObject>

class QTcpSocket;

class StatusRequest : public QObject
{
    Q_OBJECT
public:
    explicit StatusRequest(QObject* parent = nullptr);
    ~StatusRequest();

private slots:
    void onConnected();
    void onConnectionError();
    void onDisconnected();

private:
    QTcpSocket* m_tcpSocket;
};

#endif // STATUSREQUEST_H
