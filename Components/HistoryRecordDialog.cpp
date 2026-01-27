#include "HistoryRecordDialog.h"
#include "Common.h"
#include "MyDelegate.h"

#include <QLabel>
#include <QPushButton>
#include <QLayout>
#include <QTableView>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QPainter>

HistoryRecordDialog* HistoryRecordDialog::m_ptr = nullptr;

HistoryRecordDialog* HistoryRecordDialog::Ptr() {
    return m_ptr;
}

HistoryRecordDialog::HistoryRecordDialog(QWidget* parent) :
    QDialog(parent)
{
    m_ptr = this;
    resize(900, 600);
    setWindowFlags(Qt::Dialog | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);

    initDatebase();

    QVBoxLayout* rootVLayout = new QVBoxLayout(this);

    QHBoxLayout* buttonsLayout = new QHBoxLayout;
    QVBoxLayout* tableLayout = new QVBoxLayout;

    QPushButton* clearBtn = new QPushButton("清空记录");
    clearBtn->setFixedWidth(70);
    buttonsLayout->addWidget(clearBtn);
    buttonsLayout->addStretch();

    // 创建视图和模型
    tableView = new QTableView;
    m_model = new QStandardItemModel(0, 5);
    tableView->setModel(m_model);
    tableView->setItemDelegate(new MyDelegate());
    tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // 设置表头
    m_model->setHeaderData(0, Qt::Horizontal, "开始时间");
    m_model->setHeaderData(1, Qt::Horizontal, "状态");
    m_model->setHeaderData(2, Qt::Horizontal, "配方(名称/用量)");
    m_model->setHeaderData(3, Qt::Horizontal, "备注");
    m_model->setHeaderData(4, Qt::Horizontal, "结束时间");

    // 设置每一列的宽度
    tableView->setColumnWidth(0, 130);
    tableView->setColumnWidth(1, 70);
    tableView->setColumnWidth(2, 450);
    tableView->setColumnWidth(3, 50);
    tableView->setColumnWidth(4, 130);

    tableLayout->addWidget(tableView);

    rootVLayout->addLayout(buttonsLayout);
    rootVLayout->addLayout(tableLayout);

    // 需数据连接与model创建好了，才能执行，从数据库中读取数据写入model
    initTableData();

    connect(clearBtn, &QPushButton::clicked, this, &HistoryRecordDialog::clickClearBtn);
}

HistoryRecordDialog::~HistoryRecordDialog() {
    interruptFlow();

    // 关闭数据库
    m_db.close();
}

void HistoryRecordDialog::setRecipeData(const RecipeData& recipeData) {
    m_recipeData = recipeData;
}

void HistoryRecordDialog::completeFlow() {
    QString curState = m_model->data(m_model->index(0, 1)).toString();
    if (curState != "进行中") {
        return;
    }

    QString curStartDatetime = m_model->data(m_model->index(0, 0)).toString();
    queryCurrentRecord(curStartDatetime);

    QString formattedTime = Common::getCurDateTime();
    QString stateStr = "成功";

    // 修改表格数据
    m_model->setData(m_model->index(0, 1), stateStr);
    m_model->setData(m_model->index(0, 4), formattedTime);

    // 修改数据库数据
    updateCurrentState(curStartDatetime, stateStr, formattedTime);
}

void HistoryRecordDialog::restartFlow() {
    interruptFlow();
    startFlow();
}

void HistoryRecordDialog::interruptFlow() {
    // 获取对应单元格的值
    QString curState = m_model->data(m_model->index(0, 1)).toString();
    if (curState != "进行中") {
        return;
    }

    QString curStartDatetime = m_model->data(m_model->index(0, 0)).toString();
    queryCurrentRecord(curStartDatetime);

    QString formattedTime = Common::getCurDateTime();
    QString stateStr = "失败";

    // 修改表格数据
    m_model->setData(m_model->index(0, 1), stateStr);
    m_model->setData(m_model->index(0, 4), formattedTime);

    // 修改数据库数据
    updateCurrentState(curStartDatetime, stateStr, formattedTime);
}

