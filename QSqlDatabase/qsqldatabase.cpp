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
    QMutexLocker locker(&m_dbMutex);  // 自动加锁，函数退出时自动解锁
    
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

/**
 * @brief 执行参数化查询（防止SQL注入攻击）
 * @details 使用预处理语句和参数绑定的方式执行查询，比字符串拼接更安全。
 *          该方法会自动加锁保证线程安全，多个界面可以安全地并发调用。
 * 
 * @param sql SQL查询语句，使用 '?' 作为占位符
 *            示例："SELECT * FROM users WHERE name = ? AND age > ?"
 * @param bindValues 要绑定的参数值列表（QVariantList），按照 '?' 出现的顺序依次对应
 *                   示例：{QVariant("张三"), QVariant(18)}
 * 
 * @return QSqlQuery 查询结果对象
 *         - 如果查询成功，调用 next() 可以遍历结果
 *         - 如果查询失败，可通过 lastError() 获取错误信息
 * 
 * @note 线程安全：该方法内部使用 QMutexLocker 自动加锁和解锁
 * @note 防SQL注入：参数通过绑定方式传递，不会被解释为SQL代码
 * 
 * @warning 参数数量必须与SQL中的 '?' 数量一致，否则会执行失败
 * 
 * 使用示例1：查询单个条件
 * @code
 * QString sql = "SELECT originX, originY FROM LiquidMaterialArea WHERE liquidName = ?";
 * QSqlQuery query = dbm->preparedQuery(sql, {liquidName});
 * if (query.next()) {
 *     int x = query.value("originX").toInt();
 *     int y = query.value("originY").toInt();
 * }
 * @endcode
 * 
 * 使用示例2：查询多个条件
 * @code
 * QString sql = "SELECT * FROM users WHERE name = ? AND age > ? AND city = ?";
 * QSqlQuery query = dbm->preparedQuery(sql, {userName, minAge, cityName});
 * while (query.next()) {
 *     // 处理每一行结果
 * }
 * @endcode
 * 
 * 使用示例3：防SQL注入测试
 * @code
 * // 即使 userName 包含恶意代码，也会被当作普通字符串处理
 * QString userName = "admin' OR '1'='1";  // 恶意输入
 * QString sql = "SELECT * FROM users WHERE name = ?";
 * QSqlQuery query = dbm->preparedQuery(sql, {userName});
 * // 只会查找名为 "admin' OR '1'='1" 的用户（如果存在），而不是所有用户
 * 
 * @endcode
 */
QSqlQuery AppSqlDatabase::preparedQuery(const QString &sql, const QVariantList &bindValues)
{
    // 1. 自动加锁，保证线程安全（函数退出时自动解锁）
    QMutexLocker locker(&m_dbMutex);
    
    // 2. 获取数据库连接
    QSqlDatabase db = QSqlDatabase::database(kConnName);
    if (!db.isOpen()) {
        qWarning() << "数据库未打开，无法执行查询";
        return QSqlQuery(db);  // 返回无效的查询对象
    }
    
    // 3. 创建查询对象
    QSqlQuery q(db);
    
    // 4. 预处理 SQL 语句（编译SQL，准备参数占位符）
    if (!q.prepare(sql)) {
        qWarning() << "SQL 预处理失败:" << q.lastError().text() << "SQL:" << sql;
        return q;  // 返回预处理失败的查询对象
    }
    
    // 5. 绑定参数（按顺序将实际值绑定到 '?' 占位符）
    for (const QVariant &value : bindValues) {
        q.addBindValue(value);  // QVariant 会自动处理类型转换和SQL转义
    }
    
    // 6. 执行查询
    if (!q.exec()) {
        qWarning() << "参数化查询执行失败:" << q.lastError().text() 
                   << "SQL:" << sql << "参数:" << bindValues;
    }
    
    // 7. 返回查询结果（调用者可通过 next() 遍历结果）
    return q;
}

