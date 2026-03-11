#include "DatabaseOnline.h"
#include <QDebug>
#include <QDateTime>
#include <QUuid>

DatabaseOnline::DatabaseOnline(QObject *parent)
    : QObject{parent}
    , m_lastInsertId(-1)
{
    // 生成唯一的连接名称
    m_connectionName = generateConnectionName();
}

DatabaseOnline::~DatabaseOnline()
{
    disconnectFromDatabase();
}

QString DatabaseOnline::generateConnectionName()
{
    // 使用时间戳 + UUID 生成唯一连接名
    return QString("DatabaseOnline_%1_%2")
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

bool DatabaseOnline::connectToDatabase(const QString &host,
                                       int port,
                                       const QString &dbName,
                                       const QString &user,
                                       const QString &password,
                                       const QString &charset)
{
    // 如果已经连接，先断开
    if (m_db.isOpen()) {
        disconnectFromDatabase();
    }

    // 创建数据库连接
    m_db = QSqlDatabase::addDatabase("QMYSQL", m_connectionName);
    m_db.setHostName(host);
    m_db.setPort(port);
    m_db.setDatabaseName(dbName);
    m_db.setUserName(user);
    m_db.setPassword(password);

    // 设置连接选项（字符集等）
    QString connectOptions = QString("MYSQL_OPT_RECONNECT=1;CLIENT_INTERACTIVE=1");
    if (!charset.isEmpty()) {
        connectOptions += QString(";MYSQL_SET_CHARSET_NAME=%1").arg(charset);
    }
    m_db.setConnectOptions(connectOptions);

    // 尝试打开连接
    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        qWarning() << "DatabaseOnline: Failed to connect to database:"
                   << host << ":" << port
                   << "Error:" << m_lastError;
        emit errorOccurred(m_lastError);
        return false;
    }

    qDebug() << "DatabaseOnline: Successfully connected to database:"
             << host << ":" << port << "/" << dbName;
    emit connected();
    return true;
}

