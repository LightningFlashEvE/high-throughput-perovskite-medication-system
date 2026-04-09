#include "printdebug.h"
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QClipboard>
#include <QApplication>
#include <QFont>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QColor>

// 日志颜色高亮器：按行前缀染色
class LogHighlighter : public QSyntaxHighlighter {
public:
    using QSyntaxHighlighter::QSyntaxHighlighter;
protected:
    void highlightBlock(const QString &text) override {
        QTextCharFormat fmt;
        fmt.setFontWeight(QFont::Bold);
        if (text.startsWith("[CRT]") || text.startsWith("[FTL]")) {
            fmt.setForeground(QColor(210, 40, 40));       // 红
        } else if (text.startsWith("[WRN]")) {
            fmt.setForeground(QColor(200, 140, 0));       // 黄
        } else if (text.startsWith("[DBG] SEND")) {
            fmt.setForeground(QColor(30, 140, 60));       // 绿
        } else if (text.startsWith("[DBG] RECV")) {
            fmt.setForeground(QColor(20, 20, 20));        // 黑
        } else {
            fmt.setForeground(QColor(160, 160, 160));     // 浅灰
        }
        setFormat(0, text.length(), fmt);
    }
};

DebugLogWindow::DebugLogWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("打印调试");
    resize(900, 600);
    setWindowFlags(Qt::Window);
    setWindowIcon(QPixmap(":/main/pic/logo2.png"));

    m_text = new QPlainTextEdit(this);
    m_text->setReadOnly(true);
    m_text->setFont(QFont("Courier New", 9));
    new LogHighlighter(m_text->document());

    auto *btnClear = new QPushButton("清空", this);
    auto *btnCopy  = new QPushButton("复制全部", this);
    auto *btnRow   = new QHBoxLayout;
    btnRow->addWidget(btnClear);
    btnRow->addWidget(btnCopy);
    btnRow->addStretch();

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(btnRow);
    layout->addWidget(m_text);

    connect(btnClear, &QPushButton::clicked, this, &DebugLogWindow::onClear);
    connect(btnCopy,  &QPushButton::clicked, this, &DebugLogWindow::onCopyAll);

    // 打开时加载历史记录
    {
        QMutexLocker lk(&AppLog::g_mutex);
        m_text->setPlainText(AppLog::g_lines.join('\n'));
    }
    m_text->verticalScrollBar()->setValue(m_text->verticalScrollBar()->maximum());
}

void DebugLogWindow::appendLine(const QString &line)
{
    m_text->appendPlainText(line);
    QScrollBar *sb = m_text->verticalScrollBar();
    if (sb->value() >= sb->maximum() - 4)
        sb->setValue(sb->maximum());
}

void DebugLogWindow::onClear()
{
    m_text->clear();
    QMutexLocker lk(&AppLog::g_mutex);
    AppLog::g_lines.clear();
}

void DebugLogWindow::onCopyAll()
{
    QApplication::clipboard()->setText(m_text->toPlainText());
}