bool AppSqlDatabase::executeUpdate(const QString &sql)
{
    QMutexLocker locker(&m_dbMutex);
    
    QSqlDatabase db = QSqlDatabase::database(kConnName);
    if (!db.isOpen()) {
        qWarning() << "数据库未打开，无法执行更新";
        return false;
    }
    
    // 使用事务保证原子性
    if (!db.transaction()) {
        qWarning() << "开始事务失败";
        return false;
    }
    
    QSqlQuery q(db);
    if (!q.exec(sql)) {
        qWarning() << "更新执行失败:" << q.lastError().text() << "SQL:" << sql;
        db.rollback();
        return false;
    }
    
    if (!db.commit()) {
        qWarning() << "提交事务失败";
        db.rollback();
        return false;
    }
    
    qDebug() << "更新执行成功:" << sql;
    return true;
}

/**
 * @brief 执行参数化更新操作（INSERT/UPDATE/DELETE，防止SQL注入）
 * @details 使用预处理语句和参数绑定的方式执行更新，比字符串拼接更安全。
 *          该方法会自动开启事务并在成功后提交，失败时自动回滚，保证操作的原子性。
 *          同时自动加锁保证线程安全，多个界面可以安全地并发调用。
 * 
 * @param sql SQL更新语句（INSERT/UPDATE/DELETE），使用 '?' 作为占位符
 *            INSERT示例："INSERT INTO users (name, age) VALUES (?, ?)"
 *            UPDATE示例："UPDATE users SET age = ? WHERE name = ?"
 *            DELETE示例："DELETE FROM users WHERE id = ?"
 * @param bindValues 要绑定的参数值列表（QVariantList），按照 '?' 出现的顺序依次对应
 *                   示例：{QVariant("张三"), QVariant(18)}
 * 
 * @return bool 操作是否成功
 *         - true: 更新成功并已提交到数据库
 *         - false: 更新失败，已自动回滚，数据库状态未改变
 * 
 * @note 线程安全：该方法内部使用 QMutexLocker 自动加锁和解锁
 * @note 防SQL注入：参数通过绑定方式传递，不会被解释为SQL代码
 * @note 自动事务：自动开启事务，成功则提交，失败则回滚
 * 
 * @warning 参数数量必须与SQL中的 '?' 数量一致，否则会执行失败
 * @warning 执行失败时会自动回滚，数据库状态不会被修改
 * 
 * 使用示例1：插入数据
 * @code
 * QString sql = "INSERT INTO LiquidMaterialArea (liquidName, originX, originY) VALUES (?, ?, ?)";
 * bool success = dbm->preparedUpdate(sql, {"DMF", 100, 200});
 * if (success) {
 *     qDebug() << "插入成功";
 * } else {
 *     qWarning() << "插入失败";
 * }
 * @endcode
 * 
 * 使用示例2：更新数据
 * @code
 * QString sql = "UPDATE LiquidMaterialArea SET originX = ?, originY = ? WHERE liquidName = ?";
 * bool success = dbm->preparedUpdate(sql, {newX, newY, liquidName});
 * if (!success) {
 *     qWarning() << "更新失败，数据未改变";
 * }
 * @endcode
 * 
 * 使用示例3：删除数据
 * @code
 * QString sql = "DELETE FROM LiquidMaterialArea WHERE liquidName = ?";
 * bool success = dbm->preparedUpdate(sql, {liquidName});
 * // 即使 liquidName 包含特殊字符如 "test' OR '1'='1"，也会被安全处理
 * @endcode
 * 
 * 使用示例4：错误处理
 * @code
 * QString sql = "UPDATE users SET age = ? WHERE name = ?";
 * if (!dbm->preparedUpdate(sql, {age, name})) {
 *     // 更新失败，数据库已自动回滚到更新前的状态
 *     QMessageBox::warning(this, "错误", "更新失败，请检查数据是否正确");
 *     return;
 * }
 * // 更新成功，可以继续后续操作
 * @endcode
 */
