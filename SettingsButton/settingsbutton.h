#ifndef SETTINGSBUTTON_H
#define SETTINGSBUTTON_H

#include <QWidget>
#include <QVector>
#include <QPointF>
#include <QTextEdit>
#include <QByteArray>
#include <QtGlobal>
#include <QSlider>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

class Modbus485;

namespace Ui {
class SettingsButton;
}

class SettingsButton : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsButton(QWidget *parent = nullptr);
    ~SettingsButton();

    void LED(bool changeColor);

    // 在设置页面上显示当前位置，如 "(col,row)"
    void setLocation(int col, int row);

    /****** 485通信公共接口  up ******/
    /**
     * 485通信完整接口，支持：
     * 1. 串口连接管理（连接/断开/状态查询）
     * 2. 数据发送（原始字节/16进制字符串/Modbus命令）
     * 3. Modbus参数管理（设置/获取ID/功能码/地址/数据）
     * 4. CRC自动计算（标准Modbus CRC-16算法）
     * 5. 日志管理（添加/清除/获取日志内容）
     * 6. 信号通知（连接状态/数据接收/错误信息）
     *
     * 使用方法详见 SettingsButton/README_485API.md
     */
    
    // 串口连接管理
    bool connect485(const QString &portName, int baudRate);
    void disconnect485();
    bool is485Connected() const;
    QStringList getAvailablePorts() const;
    
    // 数据发送接口
    void send485Data(const QByteArray &data);
    void send485HexString(const QString &hexString);
    bool sendModbusCommand(); // 发送当前输入框中的Modbus命令
    
    // Modbus数据设置/获取
    void setModbusID(const QString &id);
    void setModbusFunction(const QString &func);
    void setModbusAddress(const QString &addr);
    void setModbusData(const QString &data);
    QString getModbusID() const;
    QString getModbusFunction() const;
    QString getModbusAddress() const;
    QString getModbusData() const;
    QString getModbusCRC() const;
    
    // CRC计算工具
    QString calculateModbusCRC(const QString &hexData) const;
    
    // 备用CRC计算函数（按您提供的示例实现，供您自用）
    quint16 calculateCRC16_Alternative(const QByteArray &data) const;
    
    // 日志管理
    void clearLog();
    void appendLog(const QString &message);
    QString getLogContent() const;
    
    // 旋转角度控制
    void setRotationAngle(int angle);
    int getRotationAngle() const;
    /****** 485通信公共接口  down ******/

signals:
    void moveUpClicked();
    void moveRightClicked();
    void moveLeftClicked();
    void moveDownClicked();
    // 坐标编辑信号：当 X/Y 文本框提交（回车或失焦）后触发
    void positionEdited(int col, int row);
    
    /****** 485通信信号  up ******/
    // 485连接状态变化
    void serial485Connected(bool connected);
    // 485数据接收（原始字节）
    void serial485DataReceived(const QByteArray &data);
    // 485数据接收（16进制字符串）
    void serial485HexReceived(const QString &hexString);
    // 485错误信息
    void serial485Error(const QString &error);
    // 旋转角度变化信号
    void rotationAngleChanged(int angle);
    /****** 485通信信号  down ******/
private:
    Ui::SettingsButton *ui;

/******  试管状态机 ******/
public:
    enum class TubeState {
        Empty,
        Full,
        Using,
        Error,
        Disabled
    };

    struct Tube {
        QPointF center;
        qreal   radius;
        TubeState state = TubeState::Empty;
    };

    // 设置指定试管状态
    void setTubeState(int idx, TubeState s);

protected:
    void paintEvent(QPaintEvent*) override;

private slots:
    // 485通信相关槽函数
    void onConnectClicked();
    void onSerialDataReceived(const QByteArray &data);
    void onSerialConnectionChanged(bool connected);
    void onSerialError(const QString &error);
    // CRC自动更新槽函数
    void onModbusDataChanged();
    // 旋转角度滑动条处理
    void onRotationAngleChanged(int value);

private:
    QVector<Tube> m_tubes; // 此处仅使用一个，中心(50,50)
    QRect tubeRect(int idx) const;
    
    // 485通信实例
    Modbus485 *modbus485 = nullptr;
    QTextEdit *m_serverLog = nullptr;  // 485通信日志显示区域
    
    // 初始化485相关UI
    void init485UI();
    // 初始化旋转角度滑动条
    void initRotationSlider();
    
    // CRC-16计算函数（Modbus标准）
    quint16 calculateCRC16(const QByteArray &data) const;
/******  试管状态机 ******/
};

#endif // SETTINGSBUTTON_H
