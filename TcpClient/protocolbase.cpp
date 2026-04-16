#include "protocolbase.h"
#include <QDebug>
#include <functional>

ProtocolFactory& ProtocolFactory::instance()
{
    static ProtocolFactory instance;
    return instance;
}

void ProtocolFactory::registerProtocol(const QString &name, std::function<ProtocolBase*()> creator)
{
    m_creators[name] = creator;
    
    // 创建原型实例用于获取显示名称
    if (!m_prototypes.contains(name)) {
        m_prototypes[name] = creator();
    }
    
    qDebug() << "协议已注册:" << name;    
}

ProtocolBase* ProtocolFactory::createProtocol(const QString &name)
{
    if (m_creators.contains(name)) {
        return m_creators[name]();
    }
    
    qWarning() << "未找到协议:" << name;
    return nullptr;
}

QStringList ProtocolFactory::availableProtocols() const
{
    return m_creators.keys();
}

QString ProtocolFactory::getDisplayName(const QString &name) const
{
    if (m_prototypes.contains(name)) {
        return m_prototypes[name]->displayName();
    }
    return name;
}
