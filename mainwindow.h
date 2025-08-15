#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QPointF>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QTimer;
class QGraphicsScene;
class QGraphicsPixmapItem;
class QResizeEvent;
class QShowEvent;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void updateTime();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    Ui::MainWindow *ui;
    QTimer *timer;

    // Graphics view / scene for chessboard rendering
    QGraphicsScene *chessScene = nullptr;
    QGraphicsPixmapItem *boardItem = nullptr;
    QVector<QGraphicsPixmapItem*> chessPieces;

    // Grid config (Chinese chess 9x10)
    int gridCols = 9;
    int gridRows = 10;
    qreal cellWidth = 0.0;
    qreal cellHeight = 0.0;

    void initChessView();
    QPointF gridCenterToScene(int col, int row) const;

public:
    // Move piece by index to board grid(col,row)
    void moveChessPiece(int pieceIndex, int col, int row);
};
#endif // MAINWINDOW_H



