#include "protocol_adp1000.h"
#include <QDebug>
#include <QRegularExpression>

// 自动注册协议
REGISTER_PROTOCOL(ProtocolADP1000, "ADP1000");

QList<ProtocolField> ProtocolADP1000::frameFields() const
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

QList<ProtocolField> ProtocolADP1000::parseFields() const
{
    return {
        {"设备ID", "(1字节)", "01", 2, false, {}},
        {"功能码", "(1字符)", "A", 1, false, {}},
        {"寄存器地址", "(2字节)", "0100", 4, false, {}},
        {"寄存器数据", "(2字节)", "0001", 4, false, {}},
        {"CRC校验", "(2字节)", "自动计算", 4, false, {}}
    };
}

QString ProtocolADP1000::buildFrame(const QStringList &values) const
{
    if (values.size() < 4) {
        return QString();
    }
    
    QString frame = values[0] + values[1] + values[2] + values[3];
    QString checksum = calculateChecksum(frame);
    frame += checksum + values[5]; // 添加校验和和帧尾
    
    return frame;
}

QStringList ProtocolADP1000::parseFrame(const QString &frame) const
{
    // ADP1000协议解析逻辑
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

QString ProtocolADP1000::calculateChecksum(const QString &data) const
{
    // ADP1000的CRC计算逻辑
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

bool ProtocolADP1000::validateFrame(const QString &frame) const
{
    return frame.length() >= 8;
}

QString ProtocolADP1000::getFieldDescription(const QString &fieldName, const QString &value) const
{
    if (fieldName == "功能代码") {
        return getCommandDescription(value);
    }
    return QString("值: %1").arg(value);
}

QStringList ProtocolADP1000::getAvailableCommands() const
{
    return {
        "A", "B", "b", "C", "c", "d", "E", "F", "f", "G", "g", "H", "h",
        "I", "i", "J", "j", "K", "k", "M", "N", "n", "O", "0", "P", "p",
        "Q", "q", "R", "r", "T", "U", "V", "v", "W", "w", "=", "2", "3",
        "4", "5", "6", "+", "-", "S", "s"
    };
}

QString ProtocolADP1000::getCommandDescription(const QString &command) const
{
    static QMap<QString, QString> descriptions = {
        {"A", "读取程序版本"},
        {"B", "设置吐液速度"},
        {"b", "读取吐液速度"},
        {"C", "设置电容探测阈值"},
        {"c", "读取电容探测阈值"},
        {"d", "查询运行状态"},
        {"E", "查询容积信息"},
        {"F", "吸吐混匀动作"},
        {"f", "查询混匀次数"},
        {"G", "空气泵初始化"},
        {"g", "查询复位状态"},
        {"H", "设置报警阈值"},
        {"h", "读取报警阈值"},
        {"I", "设置气压监测"},
        {"i", "读取气压监测"},
        {"J", "设置回吸参数"},
        {"j", "读取回吸参数"},
        {"K", "设置补偿值"},
        {"k", "读取补偿值"},
        {"M", "首次回吸"},
        {"N", "气压探测"},
        {"n", "吸液动作"},
        {"O", "设置配方类"},
        {"0", "读取配方类"},
        {"P", "二次回吸"},
        {"p", "吐液动作"},
        {"Q", "退TIP动作"},
        {"q", "查询TIP状态"},
        {"R", "设置回程差"},
        {"r", "读取回程差"},
        {"T", "修改设备ID"},
        {"U", "保存参数"},
        {"V", "设置复位速度"},
        {"v", "读取复位速度"},
        {"W", "设置运行电流"},
        {"w", "读取运行电流"},
        {"=", "设备重启"},
        {"2", "设置切断速度"},
        {"3", "读取切断速度"},
        {"4", "设置吸液速度"},
        {"5", "读取吸液速度"},
        {"6", "读取气压值"},
        {"+", "设置吸吐比例"},
        {"-", "读取吸吐比例"},
        {"S", "设置电容阈值"},
        {"s", "读取电容阈值"}
    };
    
    return descriptions.value(command, "未知指令");
}
