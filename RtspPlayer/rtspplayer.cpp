#include "rtspplayer.h"
#include "ui_rtspplayer.h"
#include <QDebug>
#include <QUrl>
#include <QMessageBox>
#include <QTimer>
#include <QWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QTextEdit>
#include <QVideoWidget>
#include <QMediaPlayer>
#include <QMediaMetaData>
#include <QNetworkAccessManager>
#include <QKeyEvent>
#include <QEvent>
#include <QCloseEvent>
#include <QSize>
#include <QVariant>

RtspPlayer::RtspPlayer(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::RtspPlayer)
    , m_mediaPlayer(nullptr)
    , m_videoWidget(nullptr)
    , m_networkManager(nullptr)
    , m_infoUpdateTimer(nullptr)
    , m_reconnectTimer(nullptr)
    , m_connectionTimeoutTimer(nullptr)
    , m_testTimeoutTimer(nullptr)
    , m_isConnected(false)
    , m_isReconnecting(false)
    , m_manualDisconnect(false)
    , m_frameWidth(0)
    , m_frameHeight(0)
    , m_frameRate(0.0)
    , m_bitRate(0)
    , m_currentTestIndex(0)
    , m_isTesting(false)
{
    ui->setupUi(this);

    // 媒体组件
    m_mediaPlayer = new QMediaPlayer(this);
    m_videoWidget = ui->videoWidget;
    m_mediaPlayer->setVideoOutput(m_videoWidget);

    // 事件过滤器（双击全屏）
    m_videoWidget->setAttribute(Qt::WA_AcceptTouchEvents, false);
    m_videoWidget->setMouseTracking(true);
    m_videoWidget->installEventFilter(this);

    // 网络
    m_networkManager = new QNetworkAccessManager(this);

    // 定时器
    m_infoUpdateTimer = new QTimer(this);
    m_infoUpdateTimer->setInterval(1000);

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    m_reconnectTimer->setInterval(3000);

    m_connectionTimeoutTimer = new QTimer(this);
    m_connectionTimeoutTimer->setSingleShot(true);
    m_connectionTimeoutTimer->setInterval(10000);

    m_testTimeoutTimer = new QTimer(this);
    m_testTimeoutTimer->setSingleShot(true);
    m_testTimeoutTimer->setInterval(8000);

    // 连接信号
    setupConnections();

    // 预置测试地址
    m_testUrls = {
        "rtsp://admin:123456@192.168.5.36:554/stream",
        "rtsp://admin:123456@192.168.5.36:554/Streaming/Channels/101",
        "rtsp://admin:123456@192.168.5.36:554/cam/realmonitor?channel=1&subtype=0",
        "rtsp://admin:123456@192.168.5.36:554/h265/ch1/main/av_stream",
        "rtsp://admin:123456@192.168.5.36:554/onvif1",
        "rtsp://admin:123456@192.168.5.36:554/user=admin&password=123456&channel=1&stream=0.sdp?real_stream"
    };

    updateUI();
    resetStreamInfo();
}

RtspPlayer::~RtspPlayer()
{
    qDebug() << "RtspPlayer 析构，开始清理资源...";

    // 停止所有定时器
    if (m_infoUpdateTimer) {
        m_infoUpdateTimer->stop();
    }
    if (m_reconnectTimer) {
        m_reconnectTimer->stop();
    }
    if (m_connectionTimeoutTimer) {
        m_connectionTimeoutTimer->stop();
    }
    if (m_testTimeoutTimer) {
        m_testTimeoutTimer->stop();
    }

    // 停止媒体播放器并清理
    if (m_mediaPlayer) {
        // 断开信号，避免析构过程中触发回调
        m_mediaPlayer->disconnect();

        // 先解绑，避免 stop() 时触发多余解码和状态变化
        m_mediaPlayer->setVideoOutput(nullptr);

        // 停止播放并清空源
        m_mediaPlayer->stop();
        m_mediaPlayer->setSource(QUrl());

        // 安排删除，避免残留线程或解码器对象
        m_mediaPlayer->deleteLater();
        m_mediaPlayer = nullptr;
    }

    // 清理网络管理器
    if (m_networkManager) {
        m_networkManager->deleteLater();
        m_networkManager = nullptr;
    }

    // 最后销毁 UI
    delete ui;

    qDebug() << "RtspPlayer 析构完成，资源已释放";
}

