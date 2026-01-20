#include "MainWindow.h"
#include "CommuInfoDialog.h"
#include "ControlPannel.h"
#include "TcpClient.h"

#include <QDebug>
#include <QMenuBar>
#include <QLayout>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    m_tcpClient(new TcpClient)
{
    setWindowTitle("高通量平台 V1.00");
    resize(800, 600);

    m_tcpClient->init("192.168.5.201", 4196);
    CommuInfoDialog::getInstance()->init(m_tcpClient);
    m_tcpClient->setCommuInfoDialog(CommuInfoDialog::getInstance());

    QWidget* centralWgt = new QWidget;
    QHBoxLayout* hLayout = new QHBoxLayout(centralWgt);

    // 主界面菜单项
    QMenu *menuFile = menuBar()->addMenu(QStringLiteral("文件"));
    QMenu *menuTools = menuBar()->addMenu(QStringLiteral("工具"));
    QMenu *menuSetup = menuBar()->addMenu(QStringLiteral("设置"));
    QMenu *menuHelp = menuBar()->addMenu(QStringLiteral("帮助"));

    QAction *newAction = new QAction(tr("新建"), this);
    menuFile->addAction(newAction);

    QAction *debugAction = new QAction(tr("调试"), this);
    menuTools->addAction(debugAction);

    hLayout->addWidget(new QLabel("左侧"));

    ControlPannel* pannel = new ControlPannel(this);
    hLayout->addWidget(pannel);

    setCentralWidget(centralWgt);

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
