#include "qsqldatabase.h"

#include <QFile>
#include <QDebug>
#include <QWidget>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QtSql/QSqlRecord>
#include <QVariant>
#include <QMessageBox>

namespace {
// 使用一个连接名，避免重复 addDatabase 警告
const char* kConnName = "app_sqlite_conn";

constexpr auto kTableTransferLeft = "Box_Transfer_Area_Left";
constexpr auto kTableTransferRight = "Box_Transfer_Area_Right";
constexpr auto kTableOther = "other";
}

AppSqlDatabase::AppSqlDatabase(const QString &dbFilePath, QObject *parent)
    : QObject{parent}
{
    const bool databaseExists = QFile::exists(dbFilePath);

    if (!openDatabase(dbFilePath)) {
        QString errorMsg = QStringLiteral("无法打开数据库: %1").arg(dbFilePath);
        if (databaseExists) {
            errorMsg.append(QStringLiteral("\n数据库文件可能正被其他程序占用，请关闭相关程序后重试。"));
        }
        qWarning() << errorMsg;

        if (auto *parentWidget = qobject_cast<QWidget*>(parent)) {
            QMessageBox::warning(parentWidget, QStringLiteral("数据库初始化失败"), errorMsg);
        }
        return;
    }

    if (!databaseExists) {
        qDebug() << "首次创建数据库文件:" << dbFilePath;
    }

    ensureDefaultSchema();
}

AppSqlDatabase::~AppSqlDatabase()
{
    close();
}

void AppSqlDatabase::close()
{
    if (QSqlDatabase::contains(kConnName)) {
        QSqlDatabase db = QSqlDatabase::database(kConnName);
        if (db.isOpen()) {
            qDebug() << "关闭数据库连接:" << db.databaseName();
            db.close();
        }
        QSqlDatabase::removeDatabase(kConnName);
        qDebug() << "数据库连接已移除";
    }
}

QString AppSqlDatabase::normalizeTableName(const QString &name)
{
    QString n = name;
    return n.replace('-', '_');
}

bool AppSqlDatabase::openDatabase(const QString &dbFilePath)
{
    if (QSqlDatabase::contains(kConnName)) {
        QSqlDatabase existingDb = QSqlDatabase::database(kConnName);
        if (!existingDb.databaseName().isEmpty() && existingDb.databaseName() != dbFilePath) {
            existingDb.close();
            QSqlDatabase::removeDatabase(kConnName);
        } else {
            if (!existingDb.isOpen() && !existingDb.open()) {
                qWarning() << "SQLite 打开失败:" << existingDb.lastError().text();
                return false;
            }
            return true;
        }
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", kConnName);
    db.setDatabaseName(dbFilePath);
    if (!db.open()) {
        qWarning() << "SQLite 打开失败:" << db.lastError().text();
        return false;
    }
    qDebug() << "SQLite 已打开:" << dbFilePath;
    return true;
}

void AppSqlDatabase::ensureDefaultSchema()
{
    QSqlDatabase db = QSqlDatabase::database(kConnName);
    if (!db.isOpen()) {
        qWarning() << "数据库未打开，无法创建默认表结构";
        return;
    }

    createDefaultTables();
    seedDefaultData();
}

void AppSqlDatabase::createDefaultTables()
{
    QSqlDatabase db = QSqlDatabase::database(kConnName);
    if (!db.isOpen()) {
        qWarning() << "数据库未打开，无法创建默认表结构";
        return;
    }

    const QString createTransferLeft = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS %1 ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  liquidName TEXT NOT NULL,"
        "  originX INTEGER NOT NULL,"
        "  originY INTEGER NOT NULL,"
        "  pipetteZ INTEGER NOT NULL,"
        "  gripperZ INTEGER NOT NULL,"
        "  solidZ INTEGER NOT NULL,"
        "  neighborRightMm REAL NOT NULL,"
        "  neighborBottomMm REAL NOT NULL,"
        "  currentIndex INTEGER NOT NULL,"
        "  selfLocation INTEGER NOT NULL DEFAULT 0"
        ")"
    ).arg(QString::fromLatin1(kTableTransferLeft));

    const QString createTransferRight = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS %1 ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  currentIndex INTEGER NOT NULL,"
        "  originX INTEGER NOT NULL,"
        "  originY INTEGER NOT NULL,"
        "  gripperZ INTEGER NOT NULL"
        ")"
    ).arg(QString::fromLatin1(kTableTransferRight));

    const QString createOther = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS %1 ("
        "  currentIndex INTEGER NOT NULL,"
        "  name TEXT NOT NULL,"
        "  originX INTEGER NOT NULL,"
        "  originY INTEGER NOT NULL,"
        "  gripperZ INTEGER NOT NULL,"
        "  tipsZ INTEGER NOT NULL DEFAULT 0,"
        "  solidZ INTEGER NOT NULL DEFAULT 0"
        ")"
    ).arg(QString::fromLatin1(kTableOther));

    QSqlQuery query(db);
    const QList<QString> statements{createTransferLeft, createTransferRight, createOther};
    for (const QString &sql : statements) {
        if (!query.exec(sql)) {
            qWarning() << "创建默认表失败:" << query.lastError().text() << "SQL:" << sql;
        }
    }

}