// RtspPlayer::~RtspPlayer()
// {
//     // 停止所有定时器
//     if (m_infoUpdateTimer) {
//         m_infoUpdateTimer->stop();
//     }
//     if (m_reconnectTimer) {
//         m_reconnectTimer->stop();
//     }
//     if (m_connectionTimeoutTimer) {
//         m_connectionTimeoutTimer->stop();
//     }
//     if (m_testTimeoutTimer) {
//         m_testTimeoutTimer->stop();
//     }
    
//     // 停止媒体播放器并清理资源
//     if (m_mediaPlayer) {
//         // 断开所有信号连接，避免析构时触发信号
//         m_mediaPlayer->disconnect();
        
//         // 停止播放
//         m_mediaPlayer->stop();
        
//         // 清除媒体源
//         m_mediaPlayer->setSource(QUrl());
        
//         // 删除媒体播放器
//         m_mediaPlayer->deleteLater();
//         m_mediaPlayer = nullptr;
//     }

//     // 清理网络管理器
//     if (m_networkManager) {
//         m_networkManager->deleteLater();
//         m_networkManager = nullptr;
//     }
    
//     delete ui;
// }


bool RtspPlayer::isConnected() const
{
    return m_isConnected;
}

void RtspPlayer::setupConnections()
{
    // UI按钮
    connect(ui->connectButton, &QPushButton::clicked, this, &RtspPlayer::onConnectClicked);
    connect(ui->disconnectButton, &QPushButton::clicked, this, &RtspPlayer::onDisconnectClicked);
    connect(ui->fullscreenButton, &QPushButton::clicked, this, &RtspPlayer::onFullscreenClicked);
    connect(ui->testAllButton, &QPushButton::clicked, this, &RtspPlayer::onTestAllClicked);
    connect(ui->diagnoseButton, &QPushButton::clicked, this, &RtspPlayer::onDiagnoseClicked);

    // 播放器
    connect(m_mediaPlayer, &QMediaPlayer::mediaStatusChanged, this, &RtspPlayer::onMediaStatusChanged);
    connect(m_mediaPlayer, &QMediaPlayer::playbackStateChanged, this, &RtspPlayer::onPlaybackStateChanged);
    connect(m_mediaPlayer, &QMediaPlayer::errorOccurred, this, &RtspPlayer::onErrorOccurred);
    connect(m_mediaPlayer, &QMediaPlayer::positionChanged, this, &RtspPlayer::onPositionChanged);
    connect(m_mediaPlayer, &QMediaPlayer::durationChanged, this, &RtspPlayer::onDurationChanged);
    connect(m_mediaPlayer, &QMediaPlayer::metaDataChanged, this, &RtspPlayer::onMetaDataChanged);

    // 定时器
    connect(m_infoUpdateTimer, &QTimer::timeout, this, &RtspPlayer::updateStreamInfo);
    connect(m_reconnectTimer, &QTimer::timeout, this, &RtspPlayer::attemptReconnect);
    connect(m_connectionTimeoutTimer, &QTimer::timeout, this, &RtspPlayer::onConnectionTimeout);
    connect(m_testTimeoutTimer, &QTimer::timeout, this, &RtspPlayer::testNextUrl);
}

void RtspPlayer::onConnectClicked()
{
    QString url = ui->urlComboBox->currentText().trimmed();
    if (url.isEmpty()) {
        showErrorMessage("请输入RTSP地址");
        return;
    }

    m_currentUrl = url;
    m_isReconnecting = false;
    m_manualDisconnect = false;

    m_mediaPlayer->setSource(QUrl(url));
    m_mediaPlayer->play();

    m_connectionTimeoutTimer->start();
    m_infoUpdateTimer->start();
    updateUI();

    ui->statusLabel->setText("状态: 连接中...");
    ui->statusLabel->setStyleSheet("color: orange;");
    ui->disconnectHintLabel->setText("");

    qDebug() << "尝试连接到RTSP流:" << url;
}

