#include "printdebug.h"
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QClipboard>
#include <QApplication>
#include <QFont>

DebugLogWindow::DebugLogWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("打印调试");
    resize(900, 600);
    setWindowFlags(Qt::Window);

    m_text = new QPlainTextEdit(this);
    m_text->setReadOnly(true);
    m_text->setFont(QFont("Courier New", 9));

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
