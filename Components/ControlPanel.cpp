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

    QGroupBox* groupBox = new QGroupBox("右机械臂");
    QVBoxLayout* groupLayout = new QVBoxLayout;

    QHBoxLayout* tcpStatusHLayout = new QHBoxLayout;
    tcpStatusHLayout->addWidget(new QLabel("TCP状态："));
    m_tcpStatusLabel = new QLabel("离线");
    m_tcpStatusLabel->setStyleSheet("color: red;");
    tcpStatusHLayout->addWidget(m_tcpStatusLabel);
    tcpStatusHLayout->addStretch();

    QVBoxLayout* vLayout = new QVBoxLayout;
    QGridLayout* gridLayout = new QGridLayout;

    QLabel* label00 = new QLabel("xy平面：");
    //QLabel* label01 = new QLabel("ControlPannel");
    //QLabel* label02 = new QLabel("ControlPannel");

    // QLabel* label10 = new QLabel("ControlPannel");
    // QLabel* label11 = new QLabel("ControlPannel");
    // QLabel* label12 = new QLabel("ControlPannel");


    // QLabel* label20 = new QLabel("ControlPannel");
    // QLabel* label21 = new QLabel("ControlPannel");
    // QLabel* label22 = new QLabel("ControlPannel");

    QPushButton* btn01 = new QPushButton("前");
    QPushButton* btn10 = new QPushButton("左");
    QPushButton* btn12 = new QPushButton("右");
    QPushButton* btn21 = new QPushButton("后");

    QPushButton* btn02 = new QPushButton("释放使能");

    QLabel* label30 = new QLabel("z爪：");
    // QLabel* label31 = new QLabel("ControlPannel");
    // QLabel* label41 = new QLabel("ControlPannel");

    QPushButton* btn31 = new QPushButton("升");
    QPushButton* btn41 = new QPushButton("降");
    QPushButton* btn32 = new QPushButton("开爪");
    QPushButton* btn42 = new QPushButton("闭爪");

    QLabel* label50 = new QLabel("z吸液：");
    QPushButton* btn51 = new QPushButton("升");
    QPushButton* btn61 = new QPushButton("降");

    QLabel* label70 = new QLabel("x复位：");
    QLabel* label80 = new QLabel("y复位：");
    QLabel* label90 = new QLabel("z爪复位：");


    // QLabel* label71 = new QLabel("x");
    // QLabel* label81 = new QLabel("y");
    // QLabel* label91 = new QLabel("z");

    QPushButton* btn71 = new QPushButton("x复位");
    QPushButton* btn81 = new QPushButton("y复位");
    QPushButton* btn91 = new QPushButton("z爪复位");
    QPushButton* btn92 = new QPushButton("z吸液复位");

    // QPushButton* btn01 = new QPushButton("上");
    // QPushButton* btn10 = new QPushButton("左");
    // QPushButton* btn12 = new QPushButton("右");
    // QPushButton* btn21 = new QPushButton("下");

    gridLayout->addWidget(label00, 0, 0);
    gridLayout->addWidget(btn01, 0, 1);
    //gridLayout->addWidget(label02, 0, 2);
    gridLayout->addWidget(btn10, 1, 0);
    //gridLayout->addWidget(label11, 1, 1);
    gridLayout->addWidget(btn12, 1, 2);
    //gridLayout->addWidget(label20, 2, 0);
    gridLayout->addWidget(btn21, 2, 1);
    //gridLayout->addWidget(label22, 2, 2);

    gridLayout->addWidget(btn02, 0, 2);

    gridLayout->addWidget(label30, 3, 0);
    gridLayout->addWidget(btn31, 3, 1);
    gridLayout->addWidget(btn41, 4, 1);
    gridLayout->addWidget(btn32, 3, 2);
    gridLayout->addWidget(btn42, 4, 2);

    gridLayout->addWidget(label50, 5, 0);
    gridLayout->addWidget(btn51, 5, 1);
    gridLayout->addWidget(btn61, 6, 1);

    gridLayout->addWidget(label70, 7, 0);
    gridLayout->addWidget(label80, 8, 0);
    gridLayout->addWidget(label90, 9, 0);

    gridLayout->addWidget(btn71, 7, 1);
    gridLayout->addWidget(btn81, 8, 1);
    gridLayout->addWidget(btn91, 9, 1);
    gridLayout->addWidget(btn92, 9, 2);

    //vLayout->addLayout(gridLayout);

    //groupBox->setLayout(gridLayout);
    //vLayout->addLayout(tcpStatusHLayout);
    vLayout->addLayout(gridLayout);
    groupBox->setLayout(vLayout);

    groupLayout->addWidget(groupBox);


    groupLayout->addStretch();

    setLayout(groupLayout);

    registerBtn(btn01, MOV_Y_N);
    registerBtn(btn21, MOV_Y_P);
    registerBtnRelease(btn01, STOP_Y);
    registerBtnRelease(btn21, STOP_Y);

    registerBtn(btn10, MOV_X_P);
    registerBtn(btn12, MOV_X_N);
    registerBtnRelease(btn10, STOP_X);
    registerBtnRelease(btn12, STOP_X);

    registerBtnClick(btn02, RELEASE_ENABEL_XY);


    registerBtn(btn31, MOV_Z_UP);
    registerBtn(btn41, MOV_Z_DOWN);
    registerBtn(btn51, MOV_Z_XIYE_UP);
    registerBtn(btn61, MOV_Z_XIYE_DOWN);

    registerBtnRelease(btn31, STOP_Z);
    registerBtnRelease(btn41, STOP_Z);
    registerBtnRelease(btn51, STOP_Z);
    registerBtnRelease(btn61, STOP_Z_XIYE);

    registerBtn(btn71, RESET_POS_X);
    registerBtn(btn81, RESET_POS_Y);
    registerBtn(btn91, RESET_POS_Z);
    registerBtn(btn92, RESET_POS_Z_XIYE);

    registerBtn(btn32, CLAW_OPEN);
    registerBtn(btn42, CLAW_CLOSED);

    connect(m_tcpSocket, &QTcpSocket::connected, this, &ControlPanel::onConnected);
    connect(m_tcpSocket, &QTcpSocket::errorOccurred, this, &ControlPanel::onConnectionError);
    connect(m_tcpSocket, &QTcpSocket::disconnected, this, &ControlPanel::onDisconnected);
}

