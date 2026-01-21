#include "tcpclient_crc.h"
#include "tcpclient.h"
#include <QDebug>
#include <QComboBox>
#include <QLineEdit>

TcpClientCrc::TcpClientCrc(TcpClient* tcpClient)
    : m_tcpSocket(tcpClient)
{
}

// 1/9  主函数
void TcpClientCrc::updateCrcDisplay()
{
    //qDebug() << "执行CRC更新 - 当前协议:" << getCurrentProtocolName();
    
    // 1. 收集帧字段数据并构建内容
    QString frameContent = buildFrameContentDirect();

    // 3. 计算并显示CRC
    if (!frameContent.isEmpty()) {
        QString crcStr = calculateAndFormatCrc(frameContent);
        m_tcpSocket->setLineEdit5Text(crcStr);
    } else {
        m_tcpSocket->clearLineEdit5();
    }
}

/*********************************** 下面是被动查看 ***********************************/

// 2/9
QString TcpClientCrc::getCurrentProtocolName() const
{
    return m_tcpSocket->getCurrentProtocolName();
}

// 3/9
TcpClientCrc::FrameData TcpClientCrc::collectFrameData()
{
    FrameData data;
    data.field1 = m_tcpSocket->getComboBox1Text();
    data.field2 = m_tcpSocket->getComboBox2Text();
    data.field3 = extractFunctionCode();
    data.field4 = m_tcpSocket->getLineEdit4Text();
    return data;
}

// 4/9
QString TcpClientCrc::extractFunctionCode()
{
    // 优先使用存储的数据值
    QString functionCode = m_tcpSocket->getComboBox3Data();
    
    if (functionCode.isEmpty()) {
        // 从显示文本中解析 (格式: "A:读取软件版本" -> "A")
        QString displayText = m_tcpSocket->getComboBox3Text();
        functionCode = displayText.contains(':') ? 
                      displayText.split(':').first() : displayText;
    }
    
    return functionCode;
}

// 5/9
QString TcpClientCrc::buildFrameContent(const FrameData &frameData)
{
    // 1) 处理帧头标准化 (3E -> >)
    QString normalizedField1 = (frameData.field1 == "3E") ? QString(">") : frameData.field1;
    
    // 2) 处理命令数据：十进制 -> 4位HEX大写；否则原样
    QString normalizedField4 = frameData.field4;
    static const QRegularExpression kOnlyDigitsRe(QStringLiteral("^[0-9]+$"));
    if (kOnlyDigitsRe.match(frameData.field4).hasMatch()) {
        bool ok = false;
        const int dec = frameData.field4.toInt(&ok, 10);
        if (ok && dec >= 0) {
            normalizedField4 = QString("%1").arg(dec, 4, 16, QChar('0')).toUpper();
        }
    }
    
    // 3) 拼接标准化后的内容
    QString content = normalizedField1 + frameData.field2 + frameData.field3 + normalizedField4;
    
    qDebug() << "构建帧内容:" << content;
    return content;
}

// 6/9
QString TcpClientCrc::calculateAndFormatCrc(const QString &frameContent)
{
    // 1. 根据协议转换数据
    QByteArray dataForCrc = convertFrameData(frameContent);
    
    // 2. 计算CRC
    quint16 crc = calculateCRC16(dataForCrc);
    
    // 3. 格式化CRC
    //qDebug() << "aaa" << QString("%1").arg(crc, 4, 16, QChar('0')).toUpper();;
    QString crcStr = formatCrcByProtocol(crc);
    //qDebug() << "bbb" << crcStr;
    
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

QString TcpClientCrc::buildFrameContentDirect()
{
    // 直接收集并处理字段数据，合并 collectFrameData + buildFrameContent
    
    // 1) 收集原始数据
    QString field1 = m_tcpSocket->getComboBox1Text();
    QString field2 = m_tcpSocket->getComboBox2Text();
    QString field3 = extractFunctionCode();
    QString field4 = m_tcpSocket->getLineEdit4Text();
    
    // 2) 处理帧头标准化 (3E -> >)
    QString normalizedField1 = (field1 == "3E") ? QString(">") : field1;
    
    // 3) 处理命令数据：十进制 -> 保持前导零个数的HEX大写；否则原样
    QString normalizedField4 = field4;
    static const QRegularExpression kOnlyDigitsRe(QStringLiteral("^[0-9]+$"));
    if (kOnlyDigitsRe.match(field4).hasMatch()) {
        bool ok = false;
        const int dec = field4.toInt(&ok, 10);
        if (ok && dec >= 0) {
            // 先转换为基础HEX
            QString baseHex = QString("%1").arg(dec, 0, 16).toUpper();
            
            // 计算原输入的前导零个数：去掉前导零后的长度差
            QString trimmed = field4;
            while (trimmed.startsWith('0') && trimmed.length() > 1) {
                trimmed.remove(0, 1);
            }
            const int leadingZeros = field4.length() - trimmed.length();
            
            // 在HEX前补上相同数量的前导零
            normalizedField4 = QString("0").repeated(leadingZeros) + baseHex;
        }
    }
    
    // 4) 拼接标准化后的内容
    QString content = normalizedField1 + field2 + field3 + normalizedField4;
    
    qDebug() << "构建帧内容:" << content;
    return content;
}

quint16 TcpClientCrc::calculateCRC16(const QByteArray &data)
{
    quint16 crc = 0xFFFF;  // 初始值
    for (int i = 0; i < data.length(); ++i) {
        crc ^= static_cast<quint8>(data[i]);  // XOR字节到CRC

        for (int j = 0; j < 8; ++j) {  // 处理8位
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;  // Modbus多项式（反向）
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}
