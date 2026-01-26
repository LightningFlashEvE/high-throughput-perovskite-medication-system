#include "StatusRequest.h"
#include "CommuInfoDialog.h"
#include <QDebug>
#include <QTcpSocket>
#include <QNetworkProxy>


using CID = CommuInfoDialog;

StatusRequest::StatusRequest(QObject* parent) : QObject(parent)
{
    m_tcpSocket = new QTcpSocket(this);
    m_tcpSocket->setProxy(QNetworkProxy::NoProxy);
    m_tcpSocket->connectToHost("192.168.10.30", 4567);
    if (m_tcpSocket->waitForConnected(3000)) {

    } else {
        qWarning() << "TCP连接失败:" << m_tcpSocket->errorString();
    }

    connect(m_tcpSocket, &QTcpSocket::connected, this, &StatusRequest::onConnected);
    connect(m_tcpSocket, &QTcpSocket::errorOccurred, this, &StatusRequest::onConnectionError);
    connect(m_tcpSocket, &QTcpSocket::disconnected, this, &StatusRequest::onDisconnected);
}

StatusRequest::~StatusRequest() {

}

void StatusRequest::onConnected() {
    //tcpStatusLabel->setText("在线");
}

void StatusRequest::onConnectionError() {

}

void StatusRequest::onDisconnected() {
    //tcpStatusLabel->setText("离线");
}
