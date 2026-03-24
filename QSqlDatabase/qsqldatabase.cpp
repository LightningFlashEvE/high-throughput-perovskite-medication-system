#include "qsqldatabase.h"

#include <QDebug>
#include <QWidget>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QtSql/QSqlRecord>
#include <QVariant>
#include <QMessageBox>

namespace {
const char* kConnName = "app_sqlite_conn";

// MySQL 连接参数
constexpr auto kHost     = "192.168.10.170";
constexpr int  kPort     = 3306;
constexpr auto kUser     = "root";
constexpr auto kPassword = "Zq17122320_";
constexpr auto kDatabase = "PhenoLabHT";
}

AppSqlDatabase::AppSqlDatabase(QObject *parent)
    : QObject{parent}
{
    if (!openDatabase()) {
        QString errorMsg = QStringLiteral("无法连接 MySQL: %1:%2/%3")
                               .arg(kHost).arg(kPort).arg(kDatabase);
        qWarning() << errorMsg;
        if (auto *parentWidget = qobject_cast<QWidget*>(parent)) {
            QMessageBox::warning(parentWidget, QStringLiteral("数据库连接失败"), errorMsg);
        }
        return;
    }
    qDebug() << "MySQL 连接成功，目标库:" << kDatabase;
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

    if (QSqlDatabase::contains(kConnName)) {
        QSqlDatabase existingDb = QSqlDatabase::database(kConnName);
        if (existingDb.isOpen()) {
            return true;
        }
        existingDb.close();
        QSqlDatabase::removeDatabase(kConnName);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL", kConnName);
    db.setHostName(kHost);
    db.setPort(kPort);
    db.setUserName(kUser);
    db.setPassword(kPassword);
    db.setDatabaseName(kDatabase);

    if (!db.open()) {
        const QSqlError err = db.lastError();
        qWarning() << "QMYSQL 打开失败";
        qWarning() << "  driverName:" << db.driverName();
        qWarning() << "  host/port/db:" << kHost << kPort << kDatabase;
        qWarning() << "  errorType:" << err.type();
        qWarning() << "  databaseText:" << err.databaseText();
        qWarning() << "  driverText:" << err.driverText();
        qWarning() << "  nativeErrorCode:" << err.nativeErrorCode();
        qWarning() << "  fullText:" << err.text();
        if (!QSqlDatabase::drivers().contains("QMYSQL")) {
            qWarning() << "  提示: 当前 Qt 运行时未发现 QMYSQL 驱动，请检查 sqldrivers/qsqlmysql.dll 是否已部署。";
        }
        return false;
    }

    qDebug() << "QMYSQL 已连接:" << kHost << kPort << kDatabase;
    return true;
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
