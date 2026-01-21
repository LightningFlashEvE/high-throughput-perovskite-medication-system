#ifndef COMMUINFODIALOG_H
#define COMMUINFODIALOG_H

#include <QDialog>
#include <QTextEdit>
#include <QPushButton>

class TcpClient;
class QLabel;

class CommuInfoDialog : public QDialog {
    Q_OBJECT
public:
    static CommuInfoDialog* getInstance();
    CommuInfoDialog(TcpClient* tcpClient, QWidget* parent = nullptr);
    ~CommuInfoDialog();

    enum MsgType {
        NONE_TYPE,
        MSG_SEND,
        MSG_READ,
        MSG_SEND_ASYNC_1,
        MSG_SEND_ASYNC_2,
        MSG_READ_BALANCE,
    };


    //void init(TcpClient* tcpSocket);
    void printMsg(const QString& msg, MsgType msgType = NONE_TYPE) const;

private slots:
    void clickClearMsgBtn();
    void clickTagBtn();
    void clickConnectionBtn();
    void clickBtn_ResetPos();
    void clickDisconnectBtn();

    void clickBtn_Y_Rel_P();
    void clickBtn_Y_Rel_N();

    // void onConnected();
    // void onConnectionError();

private:

    //bool sendCommand(const QString& cmd);

private:
    static CommuInfoDialog* m_instance;
    bool isConnecting{false};
    TcpClient* m_tcpClient{};

    QTextEdit* textEdit{};
    QLabel* tcpStatusLabel{};
};

#endif // COMMUINFODIALOG_H