void RtspPlayer::onDisconnectClicked()
{
    m_manualDisconnect = true;
    m_isReconnecting = false;

    m_reconnectTimer->stop();
    m_infoUpdateTimer->stop();
    m_connectionTimeoutTimer->stop();
    m_testTimeoutTimer->stop();

    m_mediaPlayer->stop();
    m_mediaPlayer->setSource(QUrl());

    m_isConnected = false;
    updateUI();
    resetStreamInfo();

    ui->statusLabel->setText("状态: 已手动断开");
    ui->statusLabel->setStyleSheet("color: red;");
    ui->disconnectHintLabel->setText("提示: 点击连接按钮可重新连接");

    qDebug() << "手动断开RTSP连接";
}

void RtspPlayer::onFullscreenClicked()
{
    if (m_videoWidget) {
        if (m_videoWidget->isFullScreen()) {
            m_videoWidget->setFullScreen(false);
            ui->fullscreenButton->setText("📺 全屏");
            qDebug() << "退出全屏模式";
        } else {
            m_videoWidget->setFullScreen(true);
            ui->fullscreenButton->setText("📺 退出全屏");
            qDebug() << "进入全屏模式";
        }
    }
}

void RtspPlayer::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    qDebug() << "媒体状态改变:" << status;

    switch (status) {
    case QMediaPlayer::LoadingMedia:
        ui->statusLabel->setText("状态: 加载中...");
        ui->statusLabel->setStyleSheet("color: orange;");
        break;
    case QMediaPlayer::LoadedMedia:
        ui->statusLabel->setText("状态: 已加载");
        ui->statusLabel->setStyleSheet("color: blue;");
        break;
    case QMediaPlayer::BufferingMedia:
        ui->statusLabel->setText("状态: 缓冲中...");
        ui->statusLabel->setStyleSheet("color: orange;");
        break;
    case QMediaPlayer::BufferedMedia:
        ui->statusLabel->setText("状态: 已缓冲");
        ui->statusLabel->setStyleSheet("color: blue;");
        QTimer::singleShot(500, this, &RtspPlayer::updateStreamInfo);
        break;
    case QMediaPlayer::EndOfMedia:
        ui->statusLabel->setText("状态: 播放结束");
        ui->statusLabel->setStyleSheet("color: gray;");
        m_connectionTimeoutTimer->stop();
        break;
    case QMediaPlayer::InvalidMedia:
        ui->statusLabel->setText("状态: 无效媒体");
        ui->statusLabel->setStyleSheet("color: red;");
        m_connectionTimeoutTimer->stop();
        showErrorMessage("无效的RTSP流地址");
        break;
    default:
        break;
    }
}

void RtspPlayer::onPlaybackStateChanged(QMediaPlayer::PlaybackState state)
{
    qDebug() << "播放状态改变:" << state;

    switch (state) {
    case QMediaPlayer::PlayingState:
        m_isConnected = true;
        updateUI();
        ui->statusLabel->setText("状态: 正在播放");
        ui->statusLabel->setStyleSheet("color: green;");
        m_isReconnecting = false;
        m_reconnectTimer->stop();
        m_connectionTimeoutTimer->stop();
        ui->disconnectHintLabel->setText("");
        QTimer::singleShot(1000, this, &RtspPlayer::updateStreamInfo);
        if (m_isTesting) {
            m_testTimeoutTimer->stop();
            logTestResult(m_currentUrl, true);
        }
        break;
    case QMediaPlayer::PausedState:
        ui->statusLabel->setText("状态: 已暂停");
        ui->statusLabel->setStyleSheet("color: orange;");
        break;
    case QMediaPlayer::StoppedState:
        m_isConnected = false;
        updateUI();
        if (!m_manualDisconnect && !m_isReconnecting && ui->autoReconnectCheckBox->isChecked()) {
            m_isReconnecting = true;
            m_reconnectTimer->start();
            ui->statusLabel->setText("状态: 准备重连...");
            ui->statusLabel->setStyleSheet("color: orange;");
        } else if (m_manualDisconnect) {
            ui->statusLabel->setText("状态: 已手动断开");
            ui->statusLabel->setStyleSheet("color: red;");
        } else {
            ui->statusLabel->setText("状态: 已停止");
            ui->statusLabel->setStyleSheet("color: red;");
        }
        break;
    default:
        break;
    }
}

