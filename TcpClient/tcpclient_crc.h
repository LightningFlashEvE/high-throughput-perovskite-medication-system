#ifndef TCPCLIENT_CRC_H
#define TCPCLIENT_CRC_H

#include <QString>
#include <QByteArray>

class TcpClient;

/**
 * @brief TcpClient的CRC计算辅助类
 * 
 * 负责处理不同协议的CRC计算和格式化
 * 支持ModBus-RTU和其他协议的差异化处理
 */
class TcpClientCrc
{
public:
    /**
     * @brief 帧数据结构
     */
    struct FrameData {
        QString field1;  // 字段1 (帧头等)
        QString field2;  // 字段2 (地址等)
        QString field3;  // 字段3 (功能码等)
        QString field4;  // 字段4 (数据等)
    };

    /**
     * @brief 构造函数
     * @param tcpClient TcpClient实例指针
     */
    explicit TcpClientCrc(TcpClient* tcpClient);

    /**
     * @brief 更新CRC显示
     * 主要入口函数，协调整个CRC计算流程
     */
    void updateCrcDisplay();

    /**
     * @brief 收集帧字段数据
     * @return FrameData 包含所有字段的数据结构
     */
    FrameData collectFrameData();

    /**
     * @brief 提取功能代码
     * 处理 "A:读取软件版本" -> "A" 的转换逻辑
     * @return QString 提取出的功能代码
     */
    QString extractFunctionCode();

    /**
     * @brief 构建帧内容
     * @param frameData 帧字段数据
     * @return QString 拼接后的帧内容
     */
    QString buildFrameContent(const FrameData &frameData);
    
    /**
     * @brief 直接收集数据并构建帧内容（合并版本）
     * @return QString 拼接后的帧内容
     */
    QString buildFrameContentDirect();

    /**
     * @brief 计算并格式化CRC
     * @param frameContent 帧内容字符串
     * @return QString 格式化后的CRC字符串
     */
    QString calculateAndFormatCrc(const QString &frameContent);

    /**
     * @brief 转换帧数据
     * 根据协议类型选择合适的数据转换方式
     * @param frameContent 帧内容字符串
     * @return QByteArray 转换后的字节数组
     */
    QByteArray convertFrameData(const QString &frameContent);

    /**
     * @brief 根据协议格式化CRC
     * @param crc CRC计算结果
     * @return QString 格式化后的CRC字符串
     */
    QString formatCrcByProtocol(quint16 crc);

private:
    TcpClient* m_tcpSocket;  ///< TcpClient实例指针

    /**
     * @brief 获取当前协议名称
     * @return QString 当前协议名称
     */
    QString getCurrentProtocolName() const;

    /**
     * @brief 判断是否为ModBus协议
     * @return bool true为ModBus协议
     */
    bool isModBusProtocol() const;

    /**
     * @brief 计算 CRC16 (Modbus 多项式 0xA001, 初始值 0xFFFF)
     * @param data 输入字节数组
     * @return 16位 CRC 值
     */
    static quint16 calculateCRC16(const QByteArray &data);
};

#endif // TCPCLIENT_CRC_H
