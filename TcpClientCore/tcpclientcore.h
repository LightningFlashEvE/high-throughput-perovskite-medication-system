#ifndef TCPCLIENTCORE_H
#define TCPCLIENTCORE_H

#include <QObject>
#include <QTcpSocket>
#include <QNetworkProxy>

class TcpClientCore : public QObject
{
    Q_OBJECT
public:
    explicit TcpClientCore(QObject *parent = nullptr);
    ~TcpClientCore();

    // tcp连接-接口
    bool connectToTcp(const QString& localIP, const QString& remoteIP, quint16 remotePort, bool proxyDisabled);

    // 调用TcpClientCrc类计算crc16
    quint16 calculateCrc16(const QByteArray& data);

    // tcp发送信息-接口
    // asciiOrHex: true 表示ascii，false表示16进制发送
    bool sendMessage(const QByteArray& content, bool asciiOrHex);

    // 断开连接
    void disconnectFromTcp();

    // 检查连接状态
    bool isConnected() const;

    /**
     * 构建带CRC的消息
     * @param data 原始数据字符串（如 ">0AA"）
     * @return 拼接了CRC的完整消息（如 ">0AAA3FD"）
     */
    QString buildMessageWithCrc(const QString& data);
    
    /**
     * 电爪专用：构建带ModBus CRC的消息
     * @param data 原始十六进制数据字符串（如 "01060100001"）
     * @return 拼接了ModBus CRC的完整消息（如 "0106010000149F6"）
     */
    QString buildGripperMessageWithCrc(const QString& data);

    /**
     * 构建设备命令（自动添加CRC）
     * @param deviceNum 设备编号字符串（如 "0A", "01", "07"）
     * @param functionCode 功能码字符串（如 "A" 表示获取版本信息）
     * @param commandData 命令数据（可选，默认为空）
     * @return 完整的带CRC的命令（如 ">0AAA3FD"）
     * 
     * 示例：
     *   buildDeviceCommand("0A", "A", "") -> ">0AAA3FD"
     *   buildDeviceCommand("0A", "G", "") -> ">0AGA17D"
     */
    QString buildDeviceCommand(const QString& deviceNum, const QString& functionCode, const QString& commandData = "");
    
    /**
     * 构建设备命令（重载版本，4个参数）
     * @param deviceNum 设备ID/编号（如 "0A", "01", "07"）
     * @param functionCode 功能码（如 "M" 表示移动）
     * @param registerAddress 寄存器地址（如 "0000"）
     * @param registerData 寄存器数据（如 "1234"）
     * @return 完整的带CRC的命令
     * 
     * 示例：
     *   buildDeviceCommand("0A", "M", "0000", "1234") -> ">0AM00001234XXXX"
     */
    QString buildDeviceCommand(const QString& deviceNum, 
                              const QString& functionCode, 
                              const QString& registerAddress, 
                              const QString& registerData);
    
    /**
     * 构建设备命令（重载版本，5个参数，支持十进制数据）
     * @param deviceNum 设备ID/编号（如 "05"）
     * @param functionCode 功能码（如 "06"）
     * @param registerAddress 寄存器地址（如 "0105"）
     * @param decimalData 十进制数据（如 100，支持大数值）
     * @param dataWidth 数据位宽（默认4位，如4表示"0064"，8表示"00000064"，最大16位）
     * @return 完整的带CRC的命令
     * 
     * 支持范围：
     *   4位: 0 - 65,535 (0xFFFF)
     *   8位: 0 - 4,294,967,295 (0xFFFFFFFF)
     *   16位: 0 - 18,446,744,073,709,551,615 (0xFFFFFFFFFFFFFFFF)
     * 
     * 示例：
     *   buildDeviceCommand("05", "06", "0105", 100, 4) -> "0506010500649F6"
     *   buildDeviceCommand("05", "06", "0105", 100, 8) -> "050601050000006449F6"
     *   buildDeviceCommand("05", "06", "0105", 4294967295, 8) -> "05060105FFFFFFFFXXXX"
     */
    QString buildDeviceCommand(const QString& deviceNum, 
                              const QString& functionCode, 
                              const QString& registerAddress, 
                              qint64 decimalData,
                              int dataWidth = 4);

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString& errorMsg);
    void dataReceived(const QByteArray& data);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);

private:
    QTcpSocket* m_tcpSocket;
};

#endif // TCPCLIENTCORE_H