void RtspPlayer::onErrorOccurred(QMediaPlayer::Error error, const QString &errorString)
{
    qDebug() << "媒体播放错误:" << error << errorString;

    QString errorMsg;
    QString statusText;

    switch (error) {
    case QMediaPlayer::NoError:
        return;
    case QMediaPlayer::ResourceError:
        errorMsg = "资源错误: 无法访问RTSP流";
        statusText = "状态: 资源错误";
        break;
    case QMediaPlayer::FormatError:
        errorMsg = "格式错误: 不支持的媒体格式\n\n建议:\n"
                  "1. 尝试不同的RTSP路径格式\n"
                  "2. 检查摄像头编码格式设置\n"
                  "3. 尝试H.264或H.265格式\n"
                  "4. 使用VLC等工具验证RTSP流";
        statusText = "状态: 格式错误";
        break;
    case QMediaPlayer::NetworkError:
        errorMsg = "网络错误: 连接失败";
        statusText = "状态: 网络连接失败";
        break;
    case QMediaPlayer::AccessDeniedError:
        errorMsg = "访问被拒绝: 请检查用户名和密码";
        statusText = "状态: 访问被拒绝";
        break;
    default:
        errorMsg = "未知错误: " + errorString;
        statusText = "状态: 连接失败";
        break;
    }

    ui->statusLabel->setText(statusText);
    ui->statusLabel->setStyleSheet("color: red;");

    if (m_isTesting) {
        m_testTimeoutTimer->stop();
        logTestResult(m_currentUrl, false, errorMsg);
    } else {
        showErrorMessage(errorMsg);
        m_isConnected = false;
        updateUI();
        if (!m_manualDisconnect && ui->autoReconnectCheckBox->isChecked() && !m_isReconnecting) {
            m_isReconnecting = true;
            m_reconnectTimer->start();
            ui->statusLabel->setText("状态: 准备重连...");
            ui->statusLabel->setStyleSheet("color: orange;");
        } else if (m_manualDisconnect) {
            ui->statusLabel->setText("状态: 已手动断开");
            ui->statusLabel->setStyleSheet("color: red;");
        }
    }
}

void RtspPlayer::onPositionChanged(qint64 position)
{
    Q_UNUSED(position)
}

void RtspPlayer::onDurationChanged(qint64 duration)
{
    Q_UNUSED(duration)
}

void RtspPlayer::onMetaDataChanged()
{
    qDebug() << "元数据已更新，重新获取流信息";
    updateStreamInfo();
}

