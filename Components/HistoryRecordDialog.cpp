#include "HistoryRecordDialog.h"

#include <QLabel>
#include <QPushButton>
#include <QLayout>
#include <QTableView>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QScopedPointer>
#include <QMessageBox>

static int showMessageBox(const QString& title, const QString& text)
{
    // 创建消息框
    QMessageBox msgBox;
    msgBox.setWindowTitle(title);
    msgBox.setText(text);
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);

    return msgBox.exec();
}

HistoryRecordDialog* HistoryRecordDialog::m_ptr = nullptr;

HistoryRecordDialog* HistoryRecordDialog::Ptr() {
    return m_ptr;
}

HistoryRecordDialog::HistoryRecordDialog(QWidget* parent) :
    QDialog(parent)
{
    m_ptr = this;
    resize(800, 600);
    setWindowFlags(Qt::Dialog | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);

    // 创建一个 SQLite 数据库连接
    QScopedPointer<QSqlDatabase> db(new QSqlDatabase);
    m_db.reset(db.take());

    QSqlDatabase m_db = QSqlDatabase::addDatabase("QSQLITE");
    //m_db->addDatabase("HistoryRecordDialog");

    // 设置数据库文件路径
    m_db.setDatabaseName("data.db");

    // 打开数据库
    if (!m_db.open()) {
        qDebug() << "Error: Unable to open database" << m_db.lastError().text();
    }

    // 创建表
    QSqlQuery query;
    QString createTableQuery = "CREATE TABLE IF NOT EXISTS tab_history_record ("
                               "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                               "date_time TEXT, "
                               "age INTEGER, "
                               "height REAL,"
                               "state TEXT)";
    if (!query.exec(createTableQuery)) {
        qDebug() << "Error: Unable to create table" << query.lastError().text();
    }

    // 关闭数据库
    //m_db.close();

    //qDebug() << "Database created and data inserted successfully.";

    QVBoxLayout* rootVLayout = new QVBoxLayout(this);

    QHBoxLayout* buttonsLayout = new QHBoxLayout;
    QVBoxLayout* tableLayout = new QVBoxLayout;

    QPushButton* clearBtn = new QPushButton("清空记录");
    clearBtn->setFixedWidth(70);
    buttonsLayout->addWidget(clearBtn);
    buttonsLayout->addStretch();

    // 创建视图和模型
    QTableView* tableView = new QTableView();
    m_model = new QStandardItemModel(0, 4);
    tableView->setModel(m_model);
    // 禁用编辑功能
    tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // 设置表头
    m_model->setHeaderData(0, Qt::Horizontal, "开始时间");
    m_model->setHeaderData(1, Qt::Horizontal, "溶剂");
    m_model->setHeaderData(2, Qt::Horizontal, "体积");
    m_model->setHeaderData(3, Qt::Horizontal, "状态");

    // 设置每一列的宽度
    tableView->setColumnWidth(0, 200);  // 设置第一列宽度为100
    tableView->setColumnWidth(1, 150);  // 设置第二列宽度为150
    tableView->setColumnWidth(2, 200);  // 设置第三列宽度为200
    tableView->setColumnWidth(3, 150);  // 设置第四列宽度为250

    tableLayout->addWidget(tableView);

    rootVLayout->addLayout(buttonsLayout);
    rootVLayout->addLayout(tableLayout);

    initTableData();

    connect(clearBtn, &QPushButton::clicked, this, &HistoryRecordDialog::clickClearBtn);
}

void HistoryRecordDialog::addRecord(){
    m_model->setRowCount(m_row + 1);

    // 获取当前时间
    QDateTime currentDateTime = QDateTime::currentDateTime();
    QString formattedTime = currentDateTime.toString("yyyy-MM-dd HH:mm:ss");
    QString stateStr = "进行中";
    QModelIndex columDateTime = m_model->index(m_row, 0);
    QModelIndex columState = m_model->index(m_row, 3);
    m_model->setData(columDateTime, formattedTime);
    m_model->setData(columState, stateStr);

    // 插入数据
    QSqlQuery query;
    QString insertQuery = QString("INSERT INTO tab_history_record (date_time, age, height, state) "
                                  "VALUES ('%1', 20, 5.9, '%2')")
                              .arg(formattedTime)
                              .arg(stateStr);

    if (!query.exec(insertQuery)) {
        qDebug() << "Error: Unable to insert data" << query.lastError().text();
    }

    m_row++;
}

void HistoryRecordDialog::clickClearBtn() {
    if (!m_row) {
        // 没有数据，不做任何处理
        return;
    }

    int ret = showMessageBox("提示", "删除后不可恢复，您确定要继续吗?");
    if (ret == QMessageBox::Ok) {
        //qDebug() << "用户点击了 确定";
    } else if (ret == QMessageBox::Cancel) {
        return;
    }

    QSqlQuery query;
    // 清空所有数据
    if (!query.exec("DELETE FROM tab_history_record")) {
        qDebug() << "Error: Unable to delete data" << query.lastError().text();
    }

    // 对数据库进行优化（压缩空闲空间
    query.exec("VACUUM");

    m_model->removeRows(0, m_row);
    m_model->removeRows(3, m_row);
    m_row = 0;
    m_model->setRowCount(0);
}

void HistoryRecordDialog::initTableData() {
    QSqlQuery query;

    // 执行 SQL 查询
    if (!query.exec("SELECT date_time, age, height, state FROM tab_history_record")) {
        qDebug() << "Error: Unable to execute query" << query.lastError().text();
    }

    // 迭代查询结果
    while (query.next()) {
        QString dateTime = query.value(0).toString();       // 获取 id 列
        int name = query.value(1).toInt();  // 获取 name 列
        int age = query.value(2).toInt();      // 获取 age 列
        QString state = query.value(3).toString();  // 获取 height 列

        // 打印每一行的数据
        //qDebug() << "ID:" << id << ", Name:" << name << ", Age:" << age << ", Height:" << height;

        m_model->setRowCount(m_row + 1);
        QModelIndex columDateTime = m_model->index(m_row, 0);
        QModelIndex columState = m_model->index(m_row, 3);
        m_model->setData(columDateTime, dateTime);
        m_model->setData(columState, state);
        m_row++;
    }
}
