#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QTimer>
#include <QDateTime>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QResizeEvent>

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
    
    // 初始化时间更新定时器
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateTime);
    updateTime();
    timer->start(1000);
    
    // 初始化棋盘视图与棋子
    initChessView();
    
    // 初始化流程视图（封装到 FlowViewManager）
    flowManager = new FlowViewManager(this);
    flowManager->init(ui->frame_2);
}

MainWindow::~MainWindow()
{
    delete ui;
}

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

void MainWindow::initChessView()
{
    if (!ui->graphicsView) return;

    if (!chessScene) chessScene = new QGraphicsScene(this);
    ui->graphicsView->setScene(chessScene);

    // 加载棋盘
    QPixmap boardPixmap(":/main/pic/Chessboard.png");
    chessScene->clear();
    boardItem = chessScene->addPixmap(boardPixmap);
    boardItem->setZValue(0);
    chessScene->setSceneRect(boardItem->boundingRect());

    // 视图设置
    ui->graphicsView->setRenderHint(QPainter::SmoothPixmapTransform);
    ui->graphicsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->graphicsView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->graphicsView->fitInView(chessScene->sceneRect(), Qt::KeepAspectRatio);

    // 计算网格单元尺寸（以交点为基准，9列10行 -> 相邻间距按 cols-1/rows-1 计算）
    const QRectF br = boardItem->boundingRect();
    cellWidth = br.width() / (gridCols - 1);
    cellHeight = br.height() / (gridRows - 1);

    // 创建若干棋子示例（可扩展为32个）。这里先放两个演示：红帅(4,9) 与 黑将(4,0)
    chessPieces.clear();
    QPixmap piecePixmap(":/main/pic/ChessPiece.png");
    for (int i = 0; i < 2; ++i) {
        QGraphicsPixmapItem *item = chessScene->addPixmap(piecePixmap);
        item->setZValue(1);
        // 以图片中心对齐到网格交点
        item->setOffset(-piecePixmap.width() / 2.0, -piecePixmap.height() / 2.0);
        chessPieces.append(item);
    }
    if (chessPieces.size() >= 2) {
        chessPieces[0]->setPos(gridCenterToScene(4, 9));
        chessPieces[1]->setPos(gridCenterToScene(4, 0));
    }
}

    // 流程视图逻辑已封装至 FlowViewManager



QPointF MainWindow::gridCenterToScene(int col, int row) const
{
    if (!boardItem) return QPointF();
    const QRectF br = boardItem->boundingRect();
    qreal x = br.left() + col * (br.width() / (gridCols - 1));
    qreal y = br.top() + row * (br.height() / (gridRows - 1));
    return QPointF(x, y);
}

void MainWindow::moveChessPiece(int pieceIndex, int col, int row)
{
    if (pieceIndex < 0 || pieceIndex >= chessPieces.size()) return;
    chessPieces[pieceIndex]->setPos(gridCenterToScene(col, row));
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (ui->graphicsView && chessScene) {
        ui->graphicsView->fitInView(chessScene->sceneRect(), Qt::KeepAspectRatio);
    }
    
    // 重新渲染流程视图以适应新的框体大小
    if (flowManager) {
        flowManager->relayout();
    }
}
