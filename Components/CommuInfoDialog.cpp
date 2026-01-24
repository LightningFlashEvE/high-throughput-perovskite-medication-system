#include "CommuInfoDialog.h"
#include "HttpRequest.h"

#include <QLayout>
#include <QLabel>
#include <QTcpSocket>
#include <QNetworkProxy>
#include <QTimer>

CommuInfoDialog* CommuInfoDialog::m_ptr = nullptr;
CommuInfoDialog* CommuInfoDialog::Ptr() {
    return m_ptr;
}

CommuInfoDialog::CommuInfoDialog(QTcpSocket* tcpSocket, QWidget* parent) :
    QDialog(parent),
    m_tcpSocket(tcpSocket)
{
    m_ptr = this;
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
    QPushButton* startRecvBtn = new QPushButton("开始接收");
    startRecvBtn->setFixedWidth(80);
    QPushButton* stopRecvBtn = new QPushButton("停止");
    stopRecvBtn->setFixedWidth(80);

    QHBoxLayout* btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    btnLayout->addWidget(tagBtn);
    btnLayout->addWidget(clearBtn);
    btnLayout->addWidget(startRecvBtn);
    btnLayout->addWidget(stopRecvBtn);

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
    connect(startRecvBtn, &QPushButton::clicked, this, [this](){m_isStopRecv = false;});
    connect(stopRecvBtn, &QPushButton::clicked, this, [this](){m_isStopRecv = true;});


    connect(testBtn_Y_Rel_P, &QPushButton::clicked, this, &CommuInfoDialog::clickBtn_Y_Rel_P);
    connect(testBtn_Y_Rel_N, &QPushButton::clicked, this, &CommuInfoDialog::clickBtn_Y_Rel_N);

    connect(m_tcpSocket, &QTcpSocket::connected, this, &CommuInfoDialog::onConnected);
    connect(m_tcpSocket, &QTcpSocket::errorOccurred, this, &CommuInfoDialog::onConnectionError);
    connect(m_tcpSocket, &QTcpSocket::disconnected, this, &CommuInfoDialog::onDisconnected);
}

CommuInfoDialog::~CommuInfoDialog() {

}

void CommuInfoDialog::registerBtn(QPushButton* btn) {
    connect(btn, &QPushButton::clicked, this, &CommuInfoDialog::clickAnyBtn);
}

void CommuInfoDialog::clickAnyBtn() {

}

void CommuInfoDialog::clickClearMsgBtn() {
    // 测试http请求
    //HttpRequest* httpRequest = new HttpRequest(this);
    //httpRequest->sendRequest();

    textEdit->clear();
}

void CommuInfoDialog::clickTagBtn() {
    textEdit->append("-------------------------------------------------"
                     "-------------------------------------------------");
}

void CommuInfoDialog::clickConnectionBtn() {
    //connectToHost();
    printMsg("clickConnectionBtn");
    m_tcpSocket->connectToHost("192.168.5.201", 4196);
    // 等待连接建立（最多3秒）
    if (m_tcpSocket->waitForConnected(3000)) {
        printMsg("TCP连接成功");
    } else {
        qWarning() << "TCP连接失败:" << m_tcpSocket->errorString();
    }
}

void CommuInfoDialog::clickBtn_ResetPos() {
    sendCommand(">09GA15F");
}

void CommuInfoDialog::clickBtn_Y_Rel_P() {
    sendCommand(">09D0000D0004E97");
}

void CommuInfoDialog::clickBtn_Y_Rel_N() {
    sendCommand(">09hFFFFF00013889F1E");
}

void CommuInfoDialog::clickDisconnectBtn() {
    if (tcpStatusLabel->text() == "在线") {
        m_tcpSocket->disconnectFromHost();
        tcpStatusLabel->setText("离线");
        printMsg("断开连接...");
    }
}

void CommuInfoDialog::setSocket(QTcpSocket* tcpSocket) {
    m_tcpSocket = tcpSocket;
}

void CommuInfoDialog::printMsg(const QString& msg, MsgType msgType) const {
    if (m_isStopRecv ||
        msgType == DEBUD_onBalanceReadyRead ||
        msgType == DEBUG_pollMotorPosition ||
        msgType == MSG_DEBUD ||
        msgType == MSG_ORIGIN_TCP) {
        return;
    }

    switch (msgType) {
        case NONE_TYPE:
            textEdit->append(msg);
            break;
        case MSG_DEBUD_01:
            textEdit->append(msg);
            break;
        case MSG_SEND:
            textEdit->append(QString("T: %1").arg(msg));
            break;
        case MSG_SEND_ASYNC_1:
            textEdit->append(QString("T async 1: %1").arg(msg));
            break;
        case MSG_SEND_ASYNC_2:
            textEdit->append(QString("T async 2: %1").arg(msg));
            break;
        case MSG_READ: {
            QString msg2 = msg.trimmed();
            textEdit->append(QString("R: %1").arg(msg2));
        }
            break;
        case MSG_READ_BALANCE:
            textEdit->append(QString("R balance: %1").arg(msg));
            break;
        default:
            break;
    }
}

void CommuInfoDialog::sendCommand(const QString& cmd) {
    qDebug() << "T:" << cmd;
    m_tcpSocket->write(cmd.toStdString().c_str());

    if (!m_tcpSocket->waitForReadyRead()) {
        Q_ASSERT(false);
    }

    QByteArray data = m_tcpSocket->readAll();
    qDebug() << "R:" << data;
}

void CommuInfoDialog::onConnected() {
    tcpStatusLabel->setText("在线");
}

void CommuInfoDialog::onConnectionError() {

}

void CommuInfoDialog::onDisconnected() {
    tcpStatusLabel->setText("离线");
}


