#ifndef PROTOCOLBASE_H
#define PROTOCOLBASE_H

#include <QString>
#include <QStringList>
#include <QMap>
#include <QJsonObject>
#include <QtCore/QCoreApplication>

/**
 * @brief 协议字段定义
 */
struct ProtocolField {
    QString name;           // 字段名称 (如 "设备ID", "功能码")
    QString description;    // 描述信息 (如 "(1字节)", "(2字符)")
    QString placeholder;    // 占位符文本
    int maxLength;          // 最大长度 (-1表示不限制)
    bool isEditable;        // 是否可编辑
    QStringList options;    // 可选项列表 (空表示自由输入)
};

/**
 * @brief 协议基类 - 所有协议都需要继承此类
 */
class ProtocolBase
{
public:
    virtual ~ProtocolBase() = default;
    
    // 基本信息
    virtual QString name() const = 0;                    // 协议名称
    virtual QString displayName() const = 0;             // 显示名称
    virtual QString description() const = 0;             // 协议描述
    virtual QString version() const { return "1.0"; }   // 协议版本
    
    // 帧结构定义
    virtual QList<ProtocolField> frameFields() const = 0;    // 帧结构字段
    virtual QList<ProtocolField> parseFields() const = 0;    // 解析字段
    
    // 数据处理
    virtual QString buildFrame(const QStringList &values) const = 0;     // 构建帧
    virtual QStringList parseFrame(const QString &frame) const = 0;      // 解析帧
    virtual QString calculateChecksum(const QString &data) const = 0;     // 计算校验和
    
    // 验证
    virtual bool validateFrame(const QString &frame) const = 0;           // 验证帧格式
    virtual QString getFieldDescription(const QString &fieldName, const QString &value) const = 0; // 获取字段描述
    
    // 配置
    virtual QJsonObject getConfig() const { return QJsonObject(); }      // 获取配置
    virtual void setConfig(const QJsonObject &config) { Q_UNUSED(config); } // 设置配置
};

/**
 * @brief 协议工厂 - 自动注册和管理所有协议
 */
class ProtocolFactory
{
public:
    static ProtocolFactory& instance();
    
    // 注册协议
    void registerProtocol(const QString &name, std::function<ProtocolBase*()> creator);
    
    // 创建协议实例
    ProtocolBase* createProtocol(const QString &name);
    
    // 获取所有协议名称
    QStringList availableProtocols() const;
    
    // 获取协议显示名称
    QString getDisplayName(const QString &name) const;

private:
    ProtocolFactory() = default;
    QMap<QString, std::function<ProtocolBase*()>> m_creators;
    QMap<QString, ProtocolBase*> m_prototypes; // 用于获取显示名称
};

/**
 * @brief 协议注册器 - 自动注册协议的辅助类
 */
template<typename T>
class ProtocolRegistrar
{
public:
    ProtocolRegistrar(const QString &name) {
        ProtocolFactory::instance().registerProtocol(name, []() { return new T(); });
    }
};

// 注册协议的宏（使用 Q_COREAPP_STARTUP_FUNCTION，避免全局非POD静态对象）
#define REGISTER_PROTOCOL(ClassName, ProtocolName)                                   \
    namespace {                                                                      \
    static void register_##ClassName() {                                             \
        ProtocolFactory::instance().registerProtocol(                                \
            ProtocolName, []() { return new ClassName(); });                         \
    }                                                                                \
    Q_COREAPP_STARTUP_FUNCTION(register_##ClassName)                                 \
    }

//  static ProtocolRegistrar<协议类名> g_协议类名_registrar(给类起的外号)

#endif // PROTOCOLBASE_H
