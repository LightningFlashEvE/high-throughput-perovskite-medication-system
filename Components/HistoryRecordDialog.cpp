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
    // QScopedPointer<QSqlDatabase> db(new QSqlDatabase);
    // m_db.reset(db.take());

    QSqlDatabase* m_db = new QSqlDatabase;
    m_db->addDatabase("HistoryRecordDialog");

    // 设置数据库文件路径
    m_db->setDatabaseName("data.db");

    // 打开数据库
    if (!m_db->open()) {
        qDebug() << "Error: Unable to open database" << m_db->lastError().text();
    }

    // 创建表
    QSqlQuery query;
    QString createTableQuery = "CREATE TABLE IF NOT EXISTS person ("
                               "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                               "name TEXT, "
                               "age INTEGER, "
                               "height REAL)";
    if (!query.exec(createTableQuery)) {
        qDebug() << "Error: Unable to create table" << query.lastError().text();
    }

    // 插入数据
    QString insertQuery = "INSERT INTO person (name, age, height) "
                          "VALUES ('John Doe', 33, 5.9), "
                          "('Jane Smith', 25, 5.5), "
                          "('Alice Johnson', 35, 5.7)";
    if (!query.exec(insertQuery)) {
        qDebug() << "Error: Unable to insert data" << query.lastError().text();
    }

    //qDebug() << "Database created and data inserted successfully.";

    QVBoxLayout* rootVLayout = new QVBoxLayout(this);

    QHBoxLayout* buttonsLayout = new QHBoxLayout(this);
    QVBoxLayout* tableLayout = new QVBoxLayout(this);

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

    connect(clearBtn, &QPushButton::clicked, this, &HistoryRecordDialog::clickClearBtn);
}

void HistoryRecordDialog::addRecord(){
    m_model->setRowCount(m_row + 1);

    // 获取当前时间
    QDateTime currentDateTime = QDateTime::currentDateTime();
    QString formattedTime = currentDateTime.toString("yyyy-MM-dd HH:mm:ss");
    QModelIndex columDateTime = m_model->index(m_row, 0);
    QModelIndex columState = m_model->index(m_row, 3);
    m_model->setData(columDateTime, formattedTime);
    m_model->setData(columState, "进行中");

    m_row++;
}

void HistoryRecordDialog::clickClearBtn() {

}
