#include "motorcontrol.h"
#include "ui_motorcontrol.h"
#include "tcpclientcore.h"
#include "qsqldatabase.h"
#include <QLabel>
#include <QTimer>

MotorControl::MotorControl(TcpClientCore *tcpMain,
                           TcpClientCore *tcpBalance,
                           AppSqlDatabase *db,
                           QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::MotorControl)
    , m_tcpMain(tcpMain)
    , m_tcpBalance(tcpBalance)
    , m_db(db)
{
    ui->setupUi(this);
    setWindowTitle("电机控制");

    // 取出状态灯 label
    m_ledMain    = ui->led_main;
    m_ledBalance = ui->led_balance;
    m_ledDb      = ui->led_db;

    // 启动 1s 定时器
    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, &MotorControl::updateStatusLights);
    m_statusTimer->start(1000);

    // 立即刷新一次
    updateStatusLights();
}

MotorControl::~MotorControl()
{
    delete ui;
}

void MotorControl::setLed(QLabel *led, bool on)
{
    if (!led) return;
    if (on) {
        led->setStyleSheet(
            "QLabel {"
            "  background-color: #4caf50;"
            "  border-radius: 8px;"
            "  border: 2px solid #388e3c;"
            "}"
        );
        led->setToolTip("已连接");
    } else {
        led->setStyleSheet(
            "QLabel {"
            "  background-color: #f44336;"
            "  border-radius: 8px;"
            "  border: 2px solid #c62828;"
            "}"
        );
        led->setToolTip("未连接");
    }
}

void MotorControl::updateStatusLights()
{
    setLed(m_ledMain,    m_tcpMain    && m_tcpMain->isConnected());
    setLed(m_ledBalance, m_tcpBalance && m_tcpBalance->isConnected());
    setLed(m_ledDb,      m_db         && m_db->isConnected());
}
