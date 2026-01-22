#ifndef CONTROLPANEL_L_H
#define CONTROLPANEL_L_H

#include <QWidget>
#include <QLabel>
#include <QMap>

class QTcpSocket;
class QPushButton;

class ControlPanel_L : public QWidget
{
    Q_OBJECT
public:
    ControlPanel_L(QTcpSocket* tcpSocket, QWidget* parent = nullptr);

    enum ActionType {
        NODE,
        MOV_Y_P,
        MOV_Y_N,
        MOV_X_P,
        MOV_X_N,
        MOV_Z_UP,
        MOV_Z_DOWN,
        CLAW_OPEN,
        CLAW_CLOSED,
        RESET_POS_X,
        RESET_POS_Y,
        RESET_POS_Z,
        STOP_X,
        STOP_Y,
        STOP_Z
    };

private slots:
    void clickAnyBtn();
    void releaseAnyBtn();

    void onConnected();
    void onConnectionError();
    void onDisconnected();

private:
    void registerBtn(QPushButton* btn, ActionType pressType);
    void registerBtnRelease(QPushButton* btn, ActionType actionType);

    void sendCommand(const QString& cmd);

private:
    QTcpSocket* m_tcpSocket{};
    QLabel* m_tcpStatusLabel{};
    QMap<QPushButton*, ActionType> m_buttons;
    QMap<QPushButton*, ActionType> m_buttonRelease;
};

#endif // CONTROLPANEL_L_H
