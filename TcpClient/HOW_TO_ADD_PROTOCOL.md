# 如何添加新协议

这个模块化协议系统让您可以非常轻松地添加新协议。只需几个简单步骤！

## 🚀 快速添加步骤

### 1. 创建协议头文件 `protocol_yourname.h`

```cpp
#ifndef PROTOCOL_YOURNAME_H
#define PROTOCOL_YOURNAME_H

#include "protocolbase.h"

class ProtocolYourName : public ProtocolBase
{
public:
    // 基本信息 - 必须实现
    QString name() const override { return "YourProtocol"; }
    QString displayName() const override { return "您的协议 (设备X)"; }
    QString description() const override { return "您的协议描述"; }
    
    // 帧结构定义 - 必须实现
    QList<ProtocolField> frameFields() const override;
    QList<ProtocolField> parseFields() const override;
    
    // 数据处理 - 必须实现
    QString buildFrame(const QStringList &values) const override;
    QStringList parseFrame(const QString &frame) const override;
    QString calculateChecksum(const QString &data) const override;
    
    // 验证 - 必须实现
    bool validateFrame(const QString &frame) const override;
    QString getFieldDescription(const QString &fieldName, const QString &value) const override;
};

#endif
```

### 2. 创建协议实现文件 `protocol_yourname.cpp`

```cpp
#include "protocol_yourname.h"

// 🎯 关键：这一行自动注册您的协议！
REGISTER_PROTOCOL(ProtocolYourName, "YourProtocol");

QList<ProtocolField> ProtocolYourName::frameFields() const
{
    return {
        {"字段1名称", "(描述)", "默认值", 最大长度, 是否可编辑, {"选项1", "选项2"}},
        {"字段2名称", "(描述)", "默认值", 最大长度, 是否可编辑, {"选项1", "选项2"}},
        // ... 最多6个字段
    };
}

QList<ProtocolField> ProtocolYourName::parseFields() const
{
    return {
        {"解析字段1", "(描述)", "占位符", 最大长度, false, {}},
        {"解析字段2", "(描述)", "占位符", 最大长度, false, {}},
        // ... 最多5个字段
    };
}

QString ProtocolYourName::buildFrame(const QStringList &values) const
{
    // 您的帧构建逻辑
    return "构建的帧数据";
}

QStringList ProtocolYourName::parseFrame(const QString &frame) const
{
    // 您的帧解析逻辑
    return {"字段1值", "字段2值", "字段3值"};
}

QString ProtocolYourName::calculateChecksum(const QString &data) const
{
    // 您的校验和计算逻辑
    return "校验和";
}

bool ProtocolYourName::validateFrame(const QString &frame) const
{
    // 您的帧验证逻辑
    return frame.length() >= 8; // 例子
}

QString ProtocolYourName::getFieldDescription(const QString &fieldName, const QString &value) const
{
    // 返回字段值的描述
    return QString("值: %1").arg(value);
}
```

### 3. 更新 `tcpclient.pri` 文件

在文件中添加您的新文件：

```qmake
HEADERS += \
    $$PWD/tcpclient.h \
    $$PWD/protocolbase.h \
    $$PWD/protocol_adp1000.h \
    $$PWD/protocol_modbus.h \
    $$PWD/protocol_yourname.h      # 添加这行

SOURCES += \
    $$PWD/tcpclient.cpp \
    $$PWD/protocolbase.cpp \
    $$PWD/protocol_adp1000.cpp \
    $$PWD/protocol_modbus.cpp \
    $$PWD/protocol_yourname.cpp    # 添加这行
```

### 4. 完成！

重新编译项目，您的新协议就会自动出现在协议选择列表中！

## 📝 字段配置说明

### ProtocolField 结构说明

```cpp
struct ProtocolField {
    QString name;           // 字段显示名称 (如 "设备ID", "功能码")
    QString description;    // 描述信息 (如 "(1字节)", "(2字符)")
    QString placeholder;    // 占位符/默认值
    int maxLength;          // 最大长度 (-1表示不限制)
    bool isEditable;        // 是否可编辑 (解析字段通常为false)
    QStringList options;    // 下拉选项 (空列表表示自由输入)
};
```

### 示例配置

```cpp
// 下拉选择字段
{"设备ID", "(1字节)", "01", 2, true, {"01", "02", "03", "04"}}

// 自由输入字段
{"命令数据", "(N字符)", "输入数据", -1, true, {}}

// 只读显示字段
{"CRC校验", "(2字节)", "自动计算", 4, false, {}}
```

## 🎯 实际例子

参考现有的协议实现：
- `protocol_adp1000.cpp` - ADP1000协议
- `protocol_modbus.cpp` - ModBus-RTU协议

## 🔧 高级功能

### 自定义配置

```cpp
QJsonObject getConfig() const override {
    QJsonObject config;
    config["baudRate"] = 9600;
    config["dataBits"] = 8;
    return config;
}
```

### 复杂的字段描述

```cpp
QString getFieldDescription(const QString &fieldName, const QString &value) const override
{
    if (fieldName == "功能码") {
        static QMap<QString, QString> codes = {
            {"01", "读线圈状态"},
            {"03", "读保持寄存器"}
        };
        return codes.value(value, "未知功能码");
    }
    return ProtocolBase::getFieldDescription(fieldName, value);
}
```

就这么简单！您的新协议将自动集成到整个系统中，包括：
- ✅ 自动出现在协议选择列表
- ✅ 动态更新UI标签和字段
- ✅ 自动支持帧构建和解析
- ✅ 自动支持数据验证和描述

## 🎉 无需修改主程序代码！

协议注册是完全自动的，您只需要：
1. 创建协议类
2. 使用 `REGISTER_PROTOCOL` 宏
3. 添加到 `.pri` 文件
4. 重新编译

新协议就会自动可用！
