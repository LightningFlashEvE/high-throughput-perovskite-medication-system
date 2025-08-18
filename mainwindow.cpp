#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QTimer>
#include <QDateTime>
#include <QGraphicsView>
#include <QResizeEvent>
#include <QShowEvent>
#include <QPainter>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    /*** 显示logo ***/
    QPixmap logo(":/main/pic/logo.png");
    if (!logo.isNull()) {
        // 缩小到200像素宽度，保持比例
        QPixmap scaledLogo = logo.scaledToWidth(200, Qt::SmoothTransformation);
        ui->labelLogo->setPixmap(scaledLogo);

        // 可选：设置标签大小适应缩放后的图片
        ui->labelLogo->setFixedSize(scaledLogo.size());
    }
    
    /*** 初始化时间更新定时器 ***/
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateTime);
    updateTime();
    timer->start(1000);
    
    /*** 初始化棋盘视图与棋子（封装为 ChessBoardView） ***/
    if (ui->graphicsView) {
        ui->graphicsView->setMinimumSize(650, 440);
    }
    chessBoard = new ChessBoardView(this);
    chessBoard->init(ui->graphicsView);

    /*** 初始化流程视图（封装到 FlowViewManager，挂载 frame_2） ***/
    if (ui->frame_2) {
        ui->frame_2->setMinimumSize(450, 440);
    }
    flowManager = new FlowViewManager(this);
    flowManager->init(ui->frame_2);

}

MainWindow::~MainWindow()
{
    delete ui;
}

// 每秒刷新日期与时间显示
void MainWindow::updateTime()
{
    QDateTime currentDateTime = QDateTime::currentDateTime();
    
    // 更新日期显示 (格式: 2025.10.1)
    QString dateStr = currentDateTime.toString("yyyy.M.d");
    ui->labelDate->setText(dateStr);
    
    // 更新时间显示 (格式: 12:33:21)
    QString timeStr = currentDateTime.toString("hh:mm:ss");
    ui->labelTime->setText(timeStr);
}

// 将指定棋子移动到网格(col,row)
void MainWindow::moveChessPiece(int pieceIndex, int col, int row)
{
    if (!chessBoard) return;
    chessBoard->movePiece(pieceIndex, col, row);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (chessBoard) chessBoard->relayout();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    // 首次显示后再次自适应
    if (chessBoard) chessBoard->relayout();
}
