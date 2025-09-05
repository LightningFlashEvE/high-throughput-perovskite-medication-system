#ifndef CHESSBOARDVIEW_H
#define CHESSBOARDVIEW_H

#include <QObject>
#include <QVector>
#include <QString>
#include <QPointF>

class QGraphicsView;
class QGraphicsScene;
class QGraphicsPixmapItem;
/* ******  试管状态机  up ******/
class QGraphicsEllipseItem;
class QGraphicsSimpleTextItem;
/* ******  试管状态机  down ******/
class QGraphicsEllipseItem;
class QGraphicsSimpleTextItem;

/**
 * ChessBoardView
 *
 * 封装棋盘与棋子的视图逻辑：
 * - 接管现有 QGraphicsView，内部创建并维护 QGraphicsScene
 * - 加载棋盘与棋子资源，完成缩放、中心对齐与拖拽设置
 * - 提供基于网格(9x10)的坐标定位与移动接口
 */
class ChessBoardView : public QObject {
    Q_OBJECT
public:
    explicit ChessBoardView(QObject *parent = nullptr);
    ~ChessBoardView() override;

    /**
     * 初始化并挂载到给定的 QGraphicsView
     * @param view 现有视图（UI中的 graphicsView）
     * @param boardImage 棋盘资源路径，默认":/main/pic/Chessboard.png"
     * @param pieceImage 棋子资源路径，默认":/main/pic/zhua.png"
     */
    void init(QGraphicsView *view,
              const QString &boardImage = ":/main/pic/Chessboard.png",
              const QString &pieceImage = ":/main/pic/zhua.png");

    /**
     * 自适应重排（保持纵横比将场景适配到视图）
     */
    void relayout();

    /**
     * 将索引为 index 的棋子移动到网格坐标 (col,row) 的中心
     * @param index 棋子索引（当前默认仅 0 有效）
     * @param col 列 [0, 8]
     * @param row 行 [0, 9]
     */
    void movePiece(int index, int col, int row);

    /**
     * 网格中心点转场景坐标
     */
    QPointF gridCenterToScene(int col, int row) const;

    // 统一查询网格尺寸（用于外部约束边界）
    int gridMaxCol() const { return gridCols - 1; }
    int gridMaxRow() const { return gridRows - 1; }
    /* ******  试管状态机  up ******/
    // 在棋盘上绘制试管并控制状态
    // 步骤说明：
    // 1) ensureTubeAtGrid ：在网格(col,row)定位中心 → 创建/更新圆形与标记图元
    // 2) setTubeState：修改当前状态并调用 applyTubeStyle 应用对应“皮肤”
    // 3) applyTubeStyle：根据状态选取颜色/边框/标记，直接作用到图元
    enum class TubeState { Empty, Full, Using, Error, Disabled };
    // 步骤1 实现：根据网格坐标创建/更新试管图元（圆+中心文字）
    void ensureTubeAtGrid(int col, int row, qreal radius = 8.0);
    void setTubeState(TubeState state);
    /* ******  试管状态机  down ******/

private:
    // 视图/场景与资源
    QGraphicsView *graphicsView = nullptr;
    QGraphicsScene *scene = nullptr;
    QGraphicsPixmapItem *boardItem = nullptr;
    QVector<QGraphicsPixmapItem*> pieces;
    QString boardImagePath;
    QString pieceImagePath;
    /* ******  试管状态机  up ******/
    // 试管图形元素
    QGraphicsEllipseItem *tubeItem = nullptr;
    QGraphicsSimpleTextItem *tubeMarkItem = nullptr;
    TubeState currentTubeState = TubeState::Empty;
    /* ******  试管状态机  down ******/

    // 网格参数
    int gridCols = 200;
    int gridRows = 200;
    qreal cellWidth = 0.0;
    qreal cellHeight = 0.0;

    void setupView();
    void loadBoard();
    void createDefaultPiece();
    /* ******  试管状态机  up ******/
    // 步骤3 实现：把当前状态对应的样式（颜色/边框/标记）套到图元上
    // 步骤3 实现：把当前状态对应的样式（颜色/边框/标记）套到图元上
    void applyTubeStyle();
    /* ******  试管状态机  down ******/
};

#endif // CHESSBOARDVIEW_H


