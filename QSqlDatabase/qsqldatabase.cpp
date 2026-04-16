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
#include <QMutexLocker>

const char* AppSqlDatabase::kConnName = "app_sqlite_conn";

AppSqlDatabase::AppSqlDatabase(QObject *parent)
    : QObject{parent}
{
    if (!openDatabase()) {
        const QString errorMsg = QStringLiteral("无法连接 MySQL: 192.168.10.170:3306/PhenoLabHT");
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

    QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL", kConnName);
    db.setHostName(host);
    db.setPort(port.toInt());
    db.setDatabaseName(database);
    db.setUserName(user);
    db.setPassword(password);
    db.setConnectOptions("MYSQL_OPT_CONNECT_TIMEOUT=5");

    if (!db.open()) {
        qWarning() << "QMYSQL 打开失败:" << db.lastError().text();
        return false;
    }

    qDebug() << "QMYSQL 已连接:" << host << ":" << port << "/" << database;
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
