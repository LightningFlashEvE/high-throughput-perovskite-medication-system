#include "CommuInfoDialog.h"
#include <QLayout>
#include <QLabel>
#include <QTcpSocket>
#include <QNetworkProxy>

CommuInfoDialog* CommuInfoDialog::getInstance() {
    static CommuInfoDialog instance;
    return &instance;
}

CommuInfoDialog::CommuInfoDialog(QWidget* parent) :
    QDialog(parent),
    tcpSocket(new QTcpSocket(this))
{
    setWindowTitle("通信信息");
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
    connect(testBtn, &QPushButton::clicked, this, &CommuInfoDialog::clickTestBtn);
    connect(testBtn2, &QPushButton::clicked, this, &CommuInfoDialog::clickBtn_ResetPos);
    connect(testBtn3, &QPushButton::clicked, this, &CommuInfoDialog::clickTestBtn3);

    connect(testBtn_Y_Rel_P, &QPushButton::clicked, this, &CommuInfoDialog::clickBtn_Y_Rel_P);
    connect(testBtn_Y_Rel_N, &QPushButton::clicked, this, &CommuInfoDialog::clickBtn_Y_Rel_N);
}

CommuInfoDialog::~CommuInfoDialog() {
    tcpSocket->disconnectFromHost();
}

void CommuInfoDialog::clickClearMsgBtn() {
    textEdit->clear();
}

void CommuInfoDialog::clickTagBtn() {
    textEdit->append("-------------------------------------------------"
                     "-------------------------------------------------");
}

void CommuInfoDialog::clickTestBtn() {
    tcpSocket->setProxy(QNetworkProxy::NoProxy);
    tcpSocket->connectToHost("192.168.5.201", 4196);
    printMsg("等待连接...");

    // 等待连接建立（最多3秒）
    if (tcpSocket->waitForConnected(3000)) {
        printMsg("TCP连接成功");
        tcpStatusLabel->setText("在线");
    } else {
        printMsg("TCP连接失败" + tcpSocket->errorString());
    }
}

void CommuInfoDialog::clickBtn_ResetPos() {
    QString cmd = ">09GA15F";
    tcpSocket->write(cmd.toStdString().c_str());
    printMsg(cmd, MSG_SEND);

    QByteArray data = tcpSocket->readAll();
    printMsg(data, MSG_READ);
}

void CommuInfoDialog::clickBtn_Y_Rel_P() {
    QString cmd = ">09h0000100013883509";
    tcpSocket->write(cmd.toStdString().c_str());
    printMsg(cmd, MSG_SEND);

    QByteArray data = tcpSocket->readAll();
    printMsg(data, MSG_READ);
}

void CommuInfoDialog::clickBtn_Y_Rel_N() {
    QString cmd = ">09hFFFFF00013889F1E";
    tcpSocket->write(cmd.toStdString().c_str());
    printMsg(cmd, MSG_SEND);

    QByteArray data = tcpSocket->readAll();
    printMsg(data, MSG_READ);
}

void CommuInfoDialog::clickTestBtn3() {
    tcpSocket->disconnectFromHost();
    tcpStatusLabel->setText("离线");
    printMsg("断开连接...");
}

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
