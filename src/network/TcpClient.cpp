#include "TcpClient.h"
#include "CommuInfoDialog.h"

#include <QTimer>
#include <QNetworkProxy>

TcpClient* TcpClient::m_instance = nullptr;
TcpClient* TcpClient::getInstance() {
    return m_instance;
}

TcpClient::TcpClient(const QString& ip, int port) {
    m_ip = ip;
    m_port = port;

    setProxy(QNetworkProxy::NoProxy);

    connect(this, &QTcpSocket::connected, this, &TcpClient::onConnected);
    connect(this, &QTcpSocket::errorOccurred, this, &TcpClient::onConnectionError);
}

TcpClient::~TcpClient() {

}

void TcpClient::setCommuInfoDialog(CommuInfoDialog* dialog) {
    m_commuInfoDialog = dialog;
}

void TcpClient::connectToHost() {
    QTcpSocket::connectToHost(m_ip, m_port);
    m_commuInfoDialog->printMsg("正连接服务器，请等待...");

    // 3秒的连接时间
    QTimer::singleShot(3000, this, [this](){
        qDebug() << "singleShot";
        if (state() != QAbstractSocket::ConnectedState) {
            abort(); // 中止连接尝试
            m_commuInfoDialog->printMsg("TCP连接失败：超时");
        }
    });

}

void TcpClient::sendCommand(const QString& cmd) {
    if (state() != QAbstractSocket::ConnectedState) {
        return;
    }

    write(cmd.toStdString().c_str());
    m_commuInfoDialog->printMsg(cmd, CommuInfoDialog::MSG_SEND);

    if (!waitForReadyRead()) {
        m_commuInfoDialog->printMsg("TCP读取超时！");
        Q_ASSERT(false);
    }

    QByteArray data = readAll();
    m_commuInfoDialog->printMsg(data, CommuInfoDialog::MSG_READ);
}

void TcpClient::onConnected() {
    m_commuInfoDialog->printMsg("TCP连接成功");
    //qDebug() << "CommuInfoDialog::onConnected";
}

void TcpClient::onConnectionError() {
    //m_commuInfoDialog->printMsg("TCP连接错误！");
    qDebug() << "onConnectionError: " << errorString();
    m_commuInfoDialog->printMsg(errorString());
}

