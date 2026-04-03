#include "qsqldatabase.h"

#include <QDebug>
#include <QWidget>
#include <QSettings>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QtSql/QSqlRecord>
#include <QVariant>
#include <QMessageBox>
#include <QTimer>

const char* AppSqlDatabase::kConnName = "app_sqlite_conn";

AppSqlDatabase::AppSqlDatabase(QObject *parent)
    : QObject{parent}
{
    if (!openDatabase()) {
        const QString errorMsg = QStringLiteral("无法连接 MySQL (ODBC): 192.168.10.170:3306/PhenoLabHT");
        qWarning() << errorMsg;
        if (auto *parentWidget = qobject_cast<QWidget*>(parent)) {
            // 事件循环未启动时不能直接show QMessageBox，否则会阻塞
            // 改用 QTimer::singleShot 延迟到事件循环启动后再弹框
            QTimer::singleShot(0, parentWidget, [parentWidget, errorMsg]() {
                QMessageBox::warning(parentWidget, QStringLiteral("数据库连接失败"), errorMsg);
            });
        }
        return;
    }
    qDebug() << "MySQL 连接成功，目标库: PhenoLabHT";
}

AppSqlDatabase::~AppSqlDatabase()
{
    close();
}

void AppSqlDatabase::close()
{
    if (!QSqlDatabase::contains(kConnName)) {
        return;
    }

    {
        QSqlDatabase db = QSqlDatabase::database(kConnName);
        if (db.isOpen()) {
            qDebug() << "关闭数据库连接:" << db.databaseName();
            db.close();
        }
        // 作用域结束后，db 会被销毁，避免 removeDatabase 时仍有引用
    }

    QSqlDatabase::removeDatabase(kConnName);
    qDebug() << "数据库连接已移除";
}

QString AppSqlDatabase::normalizeTableName(const QString &name)
{
    QString n = name;
    return n.replace('-', '_');
}

bool AppSqlDatabase::openDatabase()
{
    // 从 INI 文件读取数据库配置
    QSettings settings("BoxData.ini", QSettings::IniFormat);
    settings.beginGroup("Database");

    QString host = settings.value("host", "192.168.10.170").toString();
    QString port = settings.value("port", "3306").toString();
    QString database = settings.value("database", "PhenoLabHT").toString();
    QString user = settings.value("user", "root").toString();
    QString password = settings.value("password", "Zq17122320_").toString();

    settings.endGroup();

    if (QSqlDatabase::contains(kConnName)) {
        QSqlDatabase existingDb = QSqlDatabase::database(kConnName);
        if (existingDb.isOpen()) {
            return true;
        }
        existingDb.close();
        QSqlDatabase::removeDatabase(kConnName);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QODBC", kConnName);
    db.setDatabaseName(
        QStringLiteral("DRIVER={MySQL ODBC 9.6 Unicode Driver};"
                       "SERVER=%1;PORT=%2;"
                       "DATABASE=%3;"
                       "USER=%4;PASSWORD=%5;"
                       "OPTION=3;")
        .arg(host).arg(port).arg(database).arg(user).arg(password)
    );

    if (!db.open()) {
        qWarning() << "QODBC 打开失败:" << db.lastError().text();
        return false;
    }

    qDebug() << "QODBC 已连接:" << host << ":" << port << "/" << database;
    return true;
}

bool AppSqlDatabase::isConnected() const
{
    if (!QSqlDatabase::contains(kConnName))
        return false;
    QSqlDatabase db = QSqlDatabase::database(kConnName, false);
    if (!db.isOpen())
        return false;
    // isOpen() 不检测网络层断连，用轻量探测确认连接真实可用
    QSqlQuery q(db);
    return q.exec("SELECT 1");
}

QSqlQuery AppSqlDatabase::query(const QString &sql)
{
    QSqlDatabase db = QSqlDatabase::database(kConnName);
    if (!db.isOpen()) {
        qWarning() << "数据库未打开，无法执行查询";
        return QSqlQuery(db);
    }
    QSqlQuery q(db);
    if (!q.exec(sql)) {
        qWarning() << "查询执行失败:" << q.lastError().text() << "SQL:" << sql;
    }
    return q;
}
