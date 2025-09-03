#include "tcpframedclient.h"
#include <QtEndian>
#include <QHostAddress>
#include <QDataStream>
#include <QDebug>
#include <QRegularExpression>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <limits>

namespace {
static QVector<double> extractNextNumbers(const QString &text, int startIndex, int maxCount)
{
    QVector<double> numbers;
    if (startIndex < 0 || startIndex >= text.size() || maxCount <= 0) return numbers;
    QRegularExpression numRe(QStringLiteral("[+-]?(?:\\d+\\.?\\d*|\\.\\d+)"));
    auto it = numRe.globalMatch(text.mid(startIndex));
    while (it.hasNext() && numbers.size() < maxCount) {
        auto m = it.next();
        bool ok = false;
        double v = m.captured(0).toDouble(&ok);
        if (ok) numbers.push_back(v);
    }
    return numbers;
}

static bool parseNameAndQty(const QByteArray &body, QString &name, int &qty)
{
    name.clear();
    qty = -1;
    QJsonParseError perr;
    QJsonDocument jd = QJsonDocument::fromJson(body, &perr);
    if (perr.error == QJsonParseError::NoError && jd.isObject()) {
        QJsonObject obj = jd.object();
        QString n;
        if (obj.contains("name")) n = obj.value("name").toString();
        const char *qtyKeys[] = {"qty", "quantity", "count", "remain"};
        for (const char *k : qtyKeys) {
            if (obj.contains(k) && obj.value(k).isDouble()) {
                name = n;
                qty = obj.value(k).toInt();
                if (!name.isEmpty() && qty >= 0) return true;
            }
        }
        const auto keys = obj.keys();
        if (keys.size() == 1) {
            const QString &k = keys.first();
            if (obj.value(k).isDouble()) { name = k; qty = obj.value(k).toInt(); return true; }
        }
    }
    QString text = QString::fromUtf8(body).trimmed();
    QRegularExpression kvRe(QStringLiteral("^\\s*([^:＝=\n\r\t]+)\\s*[:＝=]\\s*(\\d+)\\s*$"));
    auto m = kvRe.match(text);
    if (m.hasMatch()) { name = m.captured(1).trimmed(); qty = m.captured(2).toInt(); return true; }
    QRegularExpression wordRe(QStringLiteral("([A-Za-z0-9_-]+)"));
    QRegularExpression numRe(QStringLiteral("(\\d+)"));
    auto mw = wordRe.match(text);
    auto mn = numRe.match(text, mw.hasMatch() ? mw.capturedEnd() : 0);
    if (mw.hasMatch() && mn.hasMatch()) { name = mw.captured(1); qty = mn.captured(1).toInt(); return true; }
    return false;
}
}

// CRC-16-CCITT查找表 (多项式: 0x1021, 初始值: 0xFFFF)
const quint16 TcpFramedClient::crc16Table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

TcpFramedClient::TcpFramedClient(QObject *parent)
    : QObject(parent)
{
    socket = new QTcpSocket(this);
    connect(socket, &QTcpSocket::readyRead, this, &TcpFramedClient::onReadyRead);
    connect(socket, &QTcpSocket::connected, this, &TcpFramedClient::onConnected);
    connect(socket, &QTcpSocket::disconnected, this, &TcpFramedClient::onDisconnected);
    connect(socket, qOverload<QAbstractSocket::SocketError>(&QTcpSocket::errorOccurred),
            this, &TcpFramedClient::onError);
}

void TcpFramedClient::start(const QString &host, quint16 port)
{
    if (running) return;
    buffer.clear();
    socket->abort();
    socket->connectToHost(host, port);
    running = true;
    emit logMessage(QString("Connecting to %1:%2 ...").arg(host).arg(port));
}

void TcpFramedClient::stop()
{
    running = false;
    socket->disconnectFromHost();
    emit logMessage("Stopped.");
}

void TcpFramedClient::onConnected()
{
    emit logMessage("TCP connected.");
}

