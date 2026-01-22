#ifndef CONTROLPANNELDIALOG_H
#define CONTROLPANNELDIALOG_H

#include <QDialog>

class QTcpSocket;

class ControlPanelDialog : public QDialog
{
    Q_OBJECT
public:
    static ControlPanelDialog* Ptr();
    ControlPanelDialog(QTcpSocket* tcpSocket, QWidget* parent = nullptr);

private:
    static ControlPanelDialog* m_ptr;
    QTcpSocket* m_tcpSocket{};
};

#endif // CONTROLPANNELDIALOG_H
