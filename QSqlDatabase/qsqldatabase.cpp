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
}

AppSqlDatabase::AppSqlDatabase(QObject *parent)
    : QObject{parent}
{
    if (!openDatabase()) {
        const QString errorMsg = QStringLiteral("无法连接 MySQL (ODBC): 192.168.10.170:3306/PhenoLabHT");
        qWarning() << errorMsg;
        if (auto *parentWidget = qobject_cast<QWidget*>(parent)) {
            QMessageBox::warning(parentWidget, QStringLiteral("数据库连接失败"), errorMsg);
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
                       "SERVER=192.168.10.170;PORT=3306;"
                       "DATABASE=PhenoLabHT;"
                       "USER=root;PASSWORD=Zq17122320_;"
                       "OPTION=3;")
    );

    if (!db.open()) {
        qWarning() << "QODBC 打开失败:" << db.lastError().text();
        return false;
    }

    qDebug() << "QODBC 已连接: 192.168.10.170:3306/PhenoLabHT";
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
