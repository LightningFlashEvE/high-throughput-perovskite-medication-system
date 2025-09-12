#include "protocol_cstep.h"
#include <QRegularExpression>
#include <QDebug>

// 🎯 关键：这一行自动注册CStep协议！
REGISTER_PROTOCOL(ProtocolCStep, "CStep");

QList<ProtocolField> ProtocolCStep::frameFields() const
{
    return {
        {"帧头", "(2字符)", "3E", 2, true, {"3E", ">"}},
        {"从机地址", "(2字符)", "01", 2, true, {"01", "02", "03", "04", "05", "06", "07", "08"}},
        {"功能代码", "(1字符)", "A", 1, true, getAvailableCommands()},
        {"命令数据", "(N字符)", "输入命令数据（可选）", -1, true, {}},
        {"校验和", "(4字符)", "自动计算", 4, false, {}},
        {"帧尾", "(2字符)", "0D0A", 4, true, {"", "0D0A", "\\r\\n"}}
    };
}

QList<ProtocolField> ProtocolCStep::parseFields() const
{
    return {
        {"设备ID", "(1字节)", "01", 2, false, {}},
        {"功能码", "(1字符)", "A", 1, false, {}},
        {"寄存器地址", "(2字节)", "0100", 4, false, {}},
        {"寄存器数据", "(2字节)", "0001", 4, false, {}},
        {"CRC校验", "(2字节)", "自动计算", 4, false, {}}
    };
}

QString ProtocolCStep::buildFrame(const QStringList &values) const
{
    if (values.size() < 4) {
        return QString();
    }
    
    QString frame = values[0] + values[1] + values[2] + values[3];
    QString checksum = calculateChecksum(frame);
    frame += checksum + values[5]; // 添加校验和和帧尾
    
    return frame;
}

QStringList ProtocolCStep::parseFrame(const QString &frame) const
{
    // CStep协议解析逻辑
    QString cleanFrame = frame;
    cleanFrame.remove(QRegularExpression("[\\s\\-\\:]"));
    
    if (cleanFrame.length() < 8) {
        return QStringList();
    }
    
    return {
        cleanFrame.mid(0, 2),   // 设备ID
        cleanFrame.mid(2, 1),   // 功能码
        cleanFrame.mid(3, 2),   // 地址
        cleanFrame.mid(5, 2),   // 数据
        cleanFrame.right(4)     // 校验和
    };
}

QString ProtocolCStep::calculateChecksum(const QString &data) const
{
    // CStep的CRC计算逻辑 (与ADP1000相同)
    quint16 crc = 0xFFFF;
    QByteArray bytes = data.toUtf8();
    
    for (char byte : bytes) {
        crc ^= static_cast<quint8>(byte);
        for (int i = 0; i < 8; i++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0x1A001;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return QString("%1").arg(crc, 4, 16, QChar('0')).toUpper();
}

bool ProtocolCStep::validateFrame(const QString &frame) const
{
    return frame.length() >= 8;
}

QString ProtocolCStep::getFieldDescription(const QString &fieldName, const QString &value) const
{
    if (fieldName == "功能代码") {
        return getCommandDescription(value);
    }
    return QString("值: %1").arg(value);
}

QStringList ProtocolCStep::getAvailableCommands() const
{
    return {
        "A", "a", "B", "b", "C", "c", "D", "d", "E", "f", "G", "g", "H", "h",
        "I", "i", "j", "K", "L", "M", "N", "O", "P", "R", "r", "T", "U", "w",
        "X", "Y", "y", "+", "-", "*", "/"
    };
}

QString ProtocolCStep::getCommandDescription(const QString &command) const
{
    static QMap<QString, QString> descriptions = {
        {"A", "读取软件版本"},
        {"a", "释放或励磁电机"},
        {"B", "设置运动速度"},
        {"b", "设置电流"},
        {"C", "设置参数"},
        {"c", "设置初始化电流"},
        {"D", "电机绝对值运动"},
        {"d", "读取运动状态"},
        {"E", "读取编码器坐标"},
        {"f", "设置软件原点"},
        {"G", "电机初始化"},
        {"g", "读取初始化状态"},
        {"H", "设置液位探测距离"},
        {"h", "电机相对值运动"},
        {"I", "电机力矩运动"},
        {"i", "绝对值力矩组合"},
        {"j", "绝对值液面探测"},
        {"K", "电机JOG控制"},
        {"L", "读取传感器状态"},
        {"M", "读取运动速度"},
        {"N", "读取加减速参数"},
        {"O", "读取配置参数"},
        {"P", "读取液面检测距离"},
        {"R", "设置回原点偏移"},
        {"r", "读取回原点偏移"},
        {"T", "变更电机地址"},
        {"U", "保存所有参数"},
        {"w", "读取回原点参数"},
        {"X", "读取所有参数"},
        {"Y", "设置所有参数"},
        {"y", "设置回原点阈值"},
        {"+", "设置力矩限度"},
        {"-", "读取力矩限度"},
        {"*", "设置探测速度"},
        {"/", "读取探测速度"}
    };
    
    return descriptions.value(command, "未知指令");
}
