#ifndef APP_SQL_DATABASE_H
#define APP_SQL_DATABASE_H

#include <QObject>
#include <QString>
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

    // ODBC 连接名称（供外部共用）
    static const char* kConnName;

private:
    bool openDatabase();
};

#endif // APP_SQL_DATABASE_H
