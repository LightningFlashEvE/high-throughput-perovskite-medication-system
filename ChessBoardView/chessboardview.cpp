#include "chessboardview.h"

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsDropShadowEffect>
#include <QPainter>

ChessBoardView::ChessBoardView(QObject *parent)
    : QObject(parent)
{
}

ChessBoardView::~ChessBoardView()
{
}

void ChessBoardView::init(QGraphicsView *view,
                          const QString &boardImage,
                          const QString &pieceImage)
{
    graphicsView = view;
    boardImagePath = boardImage;
    pieceImagePath = pieceImage;

    if (!graphicsView) return;

    scene = new QGraphicsScene(this);
    graphicsView->setScene(scene);

    setupView();
    loadBoard();
    createDefaultPiece();
    relayout();
}

void ChessBoardView::setupView()
{
    graphicsView->setInteractive(true);
    graphicsView->setDragMode(QGraphicsView::NoDrag);
    graphicsView->setRenderHint(QPainter::SmoothPixmapTransform);
    graphicsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    graphicsView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void ChessBoardView::loadBoard()
{
    scene->clear();
    QPixmap boardPixmap(boardImagePath);
    boardItem = scene->addPixmap(boardPixmap);
    boardItem->setZValue(0);
    scene->setSceneRect(boardItem->boundingRect());

    const QRectF br = boardItem->boundingRect();
    cellWidth = br.width() / (gridCols - 1);
    cellHeight = br.height() / (gridRows - 1);
}

void ChessBoardView::createDefaultPiece()
{
    pieces.clear();
    QPixmap piecePixmap(pieceImagePath);
    QGraphicsPixmapItem *item = scene->addPixmap(piecePixmap);
    item->setZValue(1);
    item->setOffset(-piecePixmap.width() / 2.0, -piecePixmap.height() / 2.0);
    item->setTransformationMode(Qt::SmoothTransformation);
    item->setScale(0.1);

    // 发光描边增强对比度
    QGraphicsDropShadowEffect *glow = new QGraphicsDropShadowEffect();
    glow->setOffset(0, 0);
    glow->setBlurRadius(20);
    glow->setColor(QColor(255, 255, 255, 220));
    item->setGraphicsEffect(glow);

    // 允许拖动
    item->setFlag(QGraphicsItem::ItemIsMovable, true);
    item->setFlag(QGraphicsItem::ItemIsSelectable, true);
    item->setAcceptedMouseButtons(Qt::LeftButton);
    item->setCursor(Qt::OpenHandCursor);

    pieces.append(item);
    pieces[0]->setPos(boardItem->boundingRect().center());
}

void ChessBoardView::relayout()
{
    if (!graphicsView || !scene) return;
    graphicsView->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
}

QPointF ChessBoardView::gridCenterToScene(int col, int row) const
{
    if (!boardItem) return QPointF();
    const QRectF br = boardItem->boundingRect();
    qreal x = br.left() + col * (br.width() / (gridCols - 1));
    qreal y = br.top() + row * (br.height() / (gridRows - 1));
    return QPointF(x, y);
}

void ChessBoardView::movePiece(int index, int col, int row)
{
    if (index < 0 || index >= pieces.size()) return;
    pieces[index]->setPos(gridCenterToScene(col, row));
}


