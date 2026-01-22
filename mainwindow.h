#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QPointF>
#include <QQueue>
#include "chessboardview.h"
#include "flowviewmanager.h"
#include "settingsbutton.h"
#include "tcpclient.h"
#include "recipeanalyzer.h"
#include "tcpclientcore.h"

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
    double spacingX;    // 横向间距
    double spacingY;    // 纵向间距（往下为正方向）
    bool xDirectionReverse;  // X方向是否反向：false=减法(往左，默认), true=加法(往右)

    SlotPositionConfig() : sourceX(0), sourceY(0), cols(0), rows(0), spacingX(0), spacingY(0), xDirectionReverse(false) {}
    SlotPositionConfig(double x, double y, int c, int r, double sx, double sy, bool xReverse = false)
        : sourceX(x), sourceY(y), cols(c), rows(r), spacingX(sx), spacingY(sy), xDirectionReverse(xReverse) {}
};

class QTimer;
class QGraphicsScene;
class QGraphicsPixmapItem;
class QResizeEvent;
class QShowEvent;
class QCloseEvent;
class QLabel;
class RtspPlayer;
class Box;
class ReagentBottle;
class TcpClientCore;
class AppSqlDatabase;
class ControlPanel;
class QTcpSocket;

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
     * 摇床为空检查定时器回调：每10秒执行一次，检查摇床是否为空并停止摇床
     */
    void onShakeBedEmptyCheckTimeout();

    /**
     * 定时回调：每秒检查一次摇床区域，如果endTime已到则停止摇床
     */
    void checkShakeBedTimeout();

    void on_pushButton_6_clicked();

    void on_pushButton_7_clicked();

    void on_pushButton_8_clicked();

    /**
     * 紧急停止按钮（UI上的“紧急暂停”）点击槽
     * 目前为空实现，后续可填入紧急停止逻辑
     */
    void onEmergencyStopButtonClicked();

    void on_pushButton_3_clicked();

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
    QTimer *shakeBedEmptyCheckTimer;  // 摇床为空检查定时器（10秒执行一次，当摇床为空时停止摇床）
    QLabel *sliderValueLabel;  // 滑块值显示标签

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

    // 设备是否已经完成一次初始化（initializeAllDevices 调用后置为 true）
    bool m_allDevicesInitialized = false;


    /***************** UP *********************/
    // MessageQueueItem / RecipeQueueItem 在 tcpclientcore.h 中已定义为全局结构体
    // 这里直接使用全局定义，避免类型重复导致不匹配
    // 配方消息队列容器（每个配方对应一个独立的消息队列）
    QVector<RecipeQueueItem> m_recipeMessageQueues;
    /****************** DOWN ********************/

    // 取空瓶（盘名称 + 消息队列引用）
    bool takeEmptyBottle(const QString& trayName, QQueue<MessageQueueItem>& messageQueue);
    // 取液体（液体名称 + 体积 + tipsNum引用参数 + 消息队列引用）
    bool getLiquid(const QString& liquidName, double volumeMl, QQueue<MessageQueueItem>& messageQueue);
    // 取固体（固体名称 + 质量 + 消息队列引用 + 固体盘位置索引）
    bool getSolid(const QString& solidName, double mass, QQueue<MessageQueueItem>& messageQueue, int currentIndex = 0);
    // 拧紧瓶子（消息队列引用）
    void tightenBottle(QQueue<MessageQueueItem>& messageQueue);
    // 关盖（关闭瓶盖）
    void closeBottleCap(QQueue<MessageQueueItem>& messageQueue);
    // 开盖（打开瓶盖）- 队列版本，将开盖相关命令写入消息队列
    void openBottleCap(QQueue<MessageQueueItem>& messageQueue);
    // 设置5号电机速度（速度范围：10-100）
    void setMotor5Speed(int speed, QQueue<MessageQueueItem>& messageQueue);
    // 设置5号设备拧紧力度（力度范围：10-100）
    void setMotor5TighteningForce(int force, QQueue<MessageQueueItem>& messageQueue);
    // 设置6号电机Z轴速度（速度单位：rpm，转/分钟）
    void setMotor6ZSpeed(int speed, QQueue<MessageQueueItem>& messageQueue);
    // 5号电机旋转圈数（圈数：正数为顺时针，负数为逆时针）
    void rotateMotor5ByCircles(double circles, QQueue<MessageQueueItem>& messageQueue);
    // 摇床（参数：时间或次数等）
    void shakeBed(int parameter);
    // 控制摇床开关（第一个参数：true=开，false=关；第二个参数：true=立即发送，false=异步发送）
    void controlShakeBed(bool isOn, QQueue<MessageQueueItem>& messageQueue, bool sendImmediately = false);
    void controlShakeBed(bool isOn, bool sendImmediately = false);

    // 摇床初始化（摇3秒后停止）
    void initializeShakeBed(QQueue<MessageQueueItem>& messageQueue);
    // 摇床完成后放置试剂瓶
    void placeShakenReagentBottle();
    // 摇床到成品区（从摇床区取瓶子并放置到成品区）
    void moveShakeBedToFinishedProductArea(int selfLocation, QQueue<MessageQueueItem>& messageQueue);
    // 初始化所有设备（TCP连接和设备初始化）
    void initializeAllDevices(QQueue<MessageQueueItem>& messageQueue);
    // xyz轴恢复到零点（06，08，09，0A号电机恢复到零点）
    void resetXYZMotorsToZero(QQueue<MessageQueueItem>& messageQueue);

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
     * 保存配方到数据库
     * 将配方信息保存到 recipeQueue 表，并将消息队列保存到 recipeMessageQueue 表
     * @param recipe 要保存的配方对象
     * @param insertBeforeFirstPending 是否插入到第一个未执行配方之前（插队），默认false追加到末尾
     * @return 成功返回true，失败返回false
     */
    bool saveRecipeToDatabase(const RecipeQueueItem& recipe, bool insertBeforeFirstPending = false);

    /**
     * 从数据库加载并执行下一个未执行的配方
     * 查询 recipeQueue 表中 processState=0 的配方（按执行顺序）
     * 将消息队列加载到 tcpCore 执行
     * @return 成功加载并开始执行返回true，没有可执行配方返回false
     */
    bool loadAndExecuteNextRecipeFromDatabase();

    /**
     * 保存配方到数据库并尝试执行
     * @param recipe 要保存的配方对象
     * @param insertBeforeFirstPending 是否插入到第一个未执行配方之前（插队），默认false追加到末尾
     */
    void saveAndExecuteRecipe(const RecipeQueueItem& recipe, bool insertBeforeFirstPending = false);

    /**
     * 检查并重置中断的配方（开机时调用）
     * 1. 将 recipeQueue 表中 processState = 1（正在执行）的记录改为 9（开机中断）
     * 2. 将 shakeBedArea 表中 isEmpty = 2（占位但没用上）的记录改为 1（恢复为未使用），并清空 startTime 和 endTime
     * 用于标记上次关机时正在执行的配方和释放占位但没用上的摇床位置
     */
    void checkAndResetInterruptedRecipes();

    /**
     * 初始化系统组件
     * 包括：转移区域、ABC试剂、TCP通信、按钮连接
     */
    void initializeSystemComponents();

    /**
     * 获取本地无线网口的IP地址
     * @return 无线网口的IPv4地址，如果未找到则返回空字符串
     */
    QString getLocalWirelessIP();

    /**
     * 获取本地有线网口的IP地址
     * @return 有线网口的IPv4地址，如果未找到则返回空字符串
     */
    QString getLocalWiredIP();

public slots:
    // 测试配方发送功能（接收JSON对象）
    void testRecipeSend(const QJsonObject& recipePacket);

    // 测试配方发送功能（接收JSON对象和JSON字符串）
    void testRecipeSendWithString(const QJsonObject& recipePacket, const QString& jsonString);

private:
    void createMenuItemDialogs();
    template<typename T> void createDialog(const QString& title, QAction* itemAction) {
        QDialog* dialog = new T(m_currentTcpSocket);
        dialog->setWindowTitle(title);

        m_menuItemDialogsMap[title] = dialog;
        connect(itemAction, &QAction::triggered, this, [this](){
            QAction* openMenuItemDialogAction = qobject_cast<QAction*>(sender());
            if (!openMenuItemDialogAction) return;

            QDialog* dialog = m_menuItemDialogsMap[openMenuItemDialogAction->text()];
            dialog->show();
            dialog->raise();
            dialog->activateWindow();
        });
    }

private:
    QTcpSocket* m_currentTcpSocket{};
    ControlPanel* m_controlPanel{};

    QMap<QString, QDialog*> m_menuItemDialogsMap;
    QDialog* m_controlPanelDialog{};
    QDialog* m_commuInfoDialog{};
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


