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

    enum ActionType {
        NODE,
        MOV_Y_P,
        MOV_Y_N,
        MOV_X_P,
        MOV_X_N,
        MOV_Z_UP,
        MOV_Z_DOWN,
        RESET_POS_X,
        RESET_POS_Y,
        RESET_POS_Z,
        STOP_Y
    };

private slots:
    void clickAnyBtn();
    void releaseAnyBtn();

private:
    void registerBtn(QPushButton* btn, ActionType pressType);
    void registerBtnRelease(QPushButton* btn, ActionType actionType);

    void sendCommand(const QString& cmd);

private:
    QTcpSocket* m_tcpSocket{};
    QMap<QPushButton*, ActionType> m_buttons;
    QMap<QPushButton*, ActionType> m_buttonRelease;
};

#endif // CONTROLPANEL_H
