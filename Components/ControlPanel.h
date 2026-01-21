#ifndef CONTROLPANEL_H
#define CONTROLPANEL_H

#include <QWidget>

class QTcpSocket;

class ControlPanel : public QWidget
{
    Q_OBJECT
public:
    ControlPanel(QTcpSocket* tcpSocket, QWidget* parent = nullptr);
private:
    QTcpSocket* m_tcpSocket{};
};

#endif // CONTROLPANEL_H
