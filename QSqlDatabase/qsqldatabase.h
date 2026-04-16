#ifndef APP_SQL_DATABASE_H
#define APP_SQL_DATABASE_H

#include <QObject>
#include <QString>
#include <QMutex>
#include <QVariant>
#include <QtSql/QSqlQuery>

// 前向声明，避免与 Qt 的 QSqlDatabase 命名冲突
class AppSqlDatabase : public QObject
{
    Q_OBJECT
public:
    // 构造函数：连接到 MySQL（192.168.10.170:3306/PhenoLabHT）
    explicit AppSqlDatabase(QObject *parent = nullptr);

    // 析构函数：关闭数据库连接
    ~AppSqlDatabase();

    // 关闭数据库连接
    void close();

    // 规范化表名：将 '-' 替换为 '_'
    static QString normalizeTableName(const QString &name);

    // 检查数据库是否已连接
    bool isConnected() const;

    // 执行 SQL 查询，返回 QSqlQuery 对象
    QSqlQuery query(const QString &sql);

    // 执行预处理 SQL 查询（支持绑定参数），返回 QSqlQuery 对象
    QSqlQuery preparedQuery(const QString &sql, const QVariantList &bindValues);

    // 执行 INSERT/UPDATE/DELETE 语句，返回是否成功
    bool executeUpdate(const QString &sql);

    // 执行预处理 INSERT/UPDATE/DELETE 语句（支持绑定参数），返回是否成功
    bool preparedUpdate(const QString &sql, const QVariantList &bindValues);

    // 事务控制
    bool beginTransaction();
    bool commit();
    bool rollback();

    // ODBC 连接名称（供外部共用）
    static const char* kConnName;

private:
    bool openDatabase();
    mutable QMutex m_dbMutex;
};

#endif // APP_SQL_DATABASE_H
