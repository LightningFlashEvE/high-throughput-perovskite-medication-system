#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QTimer>
#include <QDateTime>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsDropShadowEffect>
#include <QCursor>
#include <QResizeEvent>
#include <QShowEvent>
#include <QPainter>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // 设置较为合适的初始窗口与视图尺寸，避免棋盘初始显示过小
 
    if (ui->graphicsView) {
        ui->graphicsView->setMinimumSize(650, 440);
    }
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
    
    // 初始化棋盘视图与棋子
    initChessView();
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
    ui->graphicsView->setInteractive(true);
    ui->graphicsView->setDragMode(QGraphicsView::NoDrag);

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

    // 创建一个默认棋子（放置在棋盘图片正中心）
    chessPieces.clear();
    QPixmap piecePixmap(":/main/pic/zhua.png");
    QGraphicsPixmapItem *item = chessScene->addPixmap(piecePixmap);
    item->setZValue(1);
    // 以图片中心对齐到网格交点
    item->setOffset(-piecePixmap.width() / 2.0, -piecePixmap.height() / 2.0);
    item->setTransformationMode(Qt::SmoothTransformation);
    item->setScale(0.1); // 缩小到原来的10%

    // 为棋子添加发光描边（白色）增强对比度
    QGraphicsDropShadowEffect *glow = new QGraphicsDropShadowEffect();
    glow->setOffset(0, 0);
    glow->setBlurRadius(20);
    glow->setColor(QColor(255, 255, 255, 220));
    item->setGraphicsEffect(glow);

    // 允许鼠标拖动棋子
    item->setFlag(QGraphicsItem::ItemIsMovable, true);
    item->setFlag(QGraphicsItem::ItemIsSelectable, true);
    item->setAcceptedMouseButtons(Qt::LeftButton);
    item->setCursor(Qt::OpenHandCursor);
    chessPieces.append(item);
    chessPieces[0]->setPos(boardItem->boundingRect().center());
}

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
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    // 首次显示后再调用一次fitInView，确保初始布局完成后按容器尺寸适配
    if (ui->graphicsView && chessScene) {
        ui->graphicsView->fitInView(chessScene->sceneRect(), Qt::KeepAspectRatio);
    }
}