void DatabaseOnline::disconnectFromDatabase()
{
    if (m_db.isOpen()) {
        QString dbName = m_db.databaseName();
        m_db.close();
        qDebug() << "DatabaseOnline: Disconnected from database:" << dbName;
        emit disconnected();
    }

    // 移除数据库连接
    if (QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool DatabaseOnline::isConnected() const
{
    return m_db.isOpen() && m_db.isValid();
}

QSqlQuery DatabaseOnline::executeQuery(const QString &sql)
{
    QSqlQuery query(m_db);

    if (!isConnected()) {
        m_lastError = "Database is not connected";
        qWarning() << "DatabaseOnline: Cannot execute query - database not connected";
        emit errorOccurred(m_lastError);
        return query;
    }

    if (!query.exec(sql)) {
        m_lastError = query.lastError().text();
        qWarning() << "DatabaseOnline: Query execution failed:"
                   << sql
                   << "Error:" << m_lastError;
        emit errorOccurred(m_lastError);
    } else {
        qDebug() << "DatabaseOnline: Query executed successfully:" << sql;
    }

    return query;
}

bool DatabaseOnline::executeUpdate(const QString &sql)
{
    if (!isConnected()) {
        m_lastError = "Database is not connected";
        qWarning() << "DatabaseOnline: Cannot execute update - database not connected";
        emit errorOccurred(m_lastError);
        return false;
    }

    QSqlQuery query(m_db);
    if (!query.exec(sql)) {
        m_lastError = query.lastError().text();
        qWarning() << "DatabaseOnline: Update execution failed:"
                   << sql
                   << "Error:" << m_lastError;
        emit errorOccurred(m_lastError);
        return false;
    }

    // 保存最后插入的ID（如果是INSERT操作）
    m_lastInsertId = query.lastInsertId().toLongLong();

    qDebug() << "DatabaseOnline: Update executed successfully:" << sql;
    return true;
}

QSqlQuery DatabaseOnline::executePreparedQuery(const QString &sql, const QVariantList &bindValues)
{
    QSqlQuery query(m_db);

    if (!isConnected()) {
        m_lastError = "Database is not connected";
        qWarning() << "DatabaseOnline: Cannot execute prepared query - database not connected";
        emit errorOccurred(m_lastError);
        return query;
    }

    // 准备SQL语句
    if (!query.prepare(sql)) {
        m_lastError = query.lastError().text();
        qWarning() << "DatabaseOnline: Failed to prepare query:"
                   << sql
                   << "Error:" << m_lastError;
        emit errorOccurred(m_lastError);
        return query;
    }

    // 绑定参数
    for (const QVariant &value : bindValues) {
        query.addBindValue(value);
    }

    // 执行查询
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        qWarning() << "DatabaseOnline: Prepared query execution failed:"
                   << sql
                   << "Bind values:" << bindValues
                   << "Error:" << m_lastError;
        emit errorOccurred(m_lastError);
    } else {
        qDebug() << "DatabaseOnline: Prepared query executed successfully:" << sql;
    }

    return query;
}

bool DatabaseOnline::executePreparedUpdate(const QString &sql, const QVariantList &bindValues)
{
    if (!isConnected()) {
        m_lastError = "Database is not connected";
        qWarning() << "DatabaseOnline: Cannot execute prepared update - database not connected";
        emit errorOccurred(m_lastError);
        return false;
    }

    QSqlQuery query(m_db);

    // 准备SQL语句
    if (!query.prepare(sql)) {
        m_lastError = query.lastError().text();
        qWarning() << "DatabaseOnline: Failed to prepare update:"
                   << sql
                   << "Error:" << m_lastError;
        emit errorOccurred(m_lastError);
        return false;
    }

    // 绑定参数
    for (const QVariant &value : bindValues) {
        query.addBindValue(value);
    }

    // 执行更新
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        qWarning() << "DatabaseOnline: Prepared update execution failed:"
                   << sql
                   << "Bind values:" << bindValues
                   << "Error:" << m_lastError;
        emit errorOccurred(m_lastError);
        return false;
    }

    // 保存最后插入的ID（如果是INSERT操作）
    m_lastInsertId = query.lastInsertId().toLongLong();

    qDebug() << "DatabaseOnline: Prepared update executed successfully:" << sql;
    return true;
}

qint64 DatabaseOnline::lastInsertId() const
{
    return m_lastInsertId;
}

bool DatabaseOnline::beginTransaction()
{
    if (!isConnected()) {
        m_lastError = "Database is not connected";
        qWarning() << "DatabaseOnline: Cannot begin transaction - database not connected";
        emit errorOccurred(m_lastError);
        return false;
    }

    if (!m_db.transaction()) {
        m_lastError = m_db.lastError().text();
        qWarning() << "DatabaseOnline: Failed to begin transaction:" << m_lastError;
        emit errorOccurred(m_lastError);
        return false;
    }

    qDebug() << "DatabaseOnline: Transaction started";
    return true;
}

bool DatabaseOnline::commitTransaction()
{
    if (!isConnected()) {
        m_lastError = "Database is not connected";
        qWarning() << "DatabaseOnline: Cannot commit transaction - database not connected";
        emit errorOccurred(m_lastError);
        return false;
    }

    if (!m_db.commit()) {
        m_lastError = m_db.lastError().text();
        qWarning() << "DatabaseOnline: Failed to commit transaction:" << m_lastError;
        emit errorOccurred(m_lastError);
        return false;
    }

    qDebug() << "DatabaseOnline: Transaction committed";
    return true;
}

bool DatabaseOnline::rollbackTransaction()
{
    if (!isConnected()) {
        m_lastError = "Database is not connected";
        qWarning() << "DatabaseOnline: Cannot rollback transaction - database not connected";
        emit errorOccurred(m_lastError);
        return false;
    }

    if (!m_db.rollback()) {
        m_lastError = m_db.lastError().text();
        qWarning() << "DatabaseOnline: Failed to rollback transaction:" << m_lastError;
        emit errorOccurred(m_lastError);
        return false;
    }

    qDebug() << "DatabaseOnline: Transaction rolled back";
    return true;
}

QString DatabaseOnline::lastError() const
{
    return m_lastError;
}
