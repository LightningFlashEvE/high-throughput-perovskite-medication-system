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

constexpr auto kTableTransferLeft = "LiquidMaterialArea";
constexpr auto kTableSolidMaterialArea = "SolidMaterialArea";
constexpr auto kTableOther = "other";
constexpr auto kTableShakeBedArea = "shakeBedArea";
constexpr auto kTableTipsHeadUsage = "tipsHeadUsage";
constexpr auto kTableRecipeQueue = "recipeQueue";
constexpr auto kTableRecipeMessageQueue = "recipeMessageQueue";
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
            // 确保外键约束已启用
            QSqlQuery query(existingDb);
            query.exec("PRAGMA foreign_keys = ON");
            return true;
        }
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", kConnName);
    db.setDatabaseName(dbFilePath);
    if (!db.open()) {
        qWarning() << "SQLite 打开失败:" << db.lastError().text();
        return false;
    }
    
    // 启用外键约束（SQLite 默认关闭）
    QSqlQuery query(db);
    if (!query.exec("PRAGMA foreign_keys = ON")) {
        qWarning() << "启用外键约束失败:" << query.lastError().text();
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
        "  selfLocation INTEGER PRIMARY KEY,"
        "  liquidName TEXT NOT NULL,"
        "  originX INTEGER NOT NULL,"
        "  originY INTEGER NOT NULL,"
        "  pipetteZ INTEGER NOT NULL,"
        "  gripperZ INTEGER NOT NULL,"
        "  solidZ INTEGER NOT NULL,"
        "  rightSpacing REAL NOT NULL DEFAULT 0,"
        "  bottomSpacing REAL NOT NULL DEFAULT 0,"
        "  cols INTEGER NOT NULL DEFAULT 0,"
        "  rows INTEGER NOT NULL DEFAULT 0,"
        "  currentIndex INTEGER NOT NULL"
        ")"
    ).arg(QString::fromLatin1(kTableTransferLeft));

    const QString createSolidMaterialArea = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS %1 ("
        "  selfLocation INTEGER PRIMARY KEY,"
        "  solidName TEXT NOT NULL,"
        "  originX INTEGER NOT NULL DEFAULT -1256,"
        "  originY INTEGER NOT NULL DEFAULT 3407,"
        "  solidZ INTEGER NOT NULL DEFAULT 239050,"
        "  rightSpacing REAL NOT NULL DEFAULT 2507.5,"
        "  bottomSpacing REAL NOT NULL DEFAULT 5637.25,"
        "  cols INTEGER NOT NULL DEFAULT 3,"
        "  rows INTEGER NOT NULL DEFAULT 5,"
        "  currentIndex INTEGER NOT NULL DEFAULT 0"
        ")"
    ).arg(QString::fromLatin1(kTableSolidMaterialArea));

    const QString createOther = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS %1 ("
        "  currentIndex INTEGER NOT NULL,"
        "  name TEXT NOT NULL,"
        "  originX INTEGER NOT NULL,"
        "  originY INTEGER NOT NULL,"
        "  gripperZ INTEGER NOT NULL,"
        "  tipsZ INTEGER NOT NULL DEFAULT 0,"
        "  solidZ INTEGER NOT NULL DEFAULT 0,"
        "  rightSpacing REAL NOT NULL DEFAULT 0,"
        "  bottomSpacing REAL NOT NULL DEFAULT 0,"
        "  cols INTEGER NOT NULL DEFAULT 0,"
        "  rows INTEGER NOT NULL DEFAULT 0"
        ")"
    ).arg(QString::fromLatin1(kTableOther));

    const QString createShakeBedArea = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS %1 ("
        "  selfLocation INTEGER PRIMARY KEY,"
        "  isEmpty INTEGER NOT NULL DEFAULT 0,"
        "  startTime TEXT NOT NULL DEFAULT '',"
        "  endTime TEXT NOT NULL DEFAULT ''"
        ")"
    ).arg(QString::fromLatin1(kTableShakeBedArea));

    const QString createTipsHeadUsage = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS %1 ("
        "  selfLocation INTEGER PRIMARY KEY,"
        "  status INTEGER NOT NULL DEFAULT 1"
        ")"
    ).arg(QString::fromLatin1(kTableTipsHeadUsage));

    const QString createRecipeQueue = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS %1 ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  recipeName TEXT NOT NULL,"
        "  createTime TEXT NOT NULL,"
        "  processState INTEGER NOT NULL DEFAULT 0,"
        "  executionOrder INTEGER NOT NULL DEFAULT 0"
        ")"
    ).arg(QString::fromLatin1(kTableRecipeQueue));

    const QString createRecipeMessageQueue = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS %1 ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  recipeId INTEGER NOT NULL,"
        "  messageOrder INTEGER NOT NULL,"
        "  content BLOB NOT NULL,"
        "  asciiOrHex INTEGER NOT NULL DEFAULT 1,"
        "  shouldWaitForResponse INTEGER NOT NULL DEFAULT 0,"
        "  expectedSignature TEXT NOT NULL DEFAULT '',"
        "  FOREIGN KEY (recipeId) REFERENCES %2(id) ON DELETE CASCADE"
        ")"
    ).arg(QString::fromLatin1(kTableRecipeMessageQueue), QString::fromLatin1(kTableRecipeQueue));

    QSqlQuery query(db);
    const QList<QString> statements{createTransferLeft, createSolidMaterialArea, createOther, createShakeBedArea, createTipsHeadUsage, createRecipeQueue, createRecipeMessageQueue};
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
            {"selfLocation", 0},
            {"liquidName", "DMF"},
            {"originX", 15050},
            {"originY", 23094},
            {"pipetteZ", 0},
            {"gripperZ", 272187},
            {"solidZ", 0},
            {"rightSpacing", 1732.75},
            {"bottomSpacing", 4355.0},
            {"cols", 5},
            {"rows", 3},
            {"currentIndex", 0}
        });
        insertRow(QString::fromLatin1(kTableTransferLeft), {
            {"selfLocation", 1},
            {"liquidName", "GBL"},
            {"originX", 15050},
            {"originY", 23094},
            {"pipetteZ", 0},
            {"gripperZ", 272187},
            {"solidZ", 0},
            {"rightSpacing", 1732.75},
            {"bottomSpacing", 4355.0},
            {"cols", 5},
            {"rows", 3},
            {"currentIndex", 0}
        });
        insertRow(QString::fromLatin1(kTableTransferLeft), {
            {"selfLocation", 2},
            {"liquidName", "NMP"},
            {"originX", 15050},
            {"originY", 23094},
            {"pipetteZ", 0},
            {"gripperZ", 272187},
            {"solidZ", 0},
            {"rightSpacing", 1732.75},
            {"bottomSpacing", 4355.0},
            {"cols", 5},
            {"rows", 3},
            {"currentIndex", 0}
        });

    }

    // 固体药品表初始数据
    if (isTableEmpty(QString::fromLatin1(kTableSolidMaterialArea))) {
        insertRow(QString::fromLatin1(kTableSolidMaterialArea), {
            {"selfLocation", 0},
            {"solidName", "FAI"},
            {"originX", -1256},
            {"originY", 3407},
            {"solidZ", 239050},
            {"rightSpacing", 2507.5},
            {"bottomSpacing", 5637.25},
            {"cols", 3},
            {"rows", 5},
            {"currentIndex", 0}
        });
        insertRow(QString::fromLatin1(kTableSolidMaterialArea), {
            {"selfLocation", 1},
            {"solidName", "CsI"},
            {"originX", -1256},
            {"originY", 3407},
            {"solidZ", 239050},
            {"rightSpacing", 2507.5},
            {"bottomSpacing", 5637.25},
            {"cols", 3},
            {"rows", 5},
            {"currentIndex", 1}
        });
        insertRow(QString::fromLatin1(kTableSolidMaterialArea), {
            {"selfLocation", 2},
            {"solidName", "PbI2"},
            {"originX", -1256},
            {"originY", 3407},
            {"solidZ", 239050},
            {"rightSpacing", 2507.5},
            {"bottomSpacing", 5637.25},
            {"cols", 3},
            {"rows", 5},
            {"currentIndex", 2}
        });
    }


    ensureNamedRecord(QString::fromLatin1(kTableOther), QStringLiteral("name"), QStringLiteral("emptyBottleArea"), {
        {"currentIndex", 0},
        {"name", "emptyBottleArea"},
        {"originX", 5478},
        {"originY", 23420},
        {"gripperZ", 270781}, // old:269232 new:274377
        {"tipsZ", 0},
        {"solidZ", 0},
        {"rightSpacing", 1732.75},
        {"bottomSpacing", 4355.0},
        {"cols", 5},
        {"rows", 3}
    });

    ensureNamedRecord(QString::fromLatin1(kTableOther), QStringLiteral("name"), QStringLiteral("gripArea"), {
        {"currentIndex", 0},
        {"name", "gripArea"},
        {"originX", 20383},
        {"originY", 35699},
        {"gripperZ", 265192},
        {"tipsZ", 60828},
        {"solidZ", 0},
        {"rightSpacing", 0},
        {"bottomSpacing", 0},
        {"cols", 0},
        {"rows", 0}
    });

    ensureNamedRecord(QString::fromLatin1(kTableOther), QStringLiteral("name"), QStringLiteral("balanceArea"), {
        {"currentIndex", 0},
        {"name", "balanceArea"},
        {"originX", 21525},
        {"originY", 7333},
        {"gripperZ", 260583},
        {"tipsZ", 0}, // 为0  59970
        {"solidZ", 0},
        {"rightSpacing", 0},
        {"bottomSpacing", 0},
        {"cols", 0},
        {"rows", 0}
    });

    ensureNamedRecord(QString::fromLatin1(kTableOther), QStringLiteral("name"), QStringLiteral("balanceAreaForSolid"), {
       {"currentIndex", 0},
       {"name", "balanceAreaForSolid"},
       {"originX", 10115},
       {"originY", 13290},
       {"gripperZ", 0},
       {"tipsZ", 0},
       {"solidZ", 13124}, // 28822
       {"rightSpacing", 0},
       {"bottomSpacing", 0},
       {"cols", 0},
       {"rows", 0}
   });

    // 插入第三条记录：tipsHeadArea
    ensureNamedRecord(QString::fromLatin1(kTableOther), QStringLiteral("name"), QStringLiteral("tipsHeadArea"), {
        {"currentIndex", 0},
        {"name", "tipsHeadArea"},
        {"originX", 1902},
        {"originY", 10110},
        {"gripperZ", 0},
        {"tipsZ", 0},
        {"solidZ", 0},
        {"rightSpacing", 412.86},
        {"bottomSpacing", 1135.55},
        {"cols", 8},
        {"rows", 12}
    });

    // 插入第四条记录：gripLiquidArea
    ensureNamedRecord(QString::fromLatin1(kTableOther), QStringLiteral("name"), QStringLiteral("gripLiquidArea"), {
        {"currentIndex", 0},
        {"name", "gripLiquidArea"},
        {"originX", 20712},
        {"originY", 41753},
        {"gripperZ", 40000},
        {"tipsZ", 0},
        {"solidZ", 0},
        {"rightSpacing", 0},
        {"bottomSpacing", 0},
        {"cols", 0},
        {"rows", 0}
    });

    // 插入第五条记录：wasteArea
    ensureNamedRecord(QString::fromLatin1(kTableOther), QStringLiteral("name"), QStringLiteral("wasteArea"), {
        {"currentIndex", 0},
        {"name", "wasteArea"},
        {"originX", 23256},
        {"originY", 49976},
        {"gripperZ", 0},
        {"tipsZ", 126526},
        {"solidZ", 0},
        {"rightSpacing", 0},
        {"bottomSpacing", 0},
        {"cols", 0},
        {"rows", 0}
    });

    // 插入第六条记录：solidArea
    ensureNamedRecord(QString::fromLatin1(kTableOther), QStringLiteral("name"), QStringLiteral("solidArea"), {
        {"currentIndex", 0},
        {"name", "solidArea"},
        {"originX", -1293},
        {"originY", 3472},
        {"gripperZ", 0},
        {"tipsZ", 0},
        {"solidZ", 238646},   // 235455  241838
        {"rightSpacing", 0},
        {"bottomSpacing", 0},
        {"cols", 0},
        {"rows", 0}
    });

    // 插入第七条记录，Hat区域：帽子区域
    ensureNamedRecord(QString::fromLatin1(kTableOther), QStringLiteral("name"), QStringLiteral("hatArea"), {
        {"currentIndex", 0},
        {"name", "hatArea"},
        {"originX", 17464},
        {"originY", 35572},
        {"gripperZ", 257246},
        {"tipsZ", 0},
        {"solidZ", 0},
        {"rightSpacing", 0},
        {"bottomSpacing", 0},
        {"cols", 0},
        {"rows", 0}
    });

    // 插入第八条记录，摇床区：shakeBedArea
    ensureNamedRecord(QString::fromLatin1(kTableOther), QStringLiteral("name"), QStringLiteral("shakeBedArea"), {
        {"currentIndex", 0},
        {"name", "shakeBedArea"},
        {"originX", 14893},
        {"originY", 4430},
        {"gripperZ", 276999},
        {"tipsZ", 0},
        {"solidZ", 0},
        {"rightSpacing", 1732.75},
        {"bottomSpacing", 4355.0},
        {"cols", 5},
        {"rows", 3}
    });

    // 插入第十条记录
    // 初始化 shakeBedArea 表，创建15条默认记录（selfLocation从0到14）
    if (isTableEmpty(QString::fromLatin1(kTableShakeBedArea))) {
        for (int i = 0; i < 15; ++i) {
            insertRow(QString::fromLatin1(kTableShakeBedArea), {
                {"selfLocation", i},
                {"isEmpty", 1},
                {"startTime", ""},
                {"endTime", ""}
            });
        }
    }

    // 初始化 tipsHeadUsage 表，创建96条默认记录（selfLocation从0到95，status全为1）
    if (isTableEmpty(QString::fromLatin1(kTableTipsHeadUsage))) {
        for (int i = 0; i < 96; ++i) {
            insertRow(QString::fromLatin1(kTableTipsHeadUsage), {
                {"selfLocation", i},
                {"status", 1}
            });
        }
    }

    ensureNamedRecord(QString::fromLatin1(kTableOther), QStringLiteral("name"), QStringLiteral("transferRightArea"), {
        {"currentIndex", 0},
        {"name", "transferRightArea"},
        {"originX", 5434},
        {"originY", 42388},
        {"gripperZ", 266175},
        {"tipsZ", 0},
        {"solidZ", 0},
        {"rightSpacing", 1732.75},
        {"bottomSpacing", 4355.0},
        {"cols", 5},
        {"rows", 3}
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
