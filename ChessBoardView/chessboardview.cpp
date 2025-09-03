#include "chessboardview.h"

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
/* ******  试管状态机  up ******/
#include <QGraphicsEllipseItem>
#include <QGraphicsSimpleTextItem>
/* ******  试管状态机  down ******/
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

    /* ******  试管状态机  up ******/
    // 步骤1：在网格(51,57)放置试管（设置其圆形框与中心标记的位置/大小）
    ensureTubeAtGrid(103, 115, 12.0);
    /* ******  试管状态机  down ******/
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

/* ******  试管状态机  up ******/
/*
 * 步骤1 实现：根据网格坐标创建/更新试管图元（圆 + 中心文字）
 * 参数:
 * - col, row: 网格坐标（0..gridMaxCol/Row），内部经 gridCenterToScene 转为场景像素中心
 * - radius  : 圆半径（像素）
 * 过程:
 * 1) 计算中心点 center = gridCenterToScene(col,row)
 * 2) 首次调用时创建图元：
 *    - tubeItem：QGraphicsEllipseItem，设置 Z=2、初始画笔/画刷；
 *    - tubeMarkItem：QGraphicsSimpleTextItem，设置 Z=3、字体加粗；
 * 3) 每次调用：更新圆的矩形(以 center 为圆心、radius 为半径)；
 *    同时把文字放到圆的几何中心（按文字边界矩形居中摆放）。
 * 4) 调用 applyTubeStyle()，依据当前状态刷新颜色/边框/标记文本。
 * 副作用：scene 未就绪时直接返回；需要时内部向 scene 添加/更新图元。
 */
void ChessBoardView::ensureTubeAtGrid(int col, int row, qreal radius)
{
    if (!scene) return;
    const QPointF center = gridCenterToScene(col, row);
    if (!tubeItem) {
        tubeItem = scene->addEllipse(QRectF(center.x()-radius, center.y()-radius, 2*radius, 2*radius));
        tubeItem->setZValue(2);
        tubeItem->setPen(QPen(QColor("#d9534f"), 2));
        tubeItem->setBrush(Qt::NoBrush);
        tubeMarkItem = scene->addSimpleText(QStringLiteral("×"));
        tubeMarkItem->setZValue(3);
        QFont f = tubeMarkItem->font(); f.setBold(true); tubeMarkItem->setFont(f);
    }
    tubeItem->setRect(QRectF(center.x()-radius, center.y()-radius, 2*radius, 2*radius));
    tubeMarkItem->setPos(center.x()-tubeMarkItem->boundingRect().width()/2.0,
                         center.y()-tubeMarkItem->boundingRect().height()/2.0);
    applyTubeStyle();
}

// 步骤3 实现：把当前状态对应的样式（颜色/边框/标记）套到图元上
void ChessBoardView::applyTubeStyle()
{
    if (!tubeItem) return;
    struct TubeStyle { QBrush fill; QPen border; QString mark; };
    static const TubeStyle S[] = {
        {Qt::NoBrush,            QPen(QColor("#d9534f"), 2), QStringLiteral("×")},
        {QColor("#5cb85c"),      QPen(QColor("#2e7d32"), 1), QString()},
        {QColor(92,184,92,120),  QPen(QColor("#0275d8"), 2), QStringLiteral("•")},
        {QColor(217,83,79,60),   QPen(QColor("#d9534f"), 3), QStringLiteral("!")},
        {QColor(0,0,0,20),       QPen(QColor("#9e9e9e"), 1, Qt::DashLine), QString()}
    };
    const TubeStyle &st = S[static_cast<int>(currentTubeState)];
    tubeItem->setBrush(st.fill);
    tubeItem->setPen(st.border);
    if (tubeMarkItem) tubeMarkItem->setText(st.mark);
}

// 步骤2 实现：修改状态，然后调用步骤3应用样式
void ChessBoardView::setTubeState(TubeState state)
{
    currentTubeState = state;
    applyTubeStyle();
}
/* ******  试管状态机  down ******/

void ChessBoardView::relayout()
{
    if (!graphicsView || !scene) return;
    graphicsView->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
}

/*
 * 将网格坐标 (col, row) 映射为场景坐标系（QGraphicsScene）中的像素位置（网格中心点）。
 *
 * 参数说明：
 * - col：列索引，范围 [0, gridCols-1]，默认 gridCols=9（共 9 列）
 * - row：行索引，范围 [0, gridRows-1]，默认 gridRows=10（共 10 行）
 *
 * 计算方式：
 * - 以棋盘图 `boardItem` 的边界矩形 br 作为参考坐标系；
 * - 每格像素宽高分别为：
 *     cellWidth  = br.width()  / (gridCols - 1)
 *     cellHeight = br.height() / (gridRows - 1)
 * - 中心点坐标：
 *     x = br.left() + col * cellWidth
 *     y = br.top()  + row * cellHeight
 *
 * 返回值：
 * - 对应网格中心在场景中的 QPointF 像素坐标；若棋盘未加载，返回 (0,0)。
 *
 * 注意：
 * - 这是“整格”对齐；若需要“小于一格”的细微移动，请在外部改 col/row 为浮点插值，
 *   或新增按像素偏移的接口（例如 movePieceByPixels）。
 */
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


