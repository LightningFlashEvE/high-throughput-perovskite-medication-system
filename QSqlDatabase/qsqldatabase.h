#ifndef APP_SQL_DATABASE_H
#define APP_SQL_DATABASE_H

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QMessageBox>
#include <QMutex>
#include <QtSql/QSqlQuery>

// 前向声明，避免与 Qt 的 QSqlDatabase 命名冲突
class AppSqlDatabase : public QObject
{
    Q_OBJECT
public:
    // 构造函数：检查数据库文件是否被占用（默认检查 liquid.db）
    explicit AppSqlDatabase(const QString &dbFilePath = QString("liquid.db"), QObject *parent = nullptr);

    // 析构函数：关闭数据库连接
    ~AppSqlDatabase();

    // 关闭数据库连接
    void close();

    // 规范化表名：将 '-' 替换为 '_'
    static QString normalizeTableName(const QString &name);

    // 执行 SQL 查询，返回 QSqlQuery 对象（线程安全）
    QSqlQuery query(const QString &sql);
    
    // 执行参数化查询（防SQL注入，线程安全）
    QSqlQuery preparedQuery(const QString &sql, const QVariantList &bindValues);
    
    // 执行更新操作（INSERT/UPDATE/DELETE，线程安全）
    bool executeUpdate(const QString &sql);
    
    // 执行参数化更新（防SQL注入，线程安全）
    bool preparedUpdate(const QString &sql, const QVariantList &bindValues);
    
    // 事务支持（线程安全）
    bool beginTransaction();
    bool commit();
    bool rollback();

private:
    bool openDatabase(const QString &dbFilePath);
    void ensureDefaultSchema();
    void createDefaultTables();
    void seedDefaultData();
    bool isTableEmpty(const QString &tableName) const;
    bool ensureNamedRecord(const QString &tableName,
                           const QString &nameField,
                           const QVariant &nameValue,
                           const QVariantMap &values) const;
    bool insertRow(const QString &tableName, const QVariantMap &values) const;
    
    // 线程安全保护
    mutable QMutex m_dbMutex;  // 保护数据库访问的互斥锁
};

#endif // APP_SQL_DATABASE_H
