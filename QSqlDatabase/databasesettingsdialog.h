#ifndef DATABASESETTINGSDIALOG_H
#define DATABASESETTINGSDIALOG_H

#include <QDialog>

class QLineEdit;
class QPushButton;
class AppSqlDatabase;

class DatabaseSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DatabaseSettingsDialog(AppSqlDatabase *dbm, QWidget *parent = nullptr);

private slots:
    void onConnect();
    void onDisconnect();

private:
    void loadFromIni();
    void saveToIni();

    QLineEdit *m_host;
    QLineEdit *m_port;
    QLineEdit *m_database;
    QLineEdit *m_user;
    QLineEdit *m_password;
    QPushButton *m_btnConnect;
    QPushButton *m_btnDisconnect;

    AppSqlDatabase *m_dbm;
};

#endif // DATABASESETTINGSDIALOG_H
