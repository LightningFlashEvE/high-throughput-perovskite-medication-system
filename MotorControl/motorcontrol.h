#ifndef MOTORCONTROL_H
#define MOTORCONTROL_H

#include <QDialog>
#include <QTimer>

class TcpClientCore;
class AppSqlDatabase;
class QLabel;

namespace Ui {
class MotorControl;
}

class MotorControl : public QDialog
{
    Q_OBJECT

public:
    explicit MotorControl(TcpClientCore *tcpMain,
                          TcpClientCore *tcpBalance,
                          AppSqlDatabase *db,
                          QWidget *parent = nullptr);
    ~MotorControl();

private slots:
    void updateStatusLights();

private:
    Ui::MotorControl *ui;
    TcpClientCore  *m_tcpMain    = nullptr;
    TcpClientCore  *m_tcpBalance = nullptr;
    AppSqlDatabase *m_db         = nullptr;
    QTimer         *m_statusTimer = nullptr;

    // 三个指示灯 label
    QLabel *m_ledMain    = nullptr;
    QLabel *m_ledBalance = nullptr;
    QLabel *m_ledDb      = nullptr;

    void setLed(QLabel *led, bool on);
};

#endif // MOTORCONTROL_H
