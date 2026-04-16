#include "networksettingsdialog.h"
#include "qsqldatabase.h"
#include "../TcpClientCore/tcpclientcore.h"
#include "../mainwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QSettings>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>

static const char* kNetIniFile   = "BoxData.ini";
static const char* kDbGroup      = "Database";
static const char* kTcpGroup     = "TCP";

NetworkSettingsDialog::NetworkSettingsDialog(AppSqlDatabase *dbm,
                                             TcpClientCore  *tcpCore,
                                             TcpClientCore  *tcpBalanceCore,
                                             QWidget        *parent)
    : QDialog(parent), m_dbm(dbm), m_tcpCore(tcpCore), m_tcpBalanceCore(tcpBalanceCore)
{
    setWindowTitle("网络设置");
    setMinimumWidth(720);

    // ── 左侧：数据库 ──────────────────────────────────
    QGroupBox *dbGroup = new QGroupBox("数据库", this);
    QFormLayout *dbForm = new QFormLayout;

    m_dbHost     = new QLineEdit(this);
    m_dbPort     = new QLineEdit(this);
    m_dbDatabase = new QLineEdit(this);
    m_dbUser     = new QLineEdit(this);
    m_dbPassword = new QLineEdit(this);
    m_dbPassword->setEchoMode(QLineEdit::Password);

    dbForm->addRow("Host",     m_dbHost);
    dbForm->addRow("Port",     m_dbPort);
    dbForm->addRow("Database", m_dbDatabase);
    dbForm->addRow("User",     m_dbUser);
    dbForm->addRow("Password", m_dbPassword);

    QPushButton *btnDbConnect    = new QPushButton("连接", this);
    QPushButton *btnDbDisconnect = new QPushButton("断开", this);
    QHBoxLayout *dbBtnLayout = new QHBoxLayout;
    dbBtnLayout->addWidget(btnDbConnect);
    dbBtnLayout->addWidget(btnDbDisconnect);

    QVBoxLayout *dbLayout = new QVBoxLayout(dbGroup);
    dbLayout->addLayout(dbForm);
    dbLayout->addLayout(dbBtnLayout);

    // ── 右侧：TCP ─────────────────────────────────────
    QGroupBox *tcpGroup = new QGroupBox("TCP", this);
    QFormLayout *tcpForm = new QFormLayout;

    m_localIP = new QLineEdit(this);
    QPushButton *btnAutoIP = new QPushButton("自动查找有线 IP", this);
    QHBoxLayout *localIPLayout = new QHBoxLayout;
    localIPLayout->addWidget(m_localIP);
    localIPLayout->addWidget(btnAutoIP);

    m_tcpCoreRemoteIP      = new QLineEdit(this);
    m_tcpCoreRemotePort    = new QLineEdit(this);
    m_tcpBalanceRemoteIP   = new QLineEdit(this);
    m_tcpBalanceRemotePort = new QLineEdit(this);

    tcpForm->addRow("本机 IP",              localIPLayout);
    tcpForm->addRow("tcpCore 远端 IP",      m_tcpCoreRemoteIP);
    tcpForm->addRow("tcpCore 远端端口",     m_tcpCoreRemotePort);
    tcpForm->addRow("tcpBalanceCore 远端 IP",   m_tcpBalanceRemoteIP);
    tcpForm->addRow("tcpBalanceCore 远端端口",  m_tcpBalanceRemotePort);

    QPushButton *btnTcpCoreConnect       = new QPushButton("tcpCore 连接",       this);
    QPushButton *btnTcpCoreDisconnect    = new QPushButton("tcpCore 断开",       this);
    QPushButton *btnTcpBalanceConnect    = new QPushButton("tcpBalanceCore 连接", this);
    QPushButton *btnTcpBalanceDisconnect = new QPushButton("tcpBalanceCore 断开", this);

    QVBoxLayout *tcpCoreBtnLayout = new QVBoxLayout;
    tcpCoreBtnLayout->addWidget(btnTcpCoreConnect);
    tcpCoreBtnLayout->addWidget(btnTcpCoreDisconnect);

    QVBoxLayout *tcpBalanceBtnLayout = new QVBoxLayout;
    tcpBalanceBtnLayout->addWidget(btnTcpBalanceConnect);
    tcpBalanceBtnLayout->addWidget(btnTcpBalanceDisconnect);

    QHBoxLayout *tcpAllBtnLayout = new QHBoxLayout;
    tcpAllBtnLayout->addLayout(tcpCoreBtnLayout);
    tcpAllBtnLayout->addLayout(tcpBalanceBtnLayout);

    QVBoxLayout *tcpLayout = new QVBoxLayout(tcpGroup);
    tcpLayout->addLayout(tcpForm);
    tcpLayout->addLayout(tcpAllBtnLayout);

    // ── 主布局（左右两栏）────────────────────────────
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->addWidget(dbGroup);
    mainLayout->addWidget(tcpGroup);

    // ── 信号连接 ──────────────────────────────────────
    connect(btnDbConnect,          &QPushButton::clicked, this, &NetworkSettingsDialog::onDbConnect);
    connect(btnDbDisconnect,       &QPushButton::clicked, this, &NetworkSettingsDialog::onDbDisconnect);
    connect(btnTcpCoreConnect,     &QPushButton::clicked, this, &NetworkSettingsDialog::onTcpCoreConnect);
    connect(btnTcpCoreDisconnect,  &QPushButton::clicked, this, &NetworkSettingsDialog::onTcpCoreDisconnect);
    connect(btnTcpBalanceConnect,  &QPushButton::clicked, this, &NetworkSettingsDialog::onTcpBalanceConnect);
    connect(btnTcpBalanceDisconnect, &QPushButton::clicked, this, &NetworkSettingsDialog::onTcpBalanceDisconnect);
    connect(btnAutoIP,             &QPushButton::clicked, this, &NetworkSettingsDialog::onAutoFindLocalIP);

    loadFromIni();
}

