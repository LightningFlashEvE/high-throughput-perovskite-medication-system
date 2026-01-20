#ifndef CONTROLPANNEL_H
#define CONTROLPANNEL_H

#include <QWidget>

class QTcpSocket;

class ControlPannel : public QWidget {
    Q_OBJECT
public:
    ControlPannel(QWidget* parent = nullptr);

private slots:
    void clickBtn01();
    void clickBtn10();
    void clickBtn12();
    void clickBtn21();

    void clickBtn31();
    void clickBtn41();
    void clickBtn32();
    void clickBtn42();

    void clickBtn51();
    void clickBtn61();
    void clickBtn71();

private:
    QTcpSocket* m_tcpSocket{};
};

#endif // CONTROLPANNEL_H
