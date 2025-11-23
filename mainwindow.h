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

// 槽位坐标计算配置结构体
struct SlotPositionConfig {
    double sourceX;     // 原点X坐标（左上角）
    double sourceY;     // 原点Y坐标（左上角）
    int cols;           // 盘的列数（横向数量）
    int rows;           // 盘的行数（纵向数量）
    double spacingX;    // 横向间距（往左为负方向）
    double spacingY;   // 纵向间距（往下为正方向）
    
    SlotPositionConfig() : sourceX(0), sourceY(0), cols(0), rows(0), spacingX(0), spacingY(0) {}
    SlotPositionConfig(double x, double y, int c, int r, double sx, double sy) 
        : sourceX(x), sourceY(y), cols(c), rows(r), spacingX(sx), spacingY(sy) {}
};

class QTimer;
class QGraphicsScene;
class QGraphicsPixmapItem;
class QResizeEvent;
class QShowEvent;
class QCloseEvent;
class RtspPlayer;
class Box;
class ReagentBottle;
class TcpClientCore;
class AppSqlDatabase;

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
    
    /**
     * 定时回调：每秒检查一次摇床区域，如果endTime已到则停止摇床
     */
    void checkShakeBedTimeout();

    void on_pushButton_6_clicked();

    void on_pushButton_7_clicked();

    void on_pushButton_8_clicked();

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
    /**
     * 窗口关闭事件
     * 作用：在窗口关闭前清理所有资源，确保程序正常退出
     */
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::MainWindow *ui;
    QTimer *timer;
    QTimer *shakeBedCheckTimer;  // 摇床检查定时器
    
    /**
     * 初始化data.ini文件
     * 检查文件是否存在，如果不存在则创建文件并设置默认值
     */
    void initializeDataIni();
    
    /**
     * 清理所有资源
     * 在窗口关闭时调用，确保所有资源正确释放
     */
    void cleanupResources();

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
    
    // 转移区域和试剂管理
    Box *transferAreaBox = nullptr;          // 转移区左边区域（15槽位）
    ReagentBottle *reagentA = nullptr;       // A试剂
    ReagentBottle *reagentB = nullptr;       // B试剂
    ReagentBottle *reagentC = nullptr;       // C试剂
    
    // TCP客户端核心
    TcpClientCore *tcpCore = nullptr;        // TCP通信核心对象
    // TCP负责接收天平的串口信息
    TcpClientCore *tcpBalanceCore = nullptr;        // 天平TCP通信核心对象
    
    // 数据库管理
    AppSqlDatabase *dbm = nullptr;           // 数据库管理对象
    
    // 摇床初始化连接（用于监听启动回复）
    QMetaObject::Connection m_shakeBedInitConnection;

    // 取空瓶（盘名称）
    bool takeEmptyBottle(const QString& trayName);
    // 取液体（液体名称 + 体积 + tipsNum引用参数，用于返回使用的tips数量）
    bool getLiquid(const QString& liquidName, double volumeMl, int& tipsNum);
    // 取固体（固体名称 + 质量）
    bool getSolid(const QString& solidName, double mass);
    // 拧紧瓶子
    void tightenBottle();
    // 开盖（打开瓶盖）
    void openBottleCap();
    // 关盖（关闭瓶盖）
    void closeBottleCap();
    // 设置5号电机速度（速度范围：10-100）
    void setMotor5Speed(int speed);
    // 设置6号电机Z轴速度（速度单位：rpm，转/分钟）
    void setMotor6ZSpeed(int speed);
    // 5号电机旋转圈数（圈数：正数为顺时针，负数为逆时针）
    void rotateMotor5ByCircles(double circles);
    // 摇床（参数：时间或次数等）
    void shakeBed(int parameter);
    // 控制摇床开关（第一个参数：true=开，false=关；第二个参数：true=立即发送，false=异步发送）
    void controlShakeBed(bool isOn, bool sendImmediately = false);
    // 摇床初始化（摇3秒后停止）
    void initializeShakeBed();
    // 摇床完成后放置试剂瓶
    void placeShakenReagentBottle();
    // 摇床到成品区（从摇床区取瓶子并放置到成品区）
    void moveShakeBedToFinishedProductArea(int selfLocation);
    // 初始化所有设备（TCP连接和设备初始化）
    void initializeAllDevices();
    // xyz轴恢复到零点（06，08，09，0A号电机恢复到零点）
    void resetXYZMotorsToZero();

public:
    /**
     * 将指定索引的棋子移动到网格坐标 (col, row) 的中心
     * 该方法会转发给 `ChessBoardView`
     */
    void moveChessPiece(int pieceIndex, int col, int row);

    /**
     * 计算槽位坐标
     * @param config 槽位配置（原点、行列数、间距）
     * @param index 目标索引（从0开始）
     * @return 计算后的坐标点
     */
    QPoint calculateSlotPosition(const SlotPositionConfig& config, int index);

    /**
     * 将数据库表中指定字段的值自动加1
     * @param tableName 表名
     * @param fieldName 字段名
     * @param whereClause WHERE条件子句（例如："name = 'transferRightArea'"），如果为空则更新所有记录
     * @return 成功返回true，失败返回false
     */
    bool incrementDatabaseField(const QString& tableName, const QString& fieldName, const QString& whereClause = QString());

    /**
     * 将数据库表中指定字段的值自动减1
     * @param tableName 表名
     * @param fieldName 字段名
     * @param whereClause WHERE条件子句（例如："name = 'transferRightArea'"），如果为空则更新所有记录
     * @return 成功返回true，失败返回false
     */
    bool decrementDatabaseField(const QString& tableName, const QString& fieldName, const QString& whereClause = QString());

    /**
     * 记录摇床区域的时间信息
     * 更新other表的currentIndex，并更新shakeBedArea表的startTime、endTime和isEmpty
     * @param selfLocation 摇床位置编号
     * @param shakeDurationSeconds 摇床持续时间（秒），默认10秒
     * @return 成功返回true，失败返回false
     */
    bool recordShakeBedTime(int selfLocation, int shakeDurationSeconds = 10);

    /**
     * 初始化系统组件
     * 包括：转移区域、ABC试剂、TCP通信、按钮连接
     */
    void initializeSystemComponents();
    
public slots:
    // 测试配方发送功能（接收JSON对象）
    void testRecipeSend(const QJsonObject& recipePacket);
    
    // 测试配方发送功能（接收JSON对象和JSON字符串）
    void testRecipeSendWithString(const QJsonObject& recipePacket, const QString& jsonString);

};
#endif // MAINWINDOW_H

/*
 * AA0  天平打印关
 * AA1  天平打印开
 * AA2  天平去皮
 * AAcloseShakeBed 关摇床
 * AAopenShakeBed  开摇床
 * AArecordShakeBedTime     记录摇床需要的时间
 * AAemptyBottleAreaCurrentIndexPlusOne
 * AAtipsHeadAreaCurrentIndexPlusOne
 */