void NetworkSettingsDialog::loadFromIni()
{
    QSettings ini(kNetIniFile, QSettings::IniFormat);

    ini.beginGroup(kDbGroup);
    m_dbHost->setText(    ini.value("host",     "192.168.10.170").toString());
    m_dbPort->setText(    ini.value("port",     "3306").toString());
    m_dbDatabase->setText(ini.value("database", "PhenoLabHT").toString());
    m_dbUser->setText(    ini.value("user",     "root").toString());
    m_dbPassword->setText(ini.value("password", "").toString());
    ini.endGroup();

    ini.beginGroup(kTcpGroup);
    m_localIP->setText(             ini.value("localIP",              "").toString());
    m_tcpCoreRemoteIP->setText(     ini.value("tcpCoreRemoteIP",      "192.168.5.201").toString());
    m_tcpCoreRemotePort->setText(   ini.value("tcpCoreRemotePort",    "4196").toString());
    m_tcpBalanceRemoteIP->setText(  ini.value("tcpBalanceRemoteIP",   "192.168.5.201").toString());
    m_tcpBalanceRemotePort->setText(ini.value("tcpBalanceRemotePort", "4197").toString());
    ini.endGroup();
}

void NetworkSettingsDialog::saveDbToIni()
{
    QSettings ini(kNetIniFile, QSettings::IniFormat);
    ini.beginGroup(kDbGroup);
    ini.setValue("host",     m_dbHost->text());
    ini.setValue("port",     m_dbPort->text());
    ini.setValue("database", m_dbDatabase->text());
    ini.setValue("user",     m_dbUser->text());
    ini.setValue("password", m_dbPassword->text());
    ini.endGroup();
}

void NetworkSettingsDialog::saveTcpToIni()
{
    QSettings ini(kNetIniFile, QSettings::IniFormat);
    ini.beginGroup(kTcpGroup);
    ini.setValue("localIP",              m_localIP->text());
    ini.setValue("tcpCoreRemoteIP",      m_tcpCoreRemoteIP->text());
    ini.setValue("tcpCoreRemotePort",    m_tcpCoreRemotePort->text());
    ini.setValue("tcpBalanceRemoteIP",   m_tcpBalanceRemoteIP->text());
    ini.setValue("tcpBalanceRemotePort", m_tcpBalanceRemotePort->text());
    ini.endGroup();
}

