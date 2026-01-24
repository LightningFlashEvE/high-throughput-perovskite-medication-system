#include "ControlPanelDialog.h"
#include "ControlPanel_L.h"
#include "ControlPanel.h"

#include <QLayout>
#include <QLabel>
#include <QTcpSocket>

ControlPanelDialog* ControlPanelDialog::m_ptr = nullptr;

ControlPanelDialog* ControlPanelDialog::Ptr() {
    return m_ptr;
}

ControlPanelDialog::ControlPanelDialog(QTcpSocket* tcpSocket, QWidget* parent)
    : QDialog(parent), m_tcpSocket(tcpSocket)
{
    m_ptr = this;

    resize(400, 300);
    setWindowFlags(Qt::Dialog | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);

    QVBoxLayout* rootLayout = new QVBoxLayout;
    setLayout(rootLayout);
    QHBoxLayout* tcpStatusLayout = new QHBoxLayout;


    m_tcpStatusLabel = new QLabel();
    onDisconnected();// 初始状态离线
    tcpStatusLayout->addWidget(new QLabel("TCP状态:"));
    tcpStatusLayout->addWidget(m_tcpStatusLabel);
    tcpStatusLayout->addStretch();
    rootLayout->addLayout(tcpStatusLayout);

    QHBoxLayout* rontrolPanelLayout = new QHBoxLayout;
    m_leftControlPanel = new ControlPanel_L(m_tcpSocket, this);
    m_rightControlPanel = new ControlPanel(m_tcpSocket, this);

    rontrolPanelLayout->addWidget(m_leftControlPanel);
    rontrolPanelLayout->addWidget(m_rightControlPanel);

    rootLayout->addLayout(rontrolPanelLayout);

    connect(m_tcpSocket, &QTcpSocket::connected, this, &ControlPanelDialog::onConnected);
    connect(m_tcpSocket, &QTcpSocket::errorOccurred, this, &ControlPanelDialog::onConnectionError);
    connect(m_tcpSocket, &QTcpSocket::disconnected, this, &ControlPanelDialog::onDisconnected);
}

void ControlPanelDialog::onConnected() {
    m_tcpStatusLabel->setText("在线");
    m_tcpStatusLabel->setStyleSheet("color: #11FF11;");
}
void ControlPanelDialog::onConnectionError() {

}
void ControlPanelDialog::onDisconnected() {
    m_tcpStatusLabel->setText("离线");
    m_tcpStatusLabel->setStyleSheet("color: red;");
}
