#ifndef PROTOCOL_CSTEP_H
#define PROTOCOL_CSTEP_H

#include "protocolbase.h"

/**
 * @brief CStep协议实现 - 步进电机控制器
 */
class ProtocolCStep : public ProtocolBase
{
public:
    // 基本信息
    QString name() const override { return "CStep"; }
    QString displayName() const override { return "CStep (XYZ电机)"; }
    QString description() const override { return "CStep 步进电机控制器"; }
    
    // 帧结构定义
    QList<ProtocolField> frameFields() const override;
    QList<ProtocolField> parseFields() const override;
    
    // 数据处理
    QString buildFrame(const QStringList &values) const override;
    QStringList parseFrame(const QString &frame) const override;
    QString calculateChecksum(const QString &data) const override;
    
    // 验证
    bool validateFrame(const QString &frame) const override;
    QString getFieldDescription(const QString &fieldName, const QString &value) const override;

private:
    QStringList getAvailableCommands() const;
    QString getCommandDescription(const QString &command) const;
};

#endif // PROTOCOL_CSTEP_H
