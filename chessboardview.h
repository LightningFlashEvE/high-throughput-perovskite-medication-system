#ifndef CHESSBOARDVIEW_H
#define CHESSBOARDVIEW_H

#include <QObject>
#include <QVector>
#include <QString>
#include <QPointF>

class QGraphicsView;
class QGraphicsScene;
class QGraphicsPixmapItem;

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

private:
    // 视图/场景与资源
    QGraphicsView *graphicsView = nullptr;
    QGraphicsScene *scene = nullptr;
    QGraphicsPixmapItem *boardItem = nullptr;
    QVector<QGraphicsPixmapItem*> pieces;
    QString boardImagePath;
    QString pieceImagePath;

    // 网格参数（中国象棋）
    int gridCols = 9;
    int gridRows = 10;
    qreal cellWidth = 0.0;
    qreal cellHeight = 0.0;

    void setupView();
    void loadBoard();
    void createDefaultPiece();
};

#endif // CHESSBOARDVIEW_H


