#ifndef RTSPPLAYER_H
#define RTSPPLAYER_H

#include <QWidget>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QMediaMetaData>
#include <QKeyEvent>
#include <QEvent>
#include <QList>
#include <QString>

namespace Ui {
class RtspPlayer;
}

class RtspPlayer : public QWidget
{
    Q_OBJECT

public:
    explicit RtspPlayer(QWidget *parent = nullptr);
    ~RtspPlayer();

    bool isConnected() const;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    // 初始化与UI
    void setupConnections();

    // 按钮事件
    void onConnectClicked();
    void onDisconnectClicked();
    void onFullscreenClicked();
    void onTestAllClicked();
    void onDiagnoseClicked();

    // 媒体事件
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    void onErrorOccurred(QMediaPlayer::Error error, const QString &errorString);
    void onPositionChanged(qint64 position);
    void onDurationChanged(qint64 duration);
    void onMetaDataChanged();

    // 定时器与流程
    void updateStreamInfo();
    void attemptReconnect();
    void onConnectionTimeout();
    void testNextUrl();
    void logTestResult(const QString &url, bool success, const QString &error = QString());

private:
    // UI辅助
    void updateUI();
    void resetStreamInfo();
    void showErrorMessage(const QString &message);
    void showDiagnosticInfo();

private:
    Ui::RtspPlayer *ui;

    // 媒体播放相关
    QMediaPlayer *m_mediaPlayer;
    QVideoWidget *m_videoWidget;

    // 网络
    QNetworkAccessManager *m_networkManager;

    // 定时器
    QTimer *m_infoUpdateTimer;
    QTimer *m_reconnectTimer;
    QTimer *m_connectionTimeoutTimer;
    QTimer *m_testTimeoutTimer;

    // 状态
    bool m_isConnected;
    bool m_isReconnecting;
    bool m_manualDisconnect;
    QString m_currentUrl;

    // 流信息
    int m_frameWidth;
    int m_frameHeight;
    double m_frameRate;
    int m_bitRate;

    // 批量测试
    QList<QString> m_testUrls;
    int m_currentTestIndex;
    bool m_isTesting;
};

#endif // RTSPPLAYER_H