void AppSqlDatabase::seedDefaultData()
{
    if (isTableEmpty(QString::fromLatin1(kTableTransferLeft))) {
        insertRow(QString::fromLatin1(kTableTransferLeft), {
            {"liquidName", "DMF"},
            {"originX", 15050},
            {"originY", 23094},
            {"pipetteZ", 0},
            {"gripperZ", 272187},
            {"solidZ", 0},
            {"neighborRightMm", 39.0},
            {"neighborBottomMm", 39.0},
            {"currentIndex", 1},
            {"selfLocation", 0}
        });
    }

    if (isTableEmpty(QString::fromLatin1(kTableTransferRight))) {
        insertRow(QString::fromLatin1(kTableTransferRight), {
            {"currentIndex", 0},
            {"originX", 5513},
            {"originY", 23458},
            {"gripperZ", 269232}
        });
    }

    ensureNamedRecord(QString::fromLatin1(kTableOther), QStringLiteral("name"), QStringLiteral("gripArea"), {
        {"currentIndex", 0},
        {"name", "gripArea"},
        {"originX", 20377},
        {"originY", 35493},
        {"gripperZ", 265192},
        {"tipsZ", 0},
        {"solidZ", 0}
    });

    ensureNamedRecord(QString::fromLatin1(kTableOther), QStringLiteral("name"), QStringLiteral("balanceArea"), {
        {"currentIndex", 0},
        {"name", "balanceArea"},
        {"originX", 21499},
        {"originY", 8439},
        {"gripperZ", 276000},
        {"tipsZ", 0}, // 为0  59970
        {"solidZ", 0}
    });

    ensureNamedRecord(QString::fromLatin1(kTableOther), QStringLiteral("name"), QStringLiteral("balanceAreaForSolid"), {
       {"currentIndex", 0},
       {"name", "balanceAreaForSolid"},
       {"originX", 10113},
       {"originY", 14108},
       {"gripperZ", 0},
       {"tipsZ", 0},
       {"solidZ", 28822}
   });


    // 插入第九条记录，balanceAreaForTipsArea
    ensureNamedRecord(QString::fromLatin1(kTableOther), QStringLiteral("name"), QStringLiteral("balanceAreaForTipsArea"), {
       {"currentIndex", 0},
       {"name", "balanceAreaForTipsArea"},
       {"originX", 21814},
       {"originY", 14685},
       {"gripperZ", 0},
       {"tipsZ", 60828},
       {"solidZ", 0}
    });

    // 插入第三条记录：tipsHeadArea
    ensureNamedRecord(QString::fromLatin1(kTableOther), QStringLiteral("name"), QStringLiteral("tipsHeadArea"), {
        {"currentIndex", 0},
        {"name", "tipsHeadArea"},
        {"originX", 1902},
        {"originY", 10110},
        {"gripperZ", 0},
        {"tipsZ", 0},
        {"solidZ", 0}
    });

    // 插入第四条记录：gripLiquidArea
    ensureNamedRecord(QString::fromLatin1(kTableOther), QStringLiteral("name"), QStringLiteral("gripLiquidArea"), {
        {"currentIndex", 0},
        {"name", "gripLiquidArea"},
        {"originX", 20712},
        {"originY", 41753},
        {"gripperZ", 40000},
        {"tipsZ", 0},
        {"solidZ", 0}
    });

    // 插入第五条记录：wasteArea
    ensureNamedRecord(QString::fromLatin1(kTableOther), QStringLiteral("name"), QStringLiteral("wasteArea"), {
        {"currentIndex", 0},
        {"name", "wasteArea"},
        {"originX", 23256},
        {"originY", 49976},
        {"gripperZ", 0},
        {"tipsZ", 126526},
        {"solidZ", 0}
    });

    // 插入第六条记录：solidArea
    ensureNamedRecord(QString::fromLatin1(kTableOther), QStringLiteral("name"), QStringLiteral("solidArea"), {
        {"currentIndex", 0},
        {"name", "solidArea"},
        {"originX", -1293},
        {"originY", 3472},
        {"gripperZ", 0},
        {"tipsZ", 0},
        {"solidZ", 241838}
    });

    // 插入第七条记录，Hat区域：帽子区域
    ensureNamedRecord(QString::fromLatin1(kTableOther), QStringLiteral("name"), QStringLiteral("hatArea"), {
        {"currentIndex", 0},
        {"name", "hatArea"},
        {"originX", 17464},
        {"originY", 35572},
        {"gripperZ", 257246},
        {"tipsZ", 0},
        {"solidZ", 0}
    });

    // 插入第八条记录，摇床区：shakeBedArea
    ensureNamedRecord(QString::fromLatin1(kTableOther), QStringLiteral("name"), QStringLiteral("shakeBedArea"), {
        {"currentIndex", 0},
        {"name", "shakeBedArea"},
        {"originX", 15088},
        {"originY", 4691},
        {"gripperZ", 276999},
        {"tipsZ", 0},
        {"solidZ", 0}
    });




}