void ControlPanel::registerBtn(QPushButton* btn, ActionType pressType) {
    m_buttons[btn] = pressType;
    connect(btn, &QPushButton::pressed, this, &ControlPanel::pressAnyBtn);
}

void ControlPanel::registerBtnRelease(QPushButton* btn, ActionType pressType) {
    m_buttonRelease[btn] = pressType;
    connect(btn, &QPushButton::released, this, &ControlPanel::releaseAnyBtn);
}

void ControlPanel::registerBtnClick(QPushButton* btn, ActionType actionType) {
    m_clickButtons[btn] = actionType;
    connect(btn, &QPushButton::released, this, &ControlPanel::clickAnyBtn);
}

void ControlPanel::clickAnyBtn() {
    if (m_tcpSocket->state() != QAbstractSocket::ConnectedState) {
        // tcp未连接，不做任何处理
        return;
    }

    // 根据发送者判断是哪个按钮
    QPushButton* clickedBtn = qobject_cast<QPushButton*>(sender());
    if (!clickedBtn) return;

    ActionType btnType = NODE;
    if (m_clickButtons.contains(clickedBtn)) {
        btnType = m_clickButtons[clickedBtn];
    }

    switch(btnType) {
    case RELEASE_ENABEL_XY:
        //sendCommand(">03D0000E00854B7");
        break;
    default:
        break;
    }
}


