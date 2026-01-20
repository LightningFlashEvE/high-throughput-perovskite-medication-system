#include "MainWindow.h"
#include "CommuInfoDialog.h"

#include <QMenuBar>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    //setWindowTitle("Menu Example");
    resize(800, 600);

    // 主界面菜单项
    QMenu *menuFile = menuBar()->addMenu(QStringLiteral("文件"));
    QMenu *menuTools = menuBar()->addMenu(QStringLiteral("工具"));
    QMenu *menuSetup = menuBar()->addMenu(QStringLiteral("设置"));
    QMenu *menuHelp = menuBar()->addMenu(QStringLiteral("帮助"));

    QAction *newAction = new QAction(tr("新建"), this);
    menuFile->addAction(newAction);

    QAction *debugAction = new QAction(tr("调试"), this);
    menuTools->addAction(debugAction);

    connect(newAction, &QAction::triggered, this, &MainWindow::clickAction);
    connect(debugAction, &QAction::triggered, this, &MainWindow::clickDebugAction);
}

MainWindow::~MainWindow() {}

void MainWindow::clickAction() {
    qDebug() << "MainWindow::clickAction";
}

void MainWindow::clickDebugAction() {
    //qDebug() << "MainWindow::clickDebugAction";
    CommuInfoDialog::getInstance()->show();
}
