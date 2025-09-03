#ifndef TCPFRAMEDCLIENT_H
#define TCPFRAMEDCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QByteArray>

/**
 * TcpFramedClient
 *
 * 功能：
 * - 连接指定TCP地址并持续读取数据
 * - 采用循环缓冲解析固定头部的帧格式：
 *   magic(2) | length(4, 小端) | type(2) | payload(n) | crc(2)
 * - 支持粘包/半包处理，保证数据不丢不乱
 * - 提供start/stop控制
 */
class TcpFramedClient : public QObject {
	Q_OBJECT
public:
	explicit TcpFramedClient(QObject *parent = nullptr);

	// 启动/停止
	void start(const QString &host, quint16 port);
	void stop();
	bool isRunning() const { return running; }

signals:
	// 解析成功一帧
	void frameReceived(quint16 type, QByteArray payload);
	// 错误/日志
	void errorOccurred(const QString &msg);
	void logMessage(const QString &msg);

private slots:
	void onReadyRead();
	void onConnected();
	void onDisconnected();
	void onError(QAbstractSocket::SocketError socketError);

public:
	// 字节序配置：true=大端序，false=小端序（默认）
	void setByteOrder(bool bigEndian) { useBigEndian = bigEndian; }
	bool getByteOrder() const { return useBigEndian; }

private:
	QTcpSocket *socket = nullptr;
	QByteArray buffer; // 累积缓冲
	bool running = false;
    bool useBigEndian = true; // 默认使用小端序

	static constexpr quint16 MAGIC = 0xAA55;
	
	// CRC-16-CCITT查找表
	static const quint16 crc16Table[256];

	// 解析函数：从buffer中尽可能提取完整帧
	void parseBuffer();

	// 校验：标准CRC-16-CCITT算法
	quint16 calcCrc(quint16 type, const QByteArray &payload) const;
};

#endif // TCPFRAMEDCLIENT_H


