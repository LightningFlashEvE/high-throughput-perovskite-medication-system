#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QPointF>
#include "chessboardview.h"
#include "flowviewmanager.h"
#include "settingsbutton.h"
#include "tcpclient.h"
#include "recipeanalyzer.h"


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
class RtspPlayer;

/**
 * MainWindow
 *
 * 负责：
 * - 初始化与展示主界面
 * - 维护并渲染棋盘（QGraphicsView/QGraphicsScene）与单个可拖拽棋子
 * - 显示实时日期与时间
 *
 * 不负责：
 * - 业务流程图（已从项目中移除）
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * 构造函数
     * @param parent 父窗口
     *
     * 执行内容：
     * - 调用 setupUi 加载 UI
     * - 初始化并启动 1s 定时器更新时间
     * - 初始化棋盘视图与棋子
     */
    MainWindow(QWidget *parent = nullptr);
    /** 析构函数：释放 UI 资源 */
    ~MainWindow();

private slots:
    /**
     * 定时回调：每秒更新一次日期与时间标签
     * - 日期：yyyy.M.d
     * - 时间：hh:mm:ss
     */
    void updateTime();

protected:
    /**
     * 窗口大小变化事件
     * 作用：让 `graphicsView` 根据新的窗口大小自适应缩放棋盘（保持纵横比）
     */
    void resizeEvent(QResizeEvent *event) override;
    /**
     * 首次显示事件
     * 作用：在界面完成布局后再次触发一次自适应缩放，避免初次显示过小
     */
    void showEvent(QShowEvent *event) override;

private:
    Ui::MainWindow *ui;
    QTimer *timer;
    
    /**
     * 初始化data.ini文件
     * 检查文件是否存在，如果不存在则创建文件并设置默认值
     */
    void initializeDataIni();

    // 棋盘封装类
    ChessBoardView *chessBoard = nullptr;
    // 流程图封装类（挂载在 frame_2）
    FlowViewManager *flowManager = nullptr;
    // TcpFramedClient 已移除

    // 设置面板（非模态，可频繁打开关闭）
    SettingsButton *settingsPanel = nullptr;
    // TCP客户端调试面板
    TcpClient *tcpClientPanel = nullptr;
    // 配方解析面板
    RecipeAnalyzer *recipeAnalyzerPanel = nullptr;
    // RTSP播放器面板
    RtspPlayer *rtspPlayerPanel = nullptr;

    // zhua 当前所在网格（用于方向键移动）
    int zhuaCol = 4;
    int zhuaRow = 5;

public:
    /**
     * 将指定索引的棋子移动到网格坐标 (col, row) 的中心
     * 该方法会转发给 `ChessBoardView`
     */
    void moveChessPiece(int pieceIndex, int col, int row);


public: // 测试用 For temporary testing
    void ForTempTest();

};
#endif // MAINWINDOW_H