void RtspPlayer::updateStreamInfo()
{
    if (!m_isConnected || !m_mediaPlayer) {
        return;
    }

    QMediaMetaData metaData = m_mediaPlayer->metaData();

    QList<QMediaMetaData::Key> availableKeys = metaData.keys();
    // qDebug() << "可用的元数据键数量:" << availableKeys.size();

    // qDebug() << "=== 元数据信息 ===";
    // for (const auto& key : availableKeys) {
    //     QVariant value = metaData.value(key);
    //     QString keyName;
    //     switch (key) {
    //     case QMediaMetaData::Title: keyName = "Title"; break;
    //     case QMediaMetaData::Author: keyName = "Author"; break;
    //     case QMediaMetaData::Comment: keyName = "Comment"; break;
    //     case QMediaMetaData::Description: keyName = "Description"; break;
    //     case QMediaMetaData::Genre: keyName = "Genre"; break;
    //     case QMediaMetaData::Date: keyName = "Date"; break;
    //     case QMediaMetaData::Language: keyName = "Language"; break;
    //     case QMediaMetaData::Publisher: keyName = "Publisher"; break;
    //     case QMediaMetaData::Copyright: keyName = "Copyright"; break;
    //     case QMediaMetaData::Url: keyName = "Url"; break;
    //     case QMediaMetaData::MediaType: keyName = "MediaType"; break;
    //     case QMediaMetaData::FileFormat: keyName = "FileFormat"; break;
    //     case QMediaMetaData::Duration: keyName = "Duration"; break;
    //     case QMediaMetaData::AudioBitRate: keyName = "AudioBitRate"; break;
    //     case QMediaMetaData::AudioCodec: keyName = "AudioCodec"; break;
    //     case QMediaMetaData::VideoFrameRate: keyName = "VideoFrameRate"; break;
    //     case QMediaMetaData::VideoBitRate: keyName = "VideoBitRate"; break;
    //     case QMediaMetaData::VideoCodec: keyName = "VideoCodec"; break;
    //     case QMediaMetaData::Resolution: keyName = "Resolution"; break;
    //     case QMediaMetaData::Orientation: keyName = "Orientation"; break;
    //     default: keyName = QString("Unknown(%1)").arg(static_cast<int>(key)); break;
    //     }
    //     qDebug() << keyName << ":" << value;
    // }
    // qDebug() << "=== 元数据信息结束 ===";

    if (metaData.value(QMediaMetaData::Resolution).isValid()) {
        QSize videoResolution = metaData.value(QMediaMetaData::Resolution).toSize();
        if (videoResolution.isValid()) {
            m_frameWidth = videoResolution.width();
            m_frameHeight = videoResolution.height();
            //qDebug() << "从元数据获取分辨率:" << m_frameWidth << "x" << m_frameHeight;
        }
    }

    QString codecText = "编码器: 未知";
    if (metaData.value(QMediaMetaData::VideoCodec).isValid()) {
        QString codec = metaData.value(QMediaMetaData::VideoCodec).toString();
        QString displayName = codec;
        if (codec.contains("H264", Qt::CaseInsensitive) || codec.contains("AVC", Qt::CaseInsensitive)) {
            displayName = "H.264";
        } else if (codec.contains("H265", Qt::CaseInsensitive) || codec.contains("HEVC", Qt::CaseInsensitive)) {
            displayName = "H.265/HEVC";
        } else if (codec.contains("MJPEG", Qt::CaseInsensitive)) {
            displayName = "MJPEG";
        }
        codecText = QString("编码器: %1").arg(displayName);
    } else if (metaData.value(QMediaMetaData::AudioCodec).isValid()) {
        QString audioCodec = metaData.value(QMediaMetaData::AudioCodec).toString();
        codecText = QString("编码器: %1 (音频)").arg(audioCodec);
    }

    if (metaData.value(QMediaMetaData::VideoFrameRate).isValid()) {
        m_frameRate = metaData.value(QMediaMetaData::VideoFrameRate).toReal();
    }

    if (metaData.value(QMediaMetaData::VideoBitRate).isValid()) {
        m_bitRate = metaData.value(QMediaMetaData::VideoBitRate).toInt();
    } else if (metaData.value(QMediaMetaData::AudioBitRate).isValid()) {
        m_bitRate = metaData.value(QMediaMetaData::AudioBitRate).toInt();
    } else {
        if (m_frameWidth > 0 && m_frameHeight > 0 && m_frameRate > 0) {
            int pixelCount = m_frameWidth * m_frameHeight;
            if (pixelCount > 0) {
                double estimatedBitRate = pixelCount * m_frameRate * 0.1;
                m_bitRate = static_cast<int>(estimatedBitRate);
            }
        }
    }

    QString infoText;
    if (m_frameWidth > 0 && m_frameHeight > 0) {
        QString bitRateText = "--";
        if (m_bitRate > 0) {
            if (m_bitRate >= 1000000) {
                bitRateText = QString("%1 Mbps").arg(QString::number(m_bitRate / 1000000.0, 'f', 1));
            } else if (m_bitRate >= 1000) {
                bitRateText = QString("%1 kbps").arg(QString::number(m_bitRate / 1000.0, 'f', 1));
            } else {
                bitRateText = QString("%1 bps").arg(m_bitRate);
            }
        }
        infoText = QString("📐 分辨率: %1x%2 | 📊 帧率: %3 fps | 📈 比特率: %4 | 🎬 %5")
                  .arg(m_frameWidth)
                  .arg(m_frameHeight)
                  .arg(m_frameRate > 0 ? QString::number(m_frameRate, 'f', 1) : "--")
                  .arg(bitRateText)
                  .arg(codecText);
    } else {
        QString bitRateInfo = "";
        if (m_bitRate > 0) {
            if (m_bitRate >= 1000000) {
                bitRateInfo = QString(" | 比特率: %1 Mbps").arg(QString::number(m_bitRate / 1000000.0, 'f', 1));
            } else if (m_bitRate >= 1000) {
                bitRateInfo = QString(" | 比特率: %1 kbps").arg(QString::number(m_bitRate / 1000.0, 'f', 1));
            } else {
                bitRateInfo = QString(" | 比特率: %1 bps").arg(m_bitRate);
            }
        }
        QString statusInfo = QString("📊 状态: 已连接 | 流类型: RTSP | 元数据: %1项%2 | 🎬 %3")
                           .arg(availableKeys.size())
                           .arg(bitRateInfo)
                           .arg(codecText);
        infoText = statusInfo;
    }
    ui->infoLabel->setText(infoText);
}