void ControlPanel::pressAnyBtn() {
    if (m_tcpSocket->state() != QAbstractSocket::ConnectedState) {
        // tcp未连接，不做任何处理
        return;
    }

    // 根据发送者判断是哪个按钮
    QPushButton* clickedBtn = qobject_cast<QPushButton*>(sender());
    if (!clickedBtn) return;

    ActionType btnType1 = NODE;
    if (m_buttons.contains(clickedBtn)) {
        btnType1 = m_buttons[clickedBtn];
    }

    switch(btnType1) {
    case MOV_Y_P:
        sendCommand(">09D0000D0004E97");
        break;
    case MOV_Y_N:
        sendCommand(">09D00000000BE8C");
        break;
    case MOV_X_P:
        sendCommand(">0AD00006000F70F");
        break;
    case MOV_X_N:
        sendCommand(">0AD000000007F0F");
        break;
    case MOV_Z_UP:
        sendCommand(">06D000000008EBC");
        break;
    case MOV_Z_DOWN:
        sendCommand(">06D00035FD158FF");
        break;
    case MOV_Z_XIYE_UP:
        sendCommand(">08D000000007BDD");
        break;
    case MOV_Z_XIYE_DOWN:
        sendCommand(">08D0002802B9E27");
        break;
    case RESET_POS_X:
        sendCommand(">0AGA17D");
        break;
    case RESET_POS_Y:
        sendCommand(">09GA15F");
        break;
    case RESET_POS_Z:
        sendCommand(">06G515A");
        break;
    case RESET_POS_Z_XIYE:
        sendCommand(">08G315E");
        break;
    case CLAW_OPEN:
        //sendCommand(">0506010000014872");
        //sendCommand(">0506010800000870");
        sendCommandHex("05060105000099B3");
        break;
    case CLAW_CLOSED:
        sendCommandHex("0506010500649858");
        break;
    default:
        break;
    }
}

void ControlPanel::releaseAnyBtn() {
    if (m_tcpSocket->state() != QAbstractSocket::ConnectedState) {
        // tcp未连接，不做任何处理
        return;
    }

    // 根据发送者判断是哪个按钮
    QPushButton* clickedBtn = qobject_cast<QPushButton*>(sender());
    if (!clickedBtn) return;

    ActionType btnType2  = NODE;
    if (m_buttonRelease.contains(clickedBtn)) {
        btnType2 = m_buttonRelease[clickedBtn];
    }

    switch(btnType2) {
    case STOP_X:
        sendCommand(">0AK03564");
        break;
    case STOP_Y:
        sendCommand(">09K02CE4");
        break;
    case STOP_Z:
        sendCommand(">06K02FD4");
        break;
    case STOP_Z_XIYE:
        sendCommand(">08K0ECB5");
        break;
    default:
        break;
    }
}

void ControlPanel::sendCommand(const QString& cmd) {
    //qDebug() << "T:" << cmd;
    m_tcpSocket->write(cmd.toStdString().c_str());
    CID::Ptr()->printMsg(cmd, CommuInfoDialog::MSG_SEND);

    // if (!m_tcpSocket->waitForReadyRead()) {
    //     CID::getInstance()->printMsg("TCP读取超时！");
    //     return;
    //     //Q_ASSERT(false);
    // }

    QByteArray data = m_tcpSocket->readAll();
    CID::Ptr()->printMsg(data, CommuInfoDialog::MSG_READ);
    //qDebug() << "R:" << data;
}

void ControlPanel::sendCommandHex(const QByteArray& hexData) {
    //m_tcpSocket->write(QByteArray::fromHex(cmd.toStdString().c_str()));
    m_tcpSocket->write(QByteArray::fromHex(hexData));
    CID::Ptr()->printMsg(hexData, CommuInfoDialog::MSG_SEND);

    QByteArray data = m_tcpSocket->readAll();
    CID::Ptr()->printMsg(data, CommuInfoDialog::MSG_READ);
}

void ControlPanel::onConnected() {
    m_tcpStatusLabel->setText("在线");
    m_tcpStatusLabel->setStyleSheet("color: #11FF11;");
}
void ControlPanel::onConnectionError() {

}
void ControlPanel::onDisconnected() {
    m_tcpStatusLabel->setText("离线");
    m_tcpStatusLabel->setStyleSheet("color: red;");
}