bool AppSqlDatabase::preparedUpdate(const QString &sql, const QVariantList &bindValues)
{
    // 1. 自动加锁，保证线程安全（函数退出时自动解锁）
    QMutexLocker locker(&m_dbMutex);
    
    // 2. 获取数据库连接
    QSqlDatabase db = QSqlDatabase::database(kConnName);
    if (!db.isOpen()) {
        qWarning() << "数据库未打开，无法执行更新";
        return false;
    }
    
    // 3. 开始事务（保证操作的原子性：要么全部成功，要么全部失败）
    if (!db.transaction()) {
        qWarning() << "开始事务失败";
        return false;
    }
    
    // 4. 创建查询对象
    QSqlQuery q(db);
    
    // 5. 预处理 SQL 语句（编译SQL，准备参数占位符）
    if (!q.prepare(sql)) {
        qWarning() << "SQL 预处理失败:" << q.lastError().text() << "SQL:" << sql;
        db.rollback();  // 预处理失败，回滚事务
        return false;
    }
    
    // 6. 绑定参数（按顺序将实际值绑定到 '?' 占位符）
    for (const QVariant &value : bindValues) {
        q.addBindValue(value);  // QVariant 会自动处理类型转换和SQL转义
    }
    
    // 7. 执行更新操作
    if (!q.exec()) {
        qWarning() << "参数化更新执行失败:" << q.lastError().text() 
                   << "SQL:" << sql << "参数:" << bindValues;
        db.rollback();  // 执行失败，回滚事务
        return false;
    }
    
    // 8. 提交事务（将更改永久保存到数据库）
    if (!db.commit()) {
        qWarning() << "提交事务失败";
        db.rollback();  // 提交失败，回滚事务
        return false;
    }
    
    // 9. 操作成功
    qDebug() << "参数化更新执行成功:" << sql << "参数:" << bindValues;
    return true;
}

/**
 * @brief 手动开始一个数据库事务
 * @details 事务可以将多个数据库操作组合成一个原子操作单元，
 *          保证这些操作要么全部成功（commit），要么全部失败（rollback）。
 *          开始事务后，所有的数据库修改都不会立即生效，直到调用 commit() 提交。
 * 
 * @return bool 是否成功开始事务
 *         - true: 事务开始成功，可以执行后续的数据库操作
 *         - false: 事务开始失败（通常是数据库未打开）
 * 
 * @note 线程安全：该方法内部使用 QMutexLocker 自动加锁
 * @note 必须配对使用：beginTransaction() 后必须调用 commit() 或 rollback()
 * @note 嵌套事务：SQLite 不支持嵌套事务，调用前确保没有未完成的事务
 * 
 * @warning 开始事务后务必记得提交或回滚，否则可能导致数据库锁定
 * 
 * 使用示例：保证多个操作的原子性
 * @code
 * // 场景：转账操作，A账户扣钱，B账户加钱，必须同时成功或同时失败
 * if (!dbm->beginTransaction()) {
 *     qWarning() << "开始事务失败";
 *     return;
 * }
 * 
 * // 执行多个相关操作
 * bool ok1 = dbm->preparedUpdate("UPDATE accounts SET balance = balance - ? WHERE id = ?", {100, accountA});
 * bool ok2 = dbm->preparedUpdate("UPDATE accounts SET balance = balance + ? WHERE id = ?", {100, accountB});
 * 
 * if (ok1 && ok2) {
 *     // 所有操作成功，提交事务
 *     dbm->commit();
 *     qDebug() << "转账成功";
 * } else {
 *     // 有操作失败，回滚事务，撤销所有修改
 *     dbm->rollback();
 *     qWarning() << "转账失败，已回滚";
 * }
 * @endcode
 */
bool AppSqlDatabase::beginTransaction()
{
    // 1. 自动加锁，保证线程安全
    QMutexLocker locker(&m_dbMutex);
    
    // 2. 获取数据库连接
    QSqlDatabase db = QSqlDatabase::database(kConnName);
    if (!db.isOpen()) {
        qWarning() << "数据库未打开，无法开始事务";
        return false;
    }
    
    // 3. 开始事务（从此刻起，所有修改都暂存，不会立即生效）
    if (!db.transaction()) {
        qWarning() << "开始事务失败:" << db.lastError().text();
        return false;
    }
    
    qDebug() << "事务已开始";
    return true;
}