void RtspPlayer::attemptReconnect()
{
    if (!m_manualDisconnect && ui->autoReconnectCheckBox->isChecked() && !m_currentUrl.isEmpty()) {
        qDebug() << "尝试自动重连到:" << m_currentUrl;
        onConnectClicked();
    } else {
        m_isReconnecting = false;
        if (m_manualDisconnect) {
            ui->statusLabel->setText("状态: 已手动断开");
            ui->statusLabel->setStyleSheet("color: red;");
        }
    }
}

void RtspPlayer::onConnectionTimeout()
{
    if (!m_isConnected) {
        qDebug() << "连接超时";
        ui->statusLabel->setText("状态: 连接超时");
        ui->statusLabel->setStyleSheet("color: red;");
        if (m_isTesting) {
            m_testTimeoutTimer->stop();
            logTestResult(m_currentUrl, false, "连接超时");
        } else {
            showErrorMessage("连接超时，请检查网络连接或RTSP地址是否正确");
            m_mediaPlayer->stop();
            m_isConnected = false;
            updateUI();
            if (!m_manualDisconnect && ui->autoReconnectCheckBox->isChecked() && !m_isReconnecting) {
                m_isReconnecting = true;
                m_reconnectTimer->start();
                ui->statusLabel->setText("状态: 准备重连...");
                ui->statusLabel->setStyleSheet("color: orange;");
            } else if (m_manualDisconnect) {
                ui->statusLabel->setText("状态: 已手动断开");
                ui->statusLabel->setStyleSheet("color: red;");
            }
        }
    }
}

void RtspPlayer::updateUI()
{
    ui->connectButton->setEnabled(!m_isConnected && !m_isTesting);
    ui->disconnectButton->setEnabled(m_isConnected);
    ui->fullscreenButton->setEnabled(m_isConnected);
    ui->urlComboBox->setEnabled(!m_isConnected && !m_isTesting);
    ui->testAllButton->setEnabled(!m_isConnected && !m_isTesting);
}

void RtspPlayer::resetStreamInfo()
{
    m_frameWidth = 0;
    m_frameHeight = 0;
    m_frameRate = 0.0;
    m_bitRate = 0;
    ui->infoLabel->setText("📐 分辨率: -- | 📊 帧率: -- | 📈 比特率: -- | 🎬 编码器: 未知");
}

