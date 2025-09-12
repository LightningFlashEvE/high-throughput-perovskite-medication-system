#ifndef PROTOCOL_ADP1000_H
#define PROTOCOL_ADP1000_H

#include "protocolbase.h"

/**
 * @brief ADP1000协议实现
 */
class ProtocolADP1000 : public ProtocolBase
{
public:
    // 基本信息
    QString name() const override { return "ADP1000"; }
    QString displayName() const override { return "ADP1000 (空气泵)"; }
    QString description() const override { return "ADP1000 高通量液体处理系统"; }
    
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

#endif // PROTOCOL_ADP1000_H