void TcpFramedClient::onDisconnected()
{
    emit logMessage("TCP disconnected.");
}

void TcpFramedClient::onError(QAbstractSocket::SocketError)
{
    emit errorOccurred(socket->errorString());
}

void TcpFramedClient::onReadyRead()
{
#if 0
    // 读取全部数据进入累积缓冲
    QByteArray newData = socket->readAll();
    qDebug() << "[调试] onReadyRead被调用，接收到" << newData.size() << "字节";
    qDebug() << "[调试] 原始数据:" << newData.toHex();
    
    buffer.append(newData);
    qDebug() << "[调试] 追加后缓冲区总大小:" << buffer.size();
    
    parseBuffer();
#else
    /*
     * 这里写一个新的数据解析，比如他返回的是一个数据：一个网址链接；x坐标；y坐标；
     *          比如他返回的是两个数据：一个网址链接，第二个网址链接；x坐标，第二个x坐标；y坐标，第二个y坐标；
     *          比如他返回的是三个数据：一个网址链接，第二个网址链接，第三个网址链接；x坐标，第二个x坐标，第三个x坐标；y坐标，第二个y坐标，第三个y坐标；
     *          以此类推。。。
     * 注意：比如给的网址是@http://192.168.10.132:5555/api/codes/tbp ，你需要点击去看才知道里面的内容
     * 解析完数据后装在一起，方便我查看。你可以用容器可以用类或者用其他更好的办法。存的内容做细分说明，网址会返回药品名字和余量，比如：ACN:9863。存一个元素的时候存的是链接的药品名字，药品的余量，药品的x坐标，药品的y坐标
     * 解析完内容打印出来给我看就行，暂时没有下一步动作
     */
    QByteArray raw = socket->readAll();
    if (raw.isEmpty()) return;
    const QString text = QString::fromUtf8(raw);
    qDebug() << "[解析] 接收到文本长度=" << text.size();
    qDebug() << "[解析] 原文片段:" << text.left(200);

    // 优先解析形如： URL1,URL2; x1,x2; y1,y2; 的分组格式
    QString normalized = text;
    QStringList groups = normalized.split(QRegularExpression("\\s*[;；]+\\s*"), Qt::SkipEmptyParts);

    struct Item { QString url; double x; double y; };
    QVector<Item> items;

    if (groups.size() >= 3) {
        // 第1组：一个或多个URL，以英文/中文逗号分隔；允许首个URL带'@'
        QStringList urlTokens = groups[0].split(QRegularExpression("\\s*[,，]+\\s*"), Qt::SkipEmptyParts);
        // 第2/3组：x/y 列表（解析为 double，支持小数）
        QVector<double> xs = extractNextNumbers(groups[1], 0, groups[1].size());
        QVector<double> ys = extractNextNumbers(groups[2], 0, groups[2].size());

        const int n = urlTokens.size();
        items.reserve(n);
        for (int i = 0; i < n; ++i) {
            QString u = urlTokens[i].trimmed();
            if (u.startsWith('@')) u.remove(0, 1);
            const double x = i < xs.size() ? xs[i] : std::numeric_limits<double>::quiet_NaN();
            const double y = i < ys.size() ? ys[i] : std::numeric_limits<double>::quiet_NaN();
            items.push_back({u, x, y});
        }
    } else {
        // 回退：逐个URL匹配，但坐标从其后的第一个分号开始抽取，避免把URL里的数字当坐标
        QRegularExpression urlRe(QStringLiteral("(@?https?://[^\\s;,，；、]+)"), QRegularExpression::CaseInsensitiveOption);
        auto urlIt = urlRe.globalMatch(text);
        while (urlIt.hasNext()) {
            auto m = urlIt.next();
            QString u = m.captured(1);
            if (u.startsWith('@')) u.remove(0, 1);
            int searchFrom = text.indexOf(QRegularExpression("[;；]"), m.capturedEnd(1));
            if (searchFrom < 0) searchFrom = m.capturedEnd(1);
            QVector<double> nums = extractNextNumbers(text, searchFrom, 2);
            const double x = nums.size() > 0 ? nums[0] : std::numeric_limits<double>::quiet_NaN();
            const double y = nums.size() > 1 ? nums[1] : std::numeric_limits<double>::quiet_NaN();
            items.push_back({u, x, y});
        }
    }

    static QNetworkAccessManager *mgr = nullptr;
    if (!mgr) mgr = new QNetworkAccessManager(this);

    for (int index = 0; index < items.size(); ++index) {
        const auto &it = items[index];
        qDebug() << "[解析] 第" << (index + 1) << "项 URL=" << it.url << ", x=" << it.x << ", y=" << it.y;
        QNetworkRequest req{ QUrl(it.url) };
        QNetworkReply *reply = mgr->get(req);
        reply->setProperty("idx", index);
        reply->setProperty("url", it.url);
        reply->setProperty("x", it.x);
        reply->setProperty("y", it.y);
        connect(reply, &QNetworkReply::finished, this, [reply]() {
            const int idx = reply->property("idx").toInt();
            const QString url = reply->property("url").toString();
            const double x = reply->property("x").toDouble();
            const double y = reply->property("y").toDouble();
            if (reply->error() != QNetworkReply::NoError) {
                qWarning() << "[解析] 第" << (idx + 1) << "项 请求失败:" << reply->errorString() << ", url=" << url;
                reply->deleteLater();
                return;
            }
            QByteArray body = reply->readAll();
            QString name; int qty = -1;
            bool ok = parseNameAndQty(body, name, qty);
            if (!ok) {
                qWarning() << "[解析] 第" << (idx + 1) << "项 返回体无法解析为(药名,余量)，url=" << url
                           << "，返回体片段=" << QString::fromUtf8(body).left(120);
            } else {
                qDebug() << "[结果] #" << (idx + 1) << ": 药名=" << name
                         << ", 余量=" << qty << ", x=" << x << ", y=" << y
                         << ", url=" << url;
            }
            reply->deleteLater();
        });
    }
#endif
}

