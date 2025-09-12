#include "tcpclient_crc.h"
#include "tcpclient.h"
#include <QDebug>
#include <QComboBox>
#include <QLineEdit>

TcpClientCrc::TcpClientCrc(TcpClient* tcpClient)
    : m_tcpClient(tcpClient)
{
}

// 1/9  主函数
void TcpClientCrc::updateCrcDisplay()
{
    //qDebug() << "执行CRC更新 - 当前协议:" << getCurrentProtocolName();
    
    // 1. 收集帧字段数据
    FrameData frameData = collectFrameData();
    
    // 2. 构建帧内容
    QString frameContent = buildFrameContent(frameData);
    
    // 3. 计算并显示CRC
    if (!frameContent.isEmpty()) {
        QString crcStr = calculateAndFormatCrc(frameContent);
        m_tcpClient->setLineEdit5Text(crcStr);
    } else {
        m_tcpClient->clearLineEdit5();
    }
}

/*********************************** 下面是被动查看 ***********************************/

// 2/9
QString TcpClientCrc::getCurrentProtocolName() const
{
    return m_tcpClient->getCurrentProtocolName();
}

// 3/9
TcpClientCrc::FrameData TcpClientCrc::collectFrameData()
{
    FrameData data;
    data.field1 = m_tcpClient->getComboBox1Text();
    data.field2 = m_tcpClient->getComboBox2Text();
    data.field3 = extractFunctionCode();
    data.field4 = m_tcpClient->getLineEdit4Text();
    return data;
}

// 4/9
QString TcpClientCrc::extractFunctionCode()
{
    // 优先使用存储的数据值
    QString functionCode = m_tcpClient->getComboBox3Data();
    
    if (functionCode.isEmpty()) {
        // 从显示文本中解析 (格式: "A:读取软件版本" -> "A")
        QString displayText = m_tcpClient->getComboBox3Text();
        functionCode = displayText.contains(':') ? 
                      displayText.split(':').first() : displayText;
    }
    
    return functionCode;
}

// 5/9
QString TcpClientCrc::buildFrameContent(const FrameData &frameData)
{
    QString content = frameData.field1 + frameData.field2 + 
                     frameData.field3 + frameData.field4;
    
    // 处理特殊帧头转换 (3E -> >)
    if (frameData.field1 == "3E") {
        content = ">" + frameData.field2 + frameData.field3 + frameData.field4;
    }
    
    //qDebug() << "构建帧内容:" << content;
    return content;
}

// 6/9
QString TcpClientCrc::calculateAndFormatCrc(const QString &frameContent)
{
    // 1. 根据协议转换数据
    QByteArray dataForCrc = convertFrameData(frameContent);
    
    // 2. 计算CRC
    quint16 crc = m_tcpClient->calculateCRC16(dataForCrc);
    
    // 3. 格式化CRC
    QString crcStr = formatCrcByProtocol(crc);
    
    //qDebug() << "CRC计算完成:" << QString("0x%1").arg(crc, 4, 16, QChar('0')) << "→" << crcStr;
    return crcStr;
}

// 7/9
QByteArray TcpClientCrc::convertFrameData(const QString &frameContent)
{
    const bool isModBus = isModBusProtocol();
    
    if (isModBus) {
        // ModBus协议：十六进制转换
        QByteArray data = QByteArray::fromHex(frameContent.toLatin1());
        //qDebug() << "ModBus数据转换:" << frameContent << "→" << data.toHex().toUpper();
        return data;
    } else {
        // 其他协议：UTF-8转换
        QByteArray data = frameContent.toUtf8();
        //qDebug() << "标准数据转换:" << frameContent << "→" << data.toHex().toUpper();
        return data;
    }
}

// 8/9
QString TcpClientCrc::formatCrcByProtocol(quint16 crc)
{
    const bool isModBus = isModBusProtocol();
    
    if (isModBus) {
        // ModBus协议：小端格式 (低字节在前)
        return QString("%1%2")
               .arg(static_cast<quint8>(crc & 0xFF), 2, 16, QChar('0'))
               .arg(static_cast<quint8>((crc >> 8) & 0xFF), 2, 16, QChar('0'))
               .toUpper();
    } else {
        // 其他协议：大端格式 (高字节在前)
        return QString("%1").arg(crc, 4, 16, QChar('0')).toUpper();
    }
}

// 9/9
bool TcpClientCrc::isModBusProtocol() const
{
    return getCurrentProtocolName() == "ModBus-RTU";
}