bool AppSqlDatabase::isTableEmpty(const QString &tableName) const
{
    QSqlDatabase db = QSqlDatabase::database(kConnName);
    if (!db.isOpen()) {
        qWarning() << "数据库未打开，无法检查表记录数";
        return false;
    }

    QSqlQuery query(db);
    const QString sql = QStringLiteral("SELECT COUNT(1) FROM %1").arg(tableName);
    if (!query.exec(sql)) {
        qWarning() << "统计表记录失败:" << query.lastError().text() << "SQL:" << sql;
        return false;
    }
    if (query.next()) {
        return query.value(0).toInt() == 0;
    }
    return false;
}

bool AppSqlDatabase::ensureNamedRecord(const QString &tableName,
                                       const QString &nameField,
                                       const QVariant &nameValue,
                                       const QVariantMap &values) const
{
    QSqlDatabase db = QSqlDatabase::database(kConnName);
    if (!db.isOpen()) {
        qWarning() << "数据库未打开，无法插入默认记录";
        return false;
    }

    QSqlQuery checkQuery(db);
    const QString sql = QStringLiteral("SELECT COUNT(1) FROM %1 WHERE %2 = :name")
                            .arg(tableName, nameField);
    checkQuery.prepare(sql);
    checkQuery.bindValue(QStringLiteral(":name"), nameValue);
    if (!checkQuery.exec()) {
        qWarning() << "查询默认记录失败:" << checkQuery.lastError().text() << "SQL:" << sql;
        return false;
    }
    if (checkQuery.next() && checkQuery.value(0).toInt() > 0) {
        return true;
    }

    return insertRow(tableName, values);
}

bool AppSqlDatabase::insertRow(const QString &tableName, const QVariantMap &values) const
{
    if (values.isEmpty()) {
        return true;
    }

    QSqlDatabase db = QSqlDatabase::database(kConnName);
    if (!db.isOpen()) {
        qWarning() << "数据库未打开，无法插入记录";
        return false;
    }

    QStringList columns;
    QStringList placeholders;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        columns << it.key();
        placeholders << ":" + it.key();
    }

    QSqlQuery insertQuery(db);
    const QString sql = QStringLiteral("INSERT INTO %1 (%2) VALUES (%3)")
                            .arg(tableName, columns.join(", "), placeholders.join(", "));
    insertQuery.prepare(sql);

    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        insertQuery.bindValue(":" + it.key(), it.value());
    }

    if (!insertQuery.exec()) {
        qWarning() << "插入记录失败:" << insertQuery.lastError().text() << "SQL:" << sql;
        return false;
    }
    qDebug() << "已插入默认记录到表:" << tableName;
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
