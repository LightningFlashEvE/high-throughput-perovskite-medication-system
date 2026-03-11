#ifndef COLLISIONRECOVERYDIALOG_H
#define COLLISIONRECOVERYDIALOG_H

#include <QDialog>
#include <QLabel>
#include "TcpClientCore/tcpclientcore.h"

// 前向声明
class MainWindow;

namespace Ui {
class CollisionRecoveryDialog;
}

class CollisionRecoveryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CollisionRecoveryDialog(QWidget *parent = nullptr);
    ~CollisionRecoveryDialog();

private slots:
    /**
     * 检查TCP连接状态和对象有效性
     * @return 如果TCP核心对象存在且已连接，返回true；否则返回false
     */
    bool checkTcpConnection();

    void pushButton_Tight_clicked();
    void pushButton_xMove_clicked();
    void pushButton_yMove_clicked();
    void pushButton_zMove_clicked();
    void pushButton_Liquid_clicked();
    void pushButton_Magnet_clicked();

private:
    /**
     * 通用调试信息打印函数
     * @param message 要显示的调试信息
     */
    void printDebug(const QString& message);

private:
    Ui::CollisionRecoveryDialog *ui;
    QLabel *sliderValueLabel;  // 滑块值显示标签
    
    // 缓存TCP核心对象指针
    TcpClientCore *m_tcpCore;
    TcpClientCore *m_tcpBalanceCore;
};

#endif // COLLISIONRECOVERYDIALOG_H
