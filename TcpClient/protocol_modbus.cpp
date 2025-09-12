#include "protocol_modbus.h"
#include <QDebug>
#include <QRegularExpression>

// 自动注册协议
REGISTER_PROTOCOL(ProtocolModBus, "ModBus-RTU");

QList<ProtocolField> ProtocolModBus::frameFields() const
{
    return {
        {"设备ID", "(1字节)", "01", 2, true, {"01", "02", "03", "04", "05", "06", "07", "88"}},
        {"功能码", "(1字节)", "06", 2, true, {"06", "03"}},
        {"寄存器地址", "(2字节)", "0105", 4, true, {}},
        {"寄存器数据", "(2字节)", "10进制数值", 4, true, {}},
        {"CRC校验", "(2字节)", "自动计算", 4, false, {}},
        {"操作", "(解析)", "解析数据", -1, false, {}}
    };
}

QList<ProtocolField> ProtocolModBus::parseFields() const
{
    return {
        {"设备ID", "(1字节)", "01", 2, false, {}},
        {"功能码", "(1字节)", "06", 2, false, {}},
        {"寄存器地址", "(2字节)", "0100", 4, false, {}},
        {"寄存器数据", "(2字节)", "0001", 4, false, {}},
        {"CRC校验", "(2字节)", "自动计算", 4, false, {}}
    };
}

QString ProtocolModBus::buildFrame(const QStringList &values) const
{
    if (values.size() < 4) {
        return QString();
    }
    
    QString frame = values[0] + values[1] + values[2] + values[3];
    QString checksum = calculateChecksum(frame);
    
    return frame + checksum;
}

QStringList ProtocolModBus::parseFrame(const QString &frame) const
{
    QString cleanFrame = frame;
    cleanFrame.remove(QRegularExpression("[\\s\\-\\:]"));
    
    if (cleanFrame.length() < 16) {
        return QStringList();
    }
    
    return {
        cleanFrame.mid(0, 2),   // 设备ID
        cleanFrame.mid(2, 2),   // 功能码
        cleanFrame.mid(4, 4),   // 寄存器地址
        cleanFrame.mid(8, 4),   // 寄存器数据
        cleanFrame.mid(12, 4)   // CRC校验
    };
}

QString ProtocolModBus::calculateChecksum(const QString &data) const
{
    // ModBus CRC-16计算 - 复制自485中正确的实现
    QByteArray bytes = QByteArray::fromHex(data.toLatin1());

    quint16 crc = 0xFFFF;  // 初始值
    for (int i = 0; i < bytes.length(); ++i) {
        crc ^= static_cast<quint8>(bytes[i]);  // XOR字节到CRC
        
        for (int j = 0; j < 8; ++j) {  // 处理8位
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;  // Modbus多项式（反向）
            } else {
                crc >>= 1;
            }
        }
    }
    qDebug() << "--crc--" << crc;
    
    // 返回小端格式（低字节在前）
    return QString("%1%2")
           .arg(static_cast<quint8>(crc & 0xFF), 2, 16, QChar('0'))
           .arg(static_cast<quint8>((crc >> 8) & 0xFF), 2, 16, QChar('0'))
           .toUpper();
}

bool ProtocolModBus::validateFrame(const QString &frame) const
{
    QString cleanFrame = frame;
    cleanFrame.remove(QRegularExpression("[\\s\\-\\:]"));
    return cleanFrame.length() >= 16;
}

QString ProtocolModBus::getFieldDescription(const QString &fieldName, const QString &value) const
{
    if (fieldName == "功能码") {
        return getFunctionDescription(value);
    } else if (fieldName == "寄存器地址") {
        return getAddressDescription(value);
    }
    return QString("值: %1").arg(value);
}

QStringList ProtocolModBus::getAvailableFunctions() const
{
    return {"03", "06"};
}

QString ProtocolModBus::getFunctionDescription(const QString &function) const
{
    static QMap<QString, QString> descriptions = {
        {"03", "读取保持寄存器"},
        {"06", "写入保持寄存器"},
        {"01", "读取线圈状态"},
        {"02", "读取输入状态"},
        {"05", "写单个线圈"},
        {"15", "写多个线圈"},
        {"16", "写多个保持寄存器"}
    };
    
    return descriptions.value(function, "未知功能码");
}

QString ProtocolModBus::getAddressDescription(const QString &address) const
{
    static QMap<QString, QString> descriptions = {
        {"0100", "电爪初始化"},
        {"0101", "旋转初始化"},
        {"0102", "设备急停"},
        {"0103", "电爪运动力矩"},
        {"0104", "电爪运动速度"},
        {"0105", "运行电爪并配置位置"},
        {"0106", "旋转运动力矩"},
        {"0107", "旋转运动速度"},
        {"0108", "运行旋转并配置角度"},
        {"0200", "查询电爪初始化状态"},
        {"0201", "查询旋转初始化状态"},
        {"0202", "查询电爪运行状态"},
        {"0203", "查询旋转运行状态"},
        {"0204", "查询电爪实时位置"},
        {"0205", "查询旋转实时位置"},
        {"0300", "电爪初始化方向"},
        {"0301", "旋转初始化方向"},
        {"0302", "保存数据"},
        {"0303", "变更设备ID"},
        {"0304", "释放使能电爪"},
        {"0305", "释放使能旋转"}
    };
    
    return descriptions.value(address, "未知寄存器地址");
}

QString ProtocolModBus::getDataDescription(const QString &address, const QString &data) const
{
    // 这里可以根据地址和数据值提供更详细的描述
    return QString("地址：%1，数据值: %1").arg(address, data);
}
