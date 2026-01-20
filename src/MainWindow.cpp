#include "MainWindow.h"
//#include <QMenu>
#include <QMenuBar>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    //setWindowTitle("Menu Example");
    resize(800, 600);

    // ===== 文件菜单 =====
    QMenu *menuFile = menuBar()->addMenu(QStringLiteral("文件"));
    QMenu *menuSetup = menuBar()->addMenu(QStringLiteral("设置"));

    QAction *newAction = new QAction(tr("新建"), this);
    menuFile->addAction(newAction);

    connect(newAction, &QAction::triggered, this, &MainWindow::clickAction);
}

MainWindow::~MainWindow() {}

void MainWindow::clickAction() {
    qDebug() << "MainWindow::clickAction";
}
