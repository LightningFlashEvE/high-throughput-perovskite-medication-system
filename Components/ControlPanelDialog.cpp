#include "ControlPanelDialog.h"
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

    QHBoxLayout* rontrolPanelLayout = new QHBoxLayout;
    m_rightControlPanel = new ControlPanel(m_tcpSocket, this);
    m_leftControlPanel = new ControlPanel(m_tcpSocket, this);

    rontrolPanelLayout->addWidget(m_rightControlPanel);
    rontrolPanelLayout->addWidget(m_leftControlPanel);

    rootLayout->addLayout(rontrolPanelLayout);
}
