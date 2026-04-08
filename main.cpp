#include "mainwindow.h"
#include "RtspPlayer/rtspplayer.h"
#include "printdebug.h"

#include <QApplication>
#include <qqml.h>
#include <cstdio>

// Qt 全局消息拦截函数。
// 通过 qInstallMessageHandler 注册后，所有 qDebug/qWarning/qCritical 都会走这里，
// 而不再走 Qt 的默认处理（默认只打印到 stderr）。
// 函数签名是 Qt 规定的固定格式，不能随意修改。
static void appMessageHandler(QtMsgType type,
                               const QMessageLogContext &,  // 包含文件名/行号，暂不使用
                               const QString &msg)          // qDebug() << "..." 里的内容
{
    // 根据消息类型确定前缀标签，方便在日志窗口中区分严重程度
    const char *level = "DBG";
    if      (type == QtWarningMsg)  level = "WRN";
    else if (type == QtCriticalMsg) level = "CRT";
    else if (type == QtFatalMsg)    level = "FTL";

    // 拼接成带标签的单行字符串，例如 "[DBG] 收到配方"
    QString line = QString("[%1] %2").arg(level, msg);

    // 1. 用 C 标准库打印到 stderr
    fprintf(stderr, "%s\n", qPrintable(line));

    // 2. 写入全局环形缓冲，供"打印调试"窗口打开时回填历史记录。
    //    用 QMutexLocker 加锁，因为 qDebug 可能从任意线程调用：
    //      - 构造 QMutexLocker 时自动加锁
    //      - 离开 {} 作用域时自动析构、自动解锁，无需手动 unlock()
    {
        QMutexLocker lk(&AppLog::g_mutex);
        AppLog::g_lines.append(line);
        // 超过上限时删掉最早的一条，防止内存无限增长
        if (AppLog::g_lines.size() > AppLog::MAX_LINES)
            AppLog::g_lines.removeFirst();
    }

    // 3. 如果"打印调试"窗口当前处于打开状态，就把这条消息实时发过去显示。
    //    不能直接调用 g_debugWindow->appendLine()，因为当前可能不在主线程，
    //    直接操作 UI 会崩溃。
    //    QMetaObject::invokeMethod + Qt::QueuedConnection 会把调用"投递"到
    //    主线程的事件队列，等主线程空闲时再安全地执行 appendLine()。
    //    Q_ARG(QString, line) 是跨线程传参的固定写法，告知参数类型和值。
    if (g_debugWindow) {
        QMetaObject::invokeMethod(g_debugWindow, "appendLine",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, line));
    }

    // FTL 级别是不可恢复的致命错误，走完上面的记录流程后仍需终止程序
    if (type == QtFatalMsg) abort();
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    qInstallMessageHandler(appMessageHandler);
    // 不再暴露 QWidget 给 QML，改为在 QWidget 内部加载 QML
    MainWindow w;
    w.show();
    return a.exec();
}
