#pragma once
#include <QWidget>
#include <QStringList>
#include <QMutex>

namespace AppLog {
    inline QMutex      g_mutex;
    inline QStringList g_lines;
    constexpr int      MAX_LINES = 2000;
}

class QPlainTextEdit;

class DebugLogWindow : public QWidget {
    Q_OBJECT
public:
    explicit DebugLogWindow(QWidget *parent = nullptr);
public slots:
    void appendLine(const QString &line);
private slots:
    void onClear();
    void onCopyAll();
private:
    QPlainTextEdit *m_text = nullptr;
};

// 全局窗口指针（主线程读写，message handler 通过 invokeMethod 间接访问）
inline DebugLogWindow *g_debugWindow = nullptr;