void TcpFramedClient::parseBuffer()
{
    // 帧格式： magic(2) | length(4) | type(2) | payload(n) | crc(2)
    // length = sizeof(type)+sizeof(payload)+sizeof(crc) = 2 + n + 2
    // Example：AA 55 00 00 00 0F 00 01 48 65 6C 6C 6F 20 57 6F 72 6C 64 f4 45
    const int headerSize = 2 + 4 + 2; // magic + length + type
    
    qDebug() << "[调试] parseBuffer被调用，缓冲区大小:" << buffer.size();
    qDebug() << "[调试] 缓冲区内容:" << buffer.toHex();
    
    while (true) {
        if (buffer.size() < headerSize) {
            qDebug() << "[调试] 缓冲区太小，需要至少" << headerSize << "字节，当前有" << buffer.size() << "字节";
            return; // 等更多数据
        }

        // 寻找magic（允许丢弃前导噪声）
        bool magicFound = false;
        int startIndex = 0;
        for (int i = 0; i + 1 < buffer.size(); ++i) {
            quint16 m;
            if (useBigEndian) {
                m = qFromBigEndian<quint16>(reinterpret_cast<const uchar*>(buffer.constData() + i));
            } else {
                m = qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(buffer.constData() + i));
            }
            if (m == MAGIC) { 
                magicFound = true; 
                startIndex = i; 
                qDebug() << "[调试] 找到Magic，位置:" << i << "值:" << QString("0x%1").arg(m, 4, 16, QChar('0')) 
                        << (useBigEndian ? "(大端序)" : "(小端序)");
                break; 
            }
        }
        if (!magicFound) { 
            qDebug() << "[调试] 未找到Magic，清空缓冲区";
            buffer.clear(); 
            return; 
        }
        if (startIndex > 0) {
            qDebug() << "[调试] 移除Magic前的" << startIndex << "字节";
            buffer.remove(0, startIndex); // 丢弃magic前的数据
        }

        if (buffer.size() < headerSize) {
            qDebug() << "[调试] Magic后缓冲区太小，需要" << headerSize << "字节，当前有" << buffer.size() << "字节";
            return; // 不够头部
        }

        const uchar *data = reinterpret_cast<const uchar*>(buffer.constData());
        // magic 已匹配
        quint32 length;
        quint16 type;
        if (useBigEndian) {
            length = qFromBigEndian<quint32>(data + 2);
            type = qFromBigEndian<quint16>(data + 6);
        } else {
            length = qFromLittleEndian<quint32>(data + 2);
            type = qFromLittleEndian<quint16>(data + 6);
        }
        
        qDebug() << "[调试] 解析长度:" << length << "类型:" << type;

        // 总帧长度 = 2(magic)+4(length)+ length
        quint32 total = 2 + 4 + length;
        if (length < 4) { // 至少要包含 type(2)+crc(2)
            qDebug() << "[调试] 无效长度" << length << "< 4，移除Magic";
            // 异常，丢掉magic
            buffer.remove(0, 2); 
            continue;
        }
        if (buffer.size() < static_cast<int>(total)) {
            qDebug() << "[调试] 等待完整帧，需要" << total << "字节，当前有" << buffer.size() << "字节";
            return; // 等整帧
        }

        // 提取payload与crc
        int payloadSize = static_cast<int>(length) - 2 /*type*/ - 2 /*crc*/;
        if (payloadSize < 0) { 
            qDebug() << "[调试] 无效载荷大小:" << payloadSize;
            buffer.remove(0, 2); 
            continue; 
        }
        QByteArray payload = buffer.mid(8, payloadSize);
        quint16 crc;
        if (useBigEndian) {
            crc = qFromBigEndian<quint16>(data + 8 + payloadSize);
        } else {
            crc = qFromLittleEndian<quint16>(data + 8 + payloadSize);
        }
        
        qDebug() << "[调试] 载荷大小:" << payloadSize << "CRC:" << QString("0x%1").arg(crc, 4, 16, QChar('0'));
        qDebug() << "[调试] 载荷内容:" << payload.toHex();

        // 校验
        quint16 calc = calcCrc(type, payload);
        qDebug() << "[调试] 计算CRC:" << QString("0x%1").arg(calc, 4, 16, QChar('0')) << "接收CRC:" << QString("0x%1").arg(crc, 4, 16, QChar('0'));
        
        if (calc == crc) {
            qDebug() << "[调试] CRC匹配！发出frameReceived信号";
            emit frameReceived(type, payload);
        } else {
            qDebug() << "[调试] CRC不匹配！期望:" << QString("0x%1").arg(calc, 4, 16, QChar('0')) << "实际:" << QString("0x%1").arg(crc, 4, 16, QChar('0'));
            emit errorOccurred(QString("CRC不匹配: 计算值=%1, 接收值=%2").arg(calc).arg(crc));
        }

        // 移除已消费的整帧
        qDebug() << "[调试] 从缓冲区移除" << total << "字节";
        buffer.remove(0, static_cast<int>(total));
        qDebug() << "[调试] 移除后缓冲区大小:" << buffer.size();
    }
}

quint16 TcpFramedClient::calcCrc(quint16 type, const QByteArray &payload) const
{
    // 标准CRC-16-CCITT算法 (多项式: 0x1021, 初始值: 0xFFFF)
    quint16 crc = 0xFFFF;
    
    // 计算type字段的CRC
    crc = (crc << 8) ^ crc16Table[(crc >> 8) ^ (type & 0xFF)];
    crc = (crc << 8) ^ crc16Table[(crc >> 8) ^ ((type >> 8) & 0xFF)];
    
    // 计算payload的CRC
    const uchar *p = reinterpret_cast<const uchar*>(payload.constData());
    for (int i = 0; i < payload.size(); ++i) {
        crc = (crc << 8) ^ crc16Table[(crc >> 8) ^ p[i]];
    }
    
    return crc;
}