// ── 数据库操作 ────────────────────────────────────────

void NetworkSettingsDialog::onDbConnect()
{
    if (!m_dbm) return;
    m_dbm->close();

    const QString dsn = QString(
        "DRIVER={MySQL ODBC 9.6 Unicode Driver};"
        "SERVER=%1;PORT=%2;"
        "DATABASE=%3;"
        "USER=%4;PASSWORD=%5;"
        "OPTION=3;"
    ).arg(m_dbHost->text(), m_dbPort->text(), m_dbDatabase->text(),
          m_dbUser->text(), m_dbPassword->text());

    QSqlDatabase db = QSqlDatabase::addDatabase("QODBC", AppSqlDatabase::kConnName);
    db.setDatabaseName(dsn);

    if (!db.open()) {
        QMessageBox::critical(this, "连接失败", db.lastError().text());
        QSqlDatabase::removeDatabase(AppSqlDatabase::kConnName);
        return;
    }

    qDebug() << "QODBC 已连接:" << m_dbHost->text() << m_dbPort->text() << m_dbDatabase->text();
    saveDbToIni();
}

void NetworkSettingsDialog::onDbDisconnect()
{
    if (m_dbm) {
        m_dbm->close();
        qDebug() << "数据库已断开";
        QMessageBox::information(this, "已断开", "数据库连接已断开");
    }
}

// ── TCP 操作 ──────────────────────────────────────────

void NetworkSettingsDialog::onTcpCoreConnect()
{
    if (!m_tcpCore) return;
    bool ok = m_tcpCore->connectToTcp(
        m_localIP->text(),
        m_tcpCoreRemoteIP->text(),
        static_cast<quint16>(m_tcpCoreRemotePort->text().toUInt()),
        true);
    if (ok) {
        saveTcpToIni();
    } else {
        QMessageBox::critical(this, "tcpCore 连接失败",
            QString("无法连接到 %1:%2").arg(m_tcpCoreRemoteIP->text(), m_tcpCoreRemotePort->text()));
    }
}

void NetworkSettingsDialog::onTcpCoreDisconnect()
{
    if (m_tcpCore) {
        m_tcpCore->disconnectFromTcp();
        QMessageBox::information(this, "已断开", "tcpCore 已断开");
    }
}

void NetworkSettingsDialog::onTcpBalanceConnect()
{
    if (!m_tcpBalanceCore) return;
    bool ok = m_tcpBalanceCore->connectToTcp(
        m_localIP->text(),
        m_tcpBalanceRemoteIP->text(),
        static_cast<quint16>(m_tcpBalanceRemotePort->text().toUInt()),
        true);
    if (ok) {
        saveTcpToIni();
    } else {
        QMessageBox::critical(this, "tcpBalanceCore 连接失败",
            QString("无法连接到 %1:%2").arg(m_tcpBalanceRemoteIP->text(), m_tcpBalanceRemotePort->text()));
    }
}

void NetworkSettingsDialog::onTcpBalanceDisconnect()
{
    if (m_tcpBalanceCore) {
        m_tcpBalanceCore->disconnectFromTcp();
        QMessageBox::information(this, "已断开", "tcpBalanceCore 已断开");
    }
}

void NetworkSettingsDialog::onAutoFindLocalIP()
{
    // 通过父窗口（MainWindow）调用 getLocalWiredIP()
    MainWindow *mw = qobject_cast<MainWindow*>(parent());
    if (!mw) {
        // 尝试向上找 MainWindow
        QWidget *p = parentWidget();
        while (p) {
            mw = qobject_cast<MainWindow*>(p);
            if (mw) break;
            p = p->parentWidget();
        }
    }
    if (mw) {
        QString ip = mw->getLocalWiredIP();
        if (!ip.isEmpty()) {
            m_localIP->setText(ip);
        } else {
            QMessageBox::warning(this, "未找到", "未检测到有线网口 IP");
        }
    }
}