/**
 * @brief 提交当前事务，将所有修改永久保存到数据库
 * @details 提交事务会将从 beginTransaction() 开始的所有数据库修改永久写入磁盘。
 *          提交成功后，其他程序/进程才能看到这些修改。
 * 
 * @return bool 是否成功提交事务
 *         - true: 提交成功，所有修改已永久保存
 *         - false: 提交失败，应该调用 rollback() 回滚
 * 
 * @note 线程安全：该方法内部使用 QMutexLocker 自动加锁
 * @note 必须先调用 beginTransaction()：否则该方法无效
 * @note 提交后释放锁：提交成功后，数据库锁会被释放，其他操作可以进行
 * 
 * @warning 提交失败时应该立即调用 rollback()，避免数据库处于不一致状态
 * 
 * 使用示例：见 beginTransaction()
 */
bool AppSqlDatabase::commit()
{
    // 1. 自动加锁，保证线程安全
    QMutexLocker locker(&m_dbMutex);
    
    // 2. 获取数据库连接
    QSqlDatabase db = QSqlDatabase::database(kConnName);
    if (!db.isOpen()) {
        qWarning() << "数据库未打开，无法提交事务";
        return false;
    }
    
    // 3. 提交事务（将所有修改永久保存到磁盘）
    if (!db.commit()) {
        qWarning() << "提交事务失败:" << db.lastError().text();
        return false;
    }
    
    qDebug() << "事务已提交";
    return true;
}

/**
 * @brief 回滚当前事务，撤销所有未提交的修改
 * @details 回滚事务会撤销从 beginTransaction() 开始的所有数据库修改，
 *          数据库恢复到事务开始前的状态，就像这些操作从未发生过。
 * 
 * @return bool 是否成功回滚事务
 *         - true: 回滚成功，所有修改已撤销
 *         - false: 回滚失败（很少见）
 * 
 * @note 线程安全：该方法内部使用 QMutexLocker 自动加锁
 * @note 必须先调用 beginTransaction()：否则该方法无效
 * @note 用于错误恢复：当事务中的某个操作失败时，应该回滚整个事务
 * 
 * @warning 回滚后，事务中的所有修改都会丢失，无法恢复
 * 
 * 使用示例1：操作失败时回滚
 * @code
 * dbm->beginTransaction();
 * 
 * bool ok = dbm->preparedUpdate("UPDATE users SET name = ? WHERE id = ?", {newName, userId});
 * if (!ok) {
 *     dbm->rollback();  // 操作失败，回滚事务
 *     qWarning() << "更新失败，已撤销所有修改";
 *     return;
 * }
 * 
 * dbm->commit();
 * @endcode
 * 
 * 使用示例2：异常处理
 * @code
 * try {
 *     dbm->beginTransaction();
 *     // 执行一系列数据库操作...
 *     dbm->commit();
 * } catch (...) {
 *     dbm->rollback();  // 发生异常，回滚所有修改
 *     qWarning() << "发生异常，事务已回滚";
 * }
 * @endcode
 */
bool AppSqlDatabase::rollback()
{
    // 1. 自动加锁，保证线程安全
    QMutexLocker locker(&m_dbMutex);
    
    // 2. 获取数据库连接
    QSqlDatabase db = QSqlDatabase::database(kConnName);
    if (!db.isOpen()) {
        qWarning() << "数据库未打开，无法回滚事务";
        return false;
    }
    
    // 3. 回滚事务（撤销所有修改，恢复到事务开始前的状态）
    if (!db.rollback()) {
        qWarning() << "回滚事务失败:" << db.lastError().text();
        return false;
    }
    
    qDebug() << "事务已回滚";
    return true;
}
