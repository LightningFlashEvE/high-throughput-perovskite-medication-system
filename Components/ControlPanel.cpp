#include "ControlPanel.h"
#include "CommuInfoDialog.h"

#include <QDebug>
#include <QTcpSocket>
#include <QLabel>
#include <QGroupBox>
#include <QLayout>
#include <QGroupBox>
#include <QPushButton>

using CID = CommuInfoDialog;

ControlPanel::ControlPanel(QTcpSocket* tcpSocket, QWidget* parent) :
    QWidget(parent),
    m_tcpSocket(tcpSocket)
{
    //setStyleSheet("background-color: #11ffff00;");

    QGroupBox* groupBox = new QGroupBox("操控面板");
    QVBoxLayout* vLayout = new QVBoxLayout;

    QGridLayout* gridLayout = new QGridLayout;

    QLabel* label00 = new QLabel("xy平面：");
    QLabel* label01 = new QLabel("ControlPannel");
    QLabel* label02 = new QLabel("ControlPannel");

    QLabel* label10 = new QLabel("ControlPannel");
    QLabel* label11 = new QLabel("ControlPannel");
    QLabel* label12 = new QLabel("ControlPannel");


    QLabel* label20 = new QLabel("ControlPannel");
    QLabel* label21 = new QLabel("ControlPannel");
    QLabel* label22 = new QLabel("ControlPannel");

    QPushButton* btn01 = new QPushButton("上");
    QPushButton* btn10 = new QPushButton("左");
    QPushButton* btn12 = new QPushButton("右");
    QPushButton* btn21 = new QPushButton("下");

    QLabel* label30 = new QLabel("z爪：");
    QLabel* label31 = new QLabel("ControlPannel");
    QLabel* label41 = new QLabel("ControlPannel");

    QPushButton* btn31 = new QPushButton("上");
    QPushButton* btn41 = new QPushButton("下");
    QPushButton* btn32 = new QPushButton("开爪");
    QPushButton* btn42 = new QPushButton("闭爪");

    QLabel* label50 = new QLabel("x复位：");
    QLabel* label60 = new QLabel("y复位：");
    QLabel* label70 = new QLabel("z复位：");

    QLabel* label51 = new QLabel("x");
    QLabel* label61 = new QLabel("y");
    QLabel* label71 = new QLabel("z");

    QPushButton* btn51 = new QPushButton("x复位");
    QPushButton* btn61 = new QPushButton("y复位");
    QPushButton* btn71 = new QPushButton("z复位");

    // QPushButton* btn01 = new QPushButton("上");
    // QPushButton* btn10 = new QPushButton("左");
    // QPushButton* btn12 = new QPushButton("右");
    // QPushButton* btn21 = new QPushButton("下");

    gridLayout->addWidget(label00, 0, 0);
    gridLayout->addWidget(btn01, 0, 1);
    gridLayout->addWidget(label02, 0, 2);
    gridLayout->addWidget(btn10, 1, 0);
    gridLayout->addWidget(label11, 1, 1);
    gridLayout->addWidget(btn12, 1, 2);
    gridLayout->addWidget(label20, 2, 0);
    gridLayout->addWidget(btn21, 2, 1);
    gridLayout->addWidget(label22, 2, 2);

    gridLayout->addWidget(label30, 3, 0);
    gridLayout->addWidget(btn31, 3, 1);
    gridLayout->addWidget(btn41, 4, 1);
    gridLayout->addWidget(btn32, 3, 2);
    gridLayout->addWidget(btn42, 4, 2);

    gridLayout->addWidget(label50, 5, 0);
    gridLayout->addWidget(label60, 6, 0);
    gridLayout->addWidget(label70, 7, 0);

    gridLayout->addWidget(btn51, 5, 1);
    gridLayout->addWidget(btn61, 6, 1);
    gridLayout->addWidget(btn71, 7, 1);

    //vLayout->addLayout(gridLayout);
    vLayout->addWidget(groupBox);
    vLayout->addStretch();

    groupBox->setLayout(gridLayout);

    setLayout(vLayout);

    registerBtn(btn21, MOV_Y_P);
}

void ControlPanel::registerBtn(QPushButton* btn, ButtonType btnType) {
    m_buttons[btn] = btnType;
    connect(btn, &QPushButton::clicked, this, &ControlPanel::clickAnyBtn);
}

void ControlPanel::clickAnyBtn() {
    if (m_tcpSocket->state() != QAbstractSocket::ConnectedState) {
        // tcp未连接，不做任何处理
        return;
    }

    // 根据发送者判断是哪个按钮
    QPushButton* clickedBtn = qobject_cast<QPushButton*>(sender());
    if (!clickedBtn) return;

    ButtonType btnType = m_buttons[clickedBtn];
    switch(btnType) {
    case MOV_Y_P:
        sendCommand(">09D0000D0004E97");
        break;
    default:
        break;
    }
}

void ControlPanel::sendCommand(const QString& cmd) {
    //qDebug() << "T:" << cmd;
    m_tcpSocket->write(cmd.toStdString().c_str());
    CID::getInstance()->printMsg(cmd, CommuInfoDialog::MSG_SEND);

    if (!m_tcpSocket->waitForReadyRead()) {
        CID::getInstance()->printMsg("TCP读取超时！");
        return;
        //Q_ASSERT(false);
    }

    QByteArray data = m_tcpSocket->readAll();
    CID::getInstance()->printMsg(data, CommuInfoDialog::MSG_READ);
    //qDebug() << "R:" << data;
}
