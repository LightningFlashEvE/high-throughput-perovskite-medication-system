#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class TcpClient;
class CommuInfoDialog;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void clickAction();
    void clickDebugAction();

private:
    TcpClient* m_tcpClient{};
    CommuInfoDialog* m_CommuInfoDialog{};
};
#endif // MAINWINDOW_H
