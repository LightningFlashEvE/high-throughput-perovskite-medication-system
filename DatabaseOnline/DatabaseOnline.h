#ifndef DATABASEONLINE_H
#define DATABASEONLINE_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QVariant>

/**
 * @brief 远端 MySQL 数据库操作类
 * @details 提供连接、查询、增删改、事务等基本数据库操作功能
 */
class DatabaseOnline : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseOnline(QObject *parent = nullptr);
    ~DatabaseOnline();

    /**
     * @brief 连接到远端数据库
     * @param host 主机地址（默认：192.168.10.30）
     * @param port 端口号（默认：3306）
     * @param dbName 数据库名（默认：PhenoLabHT）
     * @param user 用户名（默认：root）
     * @param password 密码（默认：Zq17122320_）
     * @param charset 字符集（默认：utf8mb4）
     * @return true 连接成功；false 连接失败
     */
    bool connectToDatabase(const QString &host = "192.168.10.30",
                          int port = 3306,
                          const QString &dbName = "PhenoLabHT",
                          const QString &user = "root",
                          const QString &password = "Zq17122320_",
                          const QString &charset = "utf8mb4");

    /**
     * @brief 断开数据库连接
     */
    void disconnectFromDatabase();

    /**
     * @brief 检查数据库是否已连接
     * @return true 已连接；false 未连接
     */
    bool isConnected() const;

    /**
     * @brief 执行查询语句（SELECT）
     * @param sql SQL 查询语句
     * @return QSqlQuery 查询结果对象
     * 
     * 使用示例：
     * QSqlQuery query = db->executeQuery("SELECT * FROM users WHERE id = 1");
     * if (query.next()) {
     *     QString name = query.value("name").toString();
     * }
     */
    QSqlQuery executeQuery(const QString &sql);

    /**
     * @brief 执行更新语句（INSERT/UPDATE/DELETE）
     * @param sql SQL 更新语句
     * @return true 执行成功；false 执行失败
     */
    bool executeUpdate(const QString &sql);

    /**
     * @brief 执行带参数绑定的查询（防SQL注入）
     * @param sql SQL 语句（使用 ? 作为占位符）
     * @param bindValues 绑定的参数值列表
     * @return QSqlQuery 查询结果对象
     * 
     * 使用示例：
     * QSqlQuery query = db->executePreparedQuery(
     *     "SELECT * FROM users WHERE name = ? AND age > ?",
     *     {QVariant("张三"), QVariant(18)}
     * );
     */
    QSqlQuery executePreparedQuery(const QString &sql, const QVariantList &bindValues);

    /**
     * @brief 执行带参数绑定的更新（防SQL注入）
     * @param sql SQL 语句（使用 ? 作为占位符）
     * @param bindValues 绑定的参数值列表
     * @return true 执行成功；false 执行失败
     */
    bool executePreparedUpdate(const QString &sql, const QVariantList &bindValues);

    /**
     * @brief 获取最后插入记录的自增ID
     * @return 最后插入的 ID，失败返回 -1
     */
    qint64 lastInsertId() const;

    /**
     * @brief 开始事务
     * @return true 成功；false 失败
     */
    bool beginTransaction();

    /**
     * @brief 提交事务
     * @return true 成功；false 失败
     */
    bool commitTransaction();

    /**
     * @brief 回滚事务
     * @return true 成功；false 失败
     */
    bool rollbackTransaction();

    /**
     * @brief 获取最后一次错误信息
     * @return 错误信息字符串
     */
    QString lastError() const;

signals:
    /**
     * @brief 数据库连接成功信号
     */
    void connected();

    /**
     * @brief 数据库连接断开信号
     */
    void disconnected();

    /**
     * @brief 数据库操作错误信号
     * @param errorMsg 错误信息
     */
    void errorOccurred(const QString &errorMsg);

private:
    QSqlDatabase m_db;              // 数据库连接对象
    QString m_connectionName;       // 连接名称（用于多连接管理）
    QString m_lastError;            // 最后一次错误信息
    qint64 m_lastInsertId;          // 最后插入的ID

    /**
     * @brief 生成唯一的连接名称
     * @return 连接名称字符串
     */
    static QString generateConnectionName();
};

#endif // DATABASEONLINE_H
