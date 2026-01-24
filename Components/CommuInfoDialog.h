#ifndef COMMUINFODIALOG_H
#define COMMUINFODIALOG_H

#include <QDialog>
#include <QTextEdit>
#include <QPushButton>
#include <QMap>

class QTcpSocket;
class QLabel;

class CommuInfoDialog : public QDialog {
    Q_OBJECT
public:
    static CommuInfoDialog* Ptr();
    CommuInfoDialog(QTcpSocket* tcpSocket = nullptr, QWidget* parent = nullptr);
    ~CommuInfoDialog();

    enum MsgType {
        NONE_TYPE,
        MSG_SEND,
        MSG_READ,
        MSG_SEND_ASYNC_1,
        MSG_SEND_ASYNC_2,
        MSG_READ_BALANCE,

        MSG_DEBUD,
        MSG_DEBUD_01,
        MSG_ORIGIN_TCP,
        MSG_ORIGIN_WRITE,
        DEBUD_onBalanceReadyRead,
        DEBUG_pollMotorPosition
    };

    enum ActionType {

    };

    void printMsg(const QString& msg, MsgType msgType = NONE_TYPE) const;
    void setSocket(QTcpSocket* tcpSocket);

private slots:
    void clickAnyBtn();

    void clickClearMsgBtn();
    void clickTagBtn();
    void clickConnectionBtn();
    void clickBtn_ResetPos();
    void clickDisconnectBtn();

    void clickBtn_Y_Rel_P();
    void clickBtn_Y_Rel_N();

    void onConnected();
    void onConnectionError();
    void onDisconnected();

private:
    void registerBtn(QPushButton* btn);
    void sendCommand(const QString& cmd);

private:
    static CommuInfoDialog* m_ptr;
    QMap<QPushButton*, ActionType> m_buttons;

    bool isConnecting{false};
    bool m_isStopRecv{false};
    QTcpSocket* m_tcpSocket{};

    QTextEdit* textEdit{};
    QLabel* tcpStatusLabel{};
};

#endif // COMMUINFODIALOG_H
