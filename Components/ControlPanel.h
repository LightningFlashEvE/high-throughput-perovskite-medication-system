#ifndef CONTROLPANEL_H
#define CONTROLPANEL_H

#include <QWidget>
#include <QMap>

class QTcpSocket;
class QPushButton;

class ControlPanel : public QWidget
{
    Q_OBJECT
public:
    ControlPanel(QTcpSocket* tcpSocket, QWidget* parent = nullptr);

    enum ButtonType {
        MOV_Y_P,
        MOV_Y_N
    };

private slots:
    void clickAnyBtn();

private:
    void registerBtn(QPushButton* btn, ButtonType btnType);
    void sendCommand(const QString& cmd);

private:
    QTcpSocket* m_tcpSocket{};
    QMap<QPushButton*, ButtonType> m_buttons;
};

#endif // CONTROLPANEL_H
