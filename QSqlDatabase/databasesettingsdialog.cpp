#include "databasesettingsdialog.h"
#include "qsqldatabase.h"

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

static const char* kIniFile     = "BoxData.ini";
static const char* kIniGroup    = "Database";

DatabaseSettingsDialog::DatabaseSettingsDialog(AppSqlDatabase *dbm, QWidget *parent)
    : QDialog(parent), m_dbm(dbm)
{
    setWindowTitle("数据库设置");
    setMinimumWidth(360);

    // ── 上方分组：连接信息 ──────────────────────────
    QGroupBox *connGroup = new QGroupBox("数据库连接信息", this);
    QFormLayout *form = new QFormLayout(connGroup);

    m_host     = new QLineEdit(this);
    m_port     = new QLineEdit(this);
    m_database = new QLineEdit(this);
    m_user     = new QLineEdit(this);
    m_password = new QLineEdit(this);
    m_password->setEchoMode(QLineEdit::Password);

    form->addRow("Host",     m_host);
    form->addRow("Port",     m_port);
    form->addRow("Database", m_database);
    form->addRow("User",     m_user);
    form->addRow("Password", m_password);

    // ── 下方分组：操作按钮 ──────────────────────────
    QGroupBox *btnGroup = new QGroupBox("操作", this);
    QHBoxLayout *btnLayout = new QHBoxLayout(btnGroup);

    m_btnConnect    = new QPushButton("连接数据库", this);
    m_btnDisconnect = new QPushButton("断开数据库", this);
    btnLayout->addWidget(m_btnConnect);
    btnLayout->addWidget(m_btnDisconnect);

    // ── 主布局 ──────────────────────────────────────
    QVBoxLayout *main = new QVBoxLayout(this);
    main->addWidget(connGroup);
    main->addWidget(btnGroup);

    connect(m_btnConnect,    &QPushButton::clicked, this, &DatabaseSettingsDialog::onConnect);
    connect(m_btnDisconnect, &QPushButton::clicked, this, &DatabaseSettingsDialog::onDisconnect);

    loadFromIni();
}

void DatabaseSettingsDialog::loadFromIni()
{
    QSettings ini(kIniFile, QSettings::IniFormat);
    ini.beginGroup(kIniGroup);
    m_host->setText(    ini.value("host",     "192.168.10.170").toString());
    m_port->setText(    ini.value("port",     "3306").toString());
    m_database->setText(ini.value("database", "PhenoLabHT").toString());
    m_user->setText(    ini.value("user",     "root").toString());
    m_password->setText(ini.value("password", "").toString());
    ini.endGroup();
}

void DatabaseSettingsDialog::saveToIni()
{
    QSettings ini(kIniFile, QSettings::IniFormat);
    ini.beginGroup(kIniGroup);
    ini.setValue("host",     m_host->text());
    ini.setValue("port",     m_port->text());
    ini.setValue("database", m_database->text());
    ini.setValue("user",     m_user->text());
    ini.setValue("password", m_password->text());
    ini.endGroup();
}

void DatabaseSettingsDialog::onConnect()
{
    if (!m_dbm) return;

    // 先断开旧连接
    m_dbm->close();

    // 用界面上的参数重新连接
    const QString dsn = QString(
        "DRIVER={MySQL ODBC 9.6 Unicode Driver};"
        "SERVER=%1;PORT=%2;"
        "DATABASE=%3;"
        "USER=%4;PASSWORD=%5;"
        "OPTION=3;"
    ).arg(m_host->text(), m_port->text(), m_database->text(),
          m_user->text(), m_password->text());

    QSqlDatabase db = QSqlDatabase::addDatabase("QODBC", AppSqlDatabase::kConnName);
    db.setDatabaseName(dsn);

    if (!db.open()) {
        QMessageBox::critical(this, "连接失败", db.lastError().text());
        QSqlDatabase::removeDatabase(AppSqlDatabase::kConnName);
        return;
    }

    qDebug() << "QODBC 已连接:" << m_host->text() << m_port->text() << m_database->text();
    saveToIni();
    QMessageBox::information(this, "连接成功",
        QString("已连接到 %1:%2/%3").arg(m_host->text(), m_port->text(), m_database->text()));
}

void DatabaseSettingsDialog::onDisconnect()
{
    if (m_dbm) {
        m_dbm->close();
        qDebug() << "数据库已断开";
        QMessageBox::information(this, "已断开", "数据库连接已断开");
    }
}