void RtspPlayer::showErrorMessage(const QString &message)
{
    QMessageBox::warning(this, "错误", message);
    qDebug() << "错误信息:" << message;
}

bool RtspPlayer::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_videoWidget) {
        if (event->type() == QEvent::MouseButtonDblClick) {
            if (m_isConnected) {
                onFullscreenClicked();
                qDebug() << "双击视频控件切换全屏模式";
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void RtspPlayer::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        if (m_videoWidget && m_videoWidget->isFullScreen()) {
            m_videoWidget->setFullScreen(false);
            ui->fullscreenButton->setText("📺 全屏");
            qDebug() << "ESC键退出全屏模式";
            return;
        }
        break;
    case Qt::Key_F11:
        if (m_videoWidget && m_isConnected) {
            onFullscreenClicked();
            return;
        }
        break;
    case Qt::Key_Space:
        if (m_isConnected && m_mediaPlayer) {
            if (m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState) {
                m_mediaPlayer->pause();
                qDebug() << "空格键暂停播放";
            } else if (m_mediaPlayer->playbackState() == QMediaPlayer::PausedState) {
                m_mediaPlayer->play();
                qDebug() << "空格键继续播放";
            }
            return;
        }
        break;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

void RtspPlayer::closeEvent(QCloseEvent *event)
{
    qDebug() << "RTSP播放器窗口正在关闭，清理资源...";
    
    // 停止所有定时器
    if (m_infoUpdateTimer) {
        m_infoUpdateTimer->stop();
    }
    if (m_reconnectTimer) {
        m_reconnectTimer->stop();
    }
    if (m_connectionTimeoutTimer) {
        m_connectionTimeoutTimer->stop();
    }
    if (m_testTimeoutTimer) {
        m_testTimeoutTimer->stop();
    }
    
    // 停止媒体播放器
    if (m_mediaPlayer) {
        m_mediaPlayer->stop();
        m_mediaPlayer->setSource(QUrl());
    }
    
    // 重置状态
    m_isConnected = false;
    m_isReconnecting = false;
    m_manualDisconnect = true;
    
    qDebug() << "RTSP播放器资源清理完成";
    
    // 接受关闭事件
    event->accept();
}

void RtspPlayer::onTestAllClicked()
{
    if (m_isTesting) {
        return;
    }
    m_isTesting = true;
    m_currentTestIndex = 0;
    ui->testResultTextEdit->clear();
    ui->testResultTextEdit->append("开始测试所有RTSP地址...\n");
    updateUI();
    ui->statusLabel->setText("状态: 测试中...");
    ui->statusLabel->setStyleSheet("color: blue;");
    testNextUrl();
}

void RtspPlayer::testNextUrl()
{
    if (!m_isTesting || m_currentTestIndex >= m_testUrls.size()) {
        m_isTesting = false;
        updateUI();
        ui->statusLabel->setText("状态: 测试完成");
        ui->statusLabel->setStyleSheet("color: gray;");
        ui->testResultTextEdit->append("\n=== 测试完成 ===");
        return;
    }

    QString url = m_testUrls[m_currentTestIndex];
    ui->testResultTextEdit->append(QString("测试 %1/%2: %3").arg(m_currentTestIndex + 1).arg(m_testUrls.size()).arg(url));

    m_mediaPlayer->stop();
    m_connectionTimeoutTimer->stop();

    m_currentUrl = url;
    m_mediaPlayer->setSource(QUrl(url));

    m_testTimeoutTimer->start();
    m_mediaPlayer->play();

    ui->statusLabel->setText(QString("状态: 测试中 (%1/%2)").arg(m_currentTestIndex + 1).arg(m_testUrls.size()));
    ui->statusLabel->setStyleSheet("color: blue;");
}

void RtspPlayer::logTestResult(const QString &url, bool success, const QString &error)
{
    if (success) {
        ui->testResultTextEdit->append(QString("✓ 成功: %1").arg(url));
        ui->testResultTextEdit->append("  → 这个地址可以正常播放！\n");
    } else {
        ui->testResultTextEdit->append(QString("✗ 失败: %1").arg(url));
        if (!error.isEmpty()) {
            ui->testResultTextEdit->append(QString("  错误: %1").arg(error));
        }
        ui->testResultTextEdit->append("");
    }

    m_currentTestIndex++;

    if (success) {
        m_isTesting = false;
        ui->urlComboBox->setCurrentText(url);
        updateUI();
        ui->statusLabel->setText("状态: 找到可用地址");
        ui->statusLabel->setStyleSheet("color: green;");
        m_isConnected = true;
        updateUI();
        ui->testResultTextEdit->append("=== 找到可用地址，停止测试 ===");
        return;
    }

    QTimer::singleShot(500, this, &RtspPlayer::testNextUrl);
}

void RtspPlayer::onDiagnoseClicked()
{
    showDiagnosticInfo();
}

void RtspPlayer::showDiagnosticInfo()
{
    QString diagnosticInfo;
    diagnosticInfo += "=== RTSP播放器诊断信息 ===\n\n";
    diagnosticInfo += QString("Qt版本: %1\n").arg(QT_VERSION_STR);
    diagnosticInfo += QString("Qt运行时版本: %1\n").arg(qVersion());
    diagnosticInfo += "\n支持的媒体格式:\n";
    QList<QString> supportedFormats = {
        "H.264 (AVC)",
        "H.265 (HEVC)",
        "MJPEG",
        "MPEG-4",
        "WMV",
        "AVI",
        "MP4",
        "MOV"
    };
    for (const QString &format : supportedFormats) {
        diagnosticInfo += QString("  • %1\n").arg(format);
    }
    diagnosticInfo += "\n网络信息:\n";
    diagnosticInfo += QString("  当前URL: %1\n").arg(m_currentUrl.isEmpty() ? "未设置" : m_currentUrl);
    diagnosticInfo += QString("  连接状态: %1\n").arg(m_isConnected ? "已连接" : "未连接");
    diagnosticInfo += "\n播放器状态:\n";
    diagnosticInfo += QString("  媒体状态: %1\n").arg(static_cast<int>(m_mediaPlayer->mediaStatus()));
    diagnosticInfo += QString("  播放状态: %1\n").arg(static_cast<int>(m_mediaPlayer->playbackState()));
    diagnosticInfo += "\n常见RTSP地址格式:\n";
    diagnosticInfo += "  • rtsp://用户名:密码@IP:端口/路径\n";
    diagnosticInfo += "  • rtsp://IP:端口/Streaming/Channels/101\n";
    diagnosticInfo += "  • rtsp://IP:端口/cam/realmonitor?channel=1&subtype=0\n";
    diagnosticInfo += "  • rtsp://IP:端口/h264/ch1/main/av_stream\n";
    diagnosticInfo += "  • rtsp://IP:端口/h265/ch1/main/av_stream\n";
    diagnosticInfo += "  • rtsp://IP:端口/onvif1\n";
    diagnosticInfo += "\n故障排除建议:\n";
    diagnosticInfo += "  1. 检查网络连接是否正常\n";
    diagnosticInfo += "  2. 确认RTSP服务器地址和端口正确\n";
    diagnosticInfo += "  3. 验证用户名和密码是否正确\n";
    diagnosticInfo += "  4. 尝试不同的RTSP路径格式\n";
    diagnosticInfo += "  5. 检查防火墙设置\n";
    diagnosticInfo += "  6. 确认摄像头支持RTSP协议\n";
    diagnosticInfo += "  7. 尝试使用VLC等工具测试RTSP流\n";

    QMessageBox::information(this, "诊断信息", diagnosticInfo);
    ui->testResultTextEdit->append("\n=== 诊断信息 ===");
    ui->testResultTextEdit->append(diagnosticInfo);
}




