#ifndef HISTORYRECORDDIALOG_H
#define HISTORYRECORDDIALOG_H

#include <QDialog>
#include <QSqlDatabase>

class QStandardItemModel;
class QSqlDatabase;

class HistoryRecordDialog : public QDialog
{
    Q_OBJECT
public:
    static HistoryRecordDialog* Ptr();
    explicit HistoryRecordDialog(QWidget* parent = nullptr);

    void restartFlow();
    void stopFlow();
    void startFlow();

private slots:
    void clickClearBtn();

private:
    void initTableData();

private:
    static HistoryRecordDialog* m_ptr;
    QStandardItemModel* m_model{};
    int m_row{0};
    QScopedPointer<QSqlDatabase> m_db;
};

#endif // HISTORYRECORDDIALOG_H
