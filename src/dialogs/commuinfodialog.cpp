#include "CommuInfoDialog.h"
#include "TcpClient.h"

#include <QLayout>
#include <QLabel>
#include <QTcpSocket>
#include <QNetworkProxy>
#include <QTimer>

CommuInfoDialog* CommuInfoDialog::m_instance = nullptr;
CommuInfoDialog* CommuInfoDialog::getInstance() {
    return m_instance;
}

CommuInfoDialog::CommuInfoDialog(TcpClient* tcpClient, QWidget* parent) :
    QDialog(parent),
    m_tcpClient(tcpClient)
{
    m_instance = this;
    setWindowTitle("调试");
    resize(800, 600);
    setWindowFlags(Qt::Dialog | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);

    QHBoxLayout* hLayout = new QHBoxLayout;
    hLayout->setSpacing(0);
    hLayout->setContentsMargins(0, 0, 0, 0);
    QVBoxLayout* layout = new QVBoxLayout;
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);
    textEdit = new QTextEdit(this);
    textEdit->setReadOnly(true);

    QPushButton* clearBtn = new QPushButton("清空");
    clearBtn->setFixedWidth(80);
    QPushButton* tagBtn = new QPushButton("标记");
    tagBtn->setFixedWidth(80);
    QHBoxLayout* btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    btnLayout->addWidget(tagBtn);
    btnLayout->addWidget(clearBtn);

    QHBoxLayout* labelLayout = new QHBoxLayout;
    QLabel* tcpStatusTitleLabel = new QLabel("TCP状态：");
    tcpStatusLabel = new QLabel("离线");
    labelLayout->addWidget(tcpStatusTitleLabel);
    labelLayout->addWidget(tcpStatusLabel);
    labelLayout->addStretch();

    layout->addLayout(labelLayout);
    layout->addWidget(textEdit);
    layout->addLayout(btnLayout);

    // 右侧控制面板布局
    QVBoxLayout* controlLayout = new QVBoxLayout;
    QPushButton* testBtn = new QPushButton("连接");
    QPushButton* testBtn2 = new QPushButton("复位");
    QPushButton* testBtn3 = new QPushButton("断开");

    QPushButton* testBtn_Y_Rel_P = new QPushButton("Y_Rel_P");
    QPushButton* testBtn_Y_Rel_N = new QPushButton("Y_Rel_N");

    controlLayout->addWidget(testBtn);
    controlLayout->addWidget(testBtn3);
    controlLayout->addWidget(testBtn2);
    controlLayout->addWidget(testBtn_Y_Rel_P);
    controlLayout->addWidget(testBtn_Y_Rel_N);
    controlLayout->addStretch();

    hLayout->addLayout(layout);
    hLayout->addLayout(controlLayout);
    setLayout(hLayout);

    connect(clearBtn, &QPushButton::clicked, this, &CommuInfoDialog::clickClearMsgBtn);
    connect(tagBtn, &QPushButton::clicked, this, &CommuInfoDialog::clickTagBtn);
    connect(testBtn, &QPushButton::clicked, this, &CommuInfoDialog::clickConnectionBtn);
    connect(testBtn2, &QPushButton::clicked, this, &CommuInfoDialog::clickBtn_ResetPos);
    connect(testBtn3, &QPushButton::clicked, this, &CommuInfoDialog::clickDisconnectBtn);

    connect(testBtn_Y_Rel_P, &QPushButton::clicked, this, &CommuInfoDialog::clickBtn_Y_Rel_P);
    connect(testBtn_Y_Rel_N, &QPushButton::clicked, this, &CommuInfoDialog::clickBtn_Y_Rel_N);
}

CommuInfoDialog::~CommuInfoDialog() {

}

// void CommuInfoDialog::init(TcpClient* tcpSocket2) {
//     tcpSocket = tcpSocket2;
//     // connect(tcpSocket, &QTcpSocket::connected, this, &CommuInfoDialog::onConnected);
//     // connect(tcpSocket, &QTcpSocket::errorOccurred, this, &CommuInfoDialog::onConnectionError);
// }

void CommuInfoDialog::clickClearMsgBtn() {
    textEdit->clear();
}

void CommuInfoDialog::clickTagBtn() {
    textEdit->append("-------------------------------------------------"
                     "-------------------------------------------------");
}

void CommuInfoDialog::clickConnectionBtn() {
    m_tcpClient->connectToHost();
}

void CommuInfoDialog::clickBtn_ResetPos() {
    m_tcpClient->sendCommand(">09GA15F");
}

void CommuInfoDialog::clickBtn_Y_Rel_P() {
    m_tcpClient->sendCommand(">09D0000D0004E97");
}

void CommuInfoDialog::clickBtn_Y_Rel_N() {
    m_tcpClient->sendCommand(">09hFFFFF00013889F1E");
}

void CommuInfoDialog::clickDisconnectBtn() {
    if (tcpStatusLabel->text() == "在线") {
        m_tcpClient->disconnectFromHost();
        tcpStatusLabel->setText("离线");
        printMsg("断开连接...");
    }
}

// void CommuInfoDialog::onConnected() {
//     printMsg("TCP连接成功");
//     tcpStatusLabel->setText("在线");
//     qDebug() << "CommuInfoDialog::onConnected";
//     isConnecting = false;
// }

// void CommuInfoDialog::onConnectionError() {
//     qDebug() << "CommuInfoDialog::onConnectionError";
//     printMsg("TCP连接失败：" + tcpSocket->errorString());

//     isConnecting = false;
// }

void CommuInfoDialog::printMsg(const QString& msg, MsgType msgType) const {
    if (msgType == NONE_TYPE) {
        textEdit->append(msg);
    } else if (msgType == MSG_SEND) {
        textEdit->append(QString("T: %1").arg(msg));
    } else if (msgType == MSG_SEND_ASYNC_1) {
        textEdit->append(QString("T async 1: %1").arg(msg));
    } else if (msgType == MSG_SEND_ASYNC_2) {
        textEdit->append(QString("T async 2: %1").arg(msg));
    } else if (msgType == MSG_READ) {
        QString msg2 = msg.trimmed();
        textEdit->append(QString("R: %1").arg(msg2));
    } else if (msgType == MSG_READ_BALANCE) {
        textEdit->append(QString("R balance: %1").arg(msg));
    }
}
