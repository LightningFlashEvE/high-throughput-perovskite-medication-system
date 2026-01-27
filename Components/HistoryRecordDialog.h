#ifndef HISTORYRECORDDIALOG_H
#define HISTORYRECORDDIALOG_H

#include <QDialog>
#include <QSqlDatabase>
#include "RecipeData.h"

class QStandardItemModel;
class QSqlDatabase;
class QTableView;

class HistoryRecordDialog : public QDialog
{
    Q_OBJECT
public:
    static HistoryRecordDialog* Ptr();
    explicit HistoryRecordDialog(QWidget* parent = nullptr);
    ~HistoryRecordDialog();

    void startFlow();
    void restartFlow();
    void interruptFlow();
    void completeFlow();

    void setRecipeData(const RecipeData& recipeData);

private slots:
    void clickClearBtn();

private:
    void initTableData();
    void initDatebase();
    void queryCurrentRecord(const QString& curStartDatetime);

    void updateCurrentState(const QString& curStartDatetime,
                            const QString& stateStr,
                            const QString& formattedTime);

private:
    static HistoryRecordDialog* m_ptr;
    QStandardItemModel* m_model{};
    int m_row{0};
    QSqlDatabase m_db;
    QTableView* tableView{};
    RecipeData m_recipeData;
};

#endif // HISTORYRECORDDIALOG_H
