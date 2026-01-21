#include "ControlPanel.h"

#include <QTcpSocket>
#include <QLabel>
#include <QGroupBox>
#include <QLayout>

ControlPanel::ControlPanel(QTcpSocket* tcpSocket, QWidget* parent) :
    QWidget(parent),
    m_tcpSocket(tcpSocket)
{
    QGroupBox* groupBox = new QGroupBox("操控面板");
    QVBoxLayout* vLayout = new QVBoxLayout;
    vLayout->addWidget(groupBox);

    setLayout(vLayout);
}