void HistoryRecordDialog::startFlow(){
    QString formattedTime = Common::getCurDateTime();
    QString stateStr = "进行中";

    m_model->insertRow(0);
    tableView->setRowHeight(0, 80);

    QString formula = "化学式：";
    formula += m_recipeData.formula;

    QString precursor = "前驱体：";
    for (const QString& key : m_recipeData.precursor.keys()) {
        QString name = key;
        QString dosage = m_recipeData.precursor[key];
        precursor += name + " " + dosage + " mg, ";
    }

    QString solvent = "溶剂：";
    for (const QString& key : m_recipeData.solvent.keys()) {
        QString name = key;
        QString dosage = m_recipeData.solvent[key];
        solvent += name + " " + dosage + " ml, ";
    }

    QString recipe = formula + "\n" + precursor + "\n" +solvent;

    m_model->setData(m_model->index(0, 0), formattedTime);
    m_model->setData(m_model->index(0, 1), stateStr);
    m_model->setData(m_model->index(0, 2), recipe);
    m_model->setData(m_model->index(0, 4), "---");

    // 插入数据
    QSqlQuery query;
    QString insertQuery = QString("INSERT INTO tab_history_record "
                                  "(start_datetime, state, recipe, note_message, end_datetime) "
                                  "VALUES ('%1', '%2', '%3', '---', '---')")
                              .arg(formattedTime)
                              .arg(stateStr)
                              .arg(recipe);

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

    int ret = Common::showMessageBox("提示", "删除后不可恢复，您确定要继续吗?");
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

    // 对数据库进行优化（压缩空闲空间）
    query.exec("VACUUM");

    //m_model->removeRows(0, m_row);
    //m_model->removeRows(3, m_row);
    m_row = 0;
    m_model->setRowCount(0);
}

void HistoryRecordDialog::initTableData() {
    QSqlQuery query;

    // 执行 SQL 查询
    if (!query.exec("SELECT start_datetime, state, recipe, note_message, end_datetime FROM tab_history_record")) {
        qDebug() << "Error: Unable to execute query" << query.lastError().text();
    }

    // 迭代查询结果
    while (query.next()) {
        QString dateTime = query.value(0).toString();
        QString state = query.value(1).toString();
        QString recipe = query.value(2).toString();
        QString endDateTime = query.value(4).toString();

        m_model->insertRow(0);
        tableView->setRowHeight(0, 80);
        m_model->setData(m_model->index(0, 0), dateTime);
        m_model->setData(m_model->index(0, 1), state);
        m_model->setData(m_model->index(0, 2), recipe);
        m_model->setData(m_model->index(0, 4), endDateTime);

        m_row++;
    }
}

void HistoryRecordDialog::initDatebase() {
    // 创建一个 SQLite 数据库连接
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    //m_db->addDatabase("HistoryRecordDialog");
    m_db.setDatabaseName("data.db");
    if (!m_db.open()) {
        qDebug() << "Error: Unable to open database" << m_db.lastError().text();
    }

    // 创建表
    QSqlQuery query;
    QString createTableQuery = "CREATE TABLE IF NOT EXISTS tab_history_record ("
                               "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                               "start_datetime TEXT, "
                               "state TEXT,"
                               "recipe TEXT, "
                               "note_message TEXT,"
                               "end_datetime TEXT)";
    if (!query.exec(createTableQuery)) {
        qDebug() << "Error: Unable to create table" << query.lastError().text();
    }
}

void HistoryRecordDialog::queryCurrentRecord(const QString& curStartDatetime) {
    QSqlQuery query;
    query.prepare("SELECT start_datetime FROM tab_history_record WHERE start_datetime = :start_datetime");
    query.bindValue(":start_datetime", curStartDatetime);  // 绑定字符串参数
    if (!query.exec()) {
        qDebug() << "Query Error:" << query.lastError().text();
    }

    // 检查是否找到了该行
    if (query.next()) {
        // 获取当前记录的字段值
        QString startDatetime = query.value(0).toString();
        //qDebug() << "Before update: startDatetime:" << startDatetime;
    } else {
        qDebug() << "No record found with Name:" << curStartDatetime;
    }
}

void HistoryRecordDialog::updateCurrentState(const QString& curStartDatetime,
                                             const QString& stateStr,
                                             const QString& formattedTime)
{
    QSqlQuery query;
    // 更新数据
    query.prepare("UPDATE tab_history_record "
                  "SET state = :state, end_datetime = :end_datetime WHERE start_datetime = :start_datetime");
    query.bindValue(":start_datetime", curStartDatetime);
    query.bindValue(":state", stateStr);
    query.bindValue(":end_datetime", formattedTime);

    if (!query.exec()) {
        qDebug() << "Update Error:" << query.lastError().text();
    }
}
