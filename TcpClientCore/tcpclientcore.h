#ifndef TCPCLIENTCORE_H
#define TCPCLIENTCORE_H

#include <QObject>
#include <QTcpSocket>
#include <QNetworkProxy>
#include <QTimer>
#include <QEventLoop>
#include <QQueue>
#include <QStringList>
#include <QDateTime>
#include <QVector>
#include <QMap>
#include <QMutex>



// 消息队列项
struct MessageQueueItem {
    QByteArray content;          // 发送内容
    bool asciiOrHex;             // 是否为ASCII模式
    bool shouldWaitForResponse;  // 是否需要等待响应
    QString expectedSignature;   // 期望接收值（设备+指令+数据，数据可空；如 "09D" / "09d01" / HEX模式传如 "050302000001"）
    
    MessageQueueItem() : asciiOrHex(true), shouldWaitForResponse(false) {}
    MessageQueueItem(const QByteArray& c, bool aoh) 
        : content(c), asciiOrHex(aoh), shouldWaitForResponse(false) {}
    MessageQueueItem(const QByteArray& c, bool aoh, const QString& expected)
        : content(c), asciiOrHex(aoh), shouldWaitForResponse(!expected.isEmpty() && expected != "-----"), expectedSignature(expected) {}
};

// 配方处理状态
enum RecipeProcessState {
    RecipeNotProcessed  = 0,  // 未处理：刚创建，还未加入任何发送/执行流程
    RecipeProcessing    = 1,  // 正在执行：已导入/正在执行
    RecipeFinished      = 2   // 执行完毕：该配方所有命令执行完成
};

// 配方消息队列项
struct RecipeQueueItem {
    QString recipeName;                      // 配方名称（化学方程式）
    QQueue<MessageQueueItem> messageQueue;   // 该配方的消息队列
    QDateTime createTime;                    // 创建时间
    RecipeProcessState processState;         // 配方处理状态
    
    RecipeQueueItem() : processState(RecipeNotProcessed) {}
};

class TcpClientCore : public QObject
{
    Q_OBJECT
public:
    explicit TcpClientCore(QObject *parent = nullptr);
    ~TcpClientCore();

    // tcp连接-接口
    bool connectToTcp(const QString& localIP, const QString& remoteIP, quint16 remotePort, bool proxyDisabled);

    // 调用TcpClientCrc类计算crc16
    quint16 calculateCrc16(const QByteArray& data);

    // tcp发送信息-接口（旧版本，保留兼容）
    // asciiOrHex: true 表示ascii，false表示16进制发送
    // 如果是 'd' 命令（查询到位），会自动轮询直到收到到位信号
    bool sendMessage(const QByteArray& content, bool asciiOrHex);
    
    // tcp发送信息-接口（新版队列方式）
    // 统一：第三个参数为期望接收值（设备+指令+数据；未知可传 "-----" 表示不等待）
    void sendMessageAsync(const QByteArray& content, bool asciiOrHex = true);
    void sendMessageAsync(const QByteArray& content, bool asciiOrHex, const QString& expectedSignature);
    
    // 清空消息队列
    void clearMessageQueue();

    // 暂停队列处理
    void pauseQueue();
    
    // 继续队列处理
    void resumeQueue();

    // 断开连接
    void disconnectFromTcp();
    // 仅复位内部状态（不主动断开连接）
    void resetState();

    // 检查连接状态
    bool isConnected() const;

    // 发送模式枚举
    enum SendMode {
        StringMode = 0,  // 字符串模式（默认）：直接发送字符串内容
        AsciiMode = 1,   // ASCII模式：直接发送字节
        HexMode = 2      // 十六进制模式：从十六进制字符串转换为字节
    };
    
    // 天平命令枚举
    enum BalanceCommand {
        BalancePrintOff = 0,  // 关闭天平打印
        BalancePrintOn = 1,   // 打开天平打印
        BalanceTare = 2        // 去皮
    };
    
    // 直接发送命令，不经过队列
    // data: 要发送的数据（QString 或 QByteArray 都可以，内部统一处理）
    // mode: 0=字符串模式（默认），1=ASCII模式，2=十六进制模式
    void writeBalanceTareCommand(const QString& data, int mode = StringMode);
    
    // 发送天平命令（使用枚举）
    // command: 0=关闭打印，1=打开打印，2=去皮
    void sendBalanceCommand(BalanceCommand command);
    
    // 设置期望重量值（用于称重对比）
    void setExpectedWeight(double weight);
    // 获取期望重量值
    double getExpectedWeight() const;
    // 清除期望重量值（恢复为0）
    void clearExpectedWeight();

    /**
     * 将整数转换为十六进制字符串（支持负数处理）
     * @param value 整数值（正数或负数）
     * @param width 十六进制字符串的位数
     * @return 转换后的十六进制字符串（负数会先-1再取反）
     * 
     * 负数处理：绝对值-1，然后取反，保持指定位数
     * 示例：
     *   int2hex(540, 4) -> "021C"
     *   int2hex(-540, 4) -> "FDE4"  (540-1=539=0x21B, 取反=0xFDE4)
     */
    static QString int2hex(qint64 value, int width);

    /**
     * 构建带CRC的消息
     * @param data 原始数据字符串（如 ">0AA"）
     * @return 拼接了CRC的完整消息（如 ">0AAA3FD"）
     */
    QString buildMessageWithCrc(const QString& data);
    
    /**
     * 电爪专用：构建带ModBus CRC的消息
     * @param data 原始十六进制数据字符串（如 "01060100001"）
     * @return 拼接了ModBus CRC的完整消息（如 "0106010000149F6"）
     */
    QString buildGripperMessageWithCrc(const QString& data);

    /**
     * 构建设备命令（重载版本，接受数字和位数）
     * @param deviceNum 设备编号字符串（如 "04", "03", "02"）
     * @param functionCode 功能码字符串（如 "D" 表示移动）
     * @param commandData 命令数据（整数，支持负数）
     * @param width 十六进制数据的位数（如 8 表示8位十六进制）
     * @return 完整的带CRC的命令
     * 
     * 示例：
     *   buildDeviceCommand("04", "D", 123456, 8) -> ">04D0001E240XXXX"
     *   buildDeviceCommand("04", "D", -123456, 8) -> ">04DFFFE1DC0XXXX"
     */
    QString buildDeviceCommand(const QString& deviceNum, const QString& functionCode, qint64 commandData, int width);
    
    /**
     * 构建设备命令（重载版本，4个参数）
     * @param deviceNum 设备ID/编号（如 "0A", "01", "07"）
     * @param functionCode 功能码（如 "M" 表示移动）
     * @param registerAddress 寄存器地址（如 "0000"）
     * @param registerData 寄存器数据（如 "1234"）
     * @return 完整的带CRC的命令
     * 
     * 示例：
     *   buildDeviceCommand("0A", "M", "0000", "1234") -> ">0AM00001234XXXX"
     */
    QString buildDeviceCommand(const QString& deviceNum, 
                              const QString& functionCode, 
                              const QString& registerAddress, 
                              const QString& registerData);
    
    /**
     * 构建设备命令（重载版本，5个参数，支持十进制数据）
     * @param deviceNum 设备ID/编号（如 "05"）
     * @param functionCode 功能码（如 "06"）
     * @param registerAddress 寄存器地址（如 "0105"）
     * @param decimalData 十进制数据（如 100，支持大数值）
     * @param dataWidth 数据位宽（默认4位，如4表示"0064"，8表示"00000064"，最大16位）
     * @return 完整的带CRC的命令
     * 
     * 支持范围：
     *   4位: 0 - 65,535 (0xFFFF)
     *   8位: 0 - 4,294,967,295 (0xFFFFFFFF)
     *   16位: 0 - 18,446,744,073,709,551,615 (0xFFFFFFFFFFFFFFFF)
     * 
     * 示例：
     *   buildDeviceCommand("05", "06", "0105", 100, 4) -> "0506010500649F6"
     *   buildDeviceCommand("05", "06", "0105", 100, 8) -> "050601050000006449F6"
     *   buildDeviceCommand("05", "06", "0105", 4294967295, 8) -> "05060105FFFFFFFFXXXX"
     */
    QString buildDeviceCommand(const QString& deviceNum, 
                              const QString& functionCode, 
                              const QString& registerAddress, 
                              qint64 decimalData,
                              int dataWidth = 4);

    /**
     * @brief 初始化信号槽连接和定时器（按需调用）
     * 将信号槽连接和定时器创建从构造函数中分离出来，因为并不是每次新建对象都会用到
     */
    void initializeConnectionsAndTimers();

    void initializeConnectionsForBalance();
    void disconnectConnectionsForBalance();


public slots:
    void connectReceiveForBalance();
    void disconnectReceiveForBalance();
signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString& errorMsg);
    void dataReceived(const QByteArray& data);
    
    // 电机到位信号
    void motorReachedPosition(const QString& deviceNum);
    
    // 6号电机Z轴坐标信号（当收到"06E"查询响应时发出）
    void z6CoordinateReceived(int coordinate);
    
    // 天平实时重量更新信号（每次收到天平数据都发出，用于状态栏实时显示）
    // currentWeight: 当前天平读数; targetThreshold: 当前正在瞄准的阈值; goalWeight: 最终目标重量
    void balanceWeightReceived(double currentWeight, double targetThreshold, double goalWeight);

    // 天平重量达标信号
    void weightReached(double weight);
    
    // 天平去皮请求信号（当检测到AA2命令时发出）
    void balanceTareRequested();
    
    // 设置期望重量请求信号（当检测到AAsetExpectedWeight命令时发出，携带重量值）
    void setExpectedWeightRequested(double weight);
    
    // 天平打印打开请求信号（当检测到AA1命令时发出）
    void balancePrintOnRequested();
    
    // 天平打印关闭请求信号（当检测到AA0命令时发出）
    void balancePrintOffRequested();
    
    /*
    * AA0  天平打印关
    * AA1  天平打印开
    * AA2  天平去皮
    * AAcloseShakeBed 关摇床
    * AAopenShakeBed  开摇床
    * AArecordShakeBedTime     记录摇床需要的时间
    * 第一步骤
    */
    // 记录摇床时间请求信号（当检测到AArecordShakeBedTime命令时发出，携带selfLocation和shakeDurationSeconds）
    void recordShakeBedTimeRequested(int selfLocation, int shakeDurationSeconds);

    // 配方状态变更请求信号（当检测到AAsetRecipeProcessState命令时发出，携带新的状态值）
    void recipeProcessStateChangeRequested(int newState);
    
    // 启动摇床请求信号（当检测到AAopenShakeBed命令时发出）
    void openShakeBedRequested();
    
    // 关闭摇床请求信号（当检测到AAcloseShakeBed命令时发出）
    void closeShakeBedRequested();
    
    // 启动摇床并在指定秒数后关闭请求信号（当检测到AAshakeBedForSeconds:X命令时发出）
    void shakeBedForSecondsRequested(int seconds);
    
    // 空瓶区currentIndex加1请求信号（当检测到AAemptyBottleAreaCurrentIndexPlusOne命令时发出）
    void emptyBottleAreaCurrentIndexPlusOneRequested();
    
    // tips头区currentIndex加1请求信号（当检测到AAtipsHeadAreaCurrentIndexPlusOne命令时发出，携带tipsHeadUsageSelfLocation）
    void tipsHeadAreaCurrentIndexPlusOneRequested(int tipsHeadUsageSelfLocation);

    // 摇床离开请求信号（当检测到AAleaveTheShaker命令时发出，携带selfLocation）
    void leaveTheShakerRequested(int selfLocation);

    // 流程状态变更信号（当检测到AAstateChange命令时发出，携带状态名称）
    void processStateChanged(const QString& stateName);

    // 步骤跳过信号（当检测到AAskipStep命令时发出，携带步骤名称）
    void stepSkipped(const QString& stepName);

    // 所有设备初始化完成的请求信号（当检测到AAallDevicesInitialized命令时发出）
    void allDevicesInitializedRequested();

    // 配方队列中当前批次消息全部发送完毕（m_messageQueue 为空且本轮处理结束）时发出
    // MainWindow 可以在此信号中调用 sendMessage(m_recipeMessageQueues) 导入下一个配方
    void messageQueueEmpty();

    // 响应超时且重试失败信号（当命令重试次数用尽后发出，携带失败的命令和期望的响应）
    void responseTimeoutFailed(const QString& command, const QString& expectedResponse);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);
    
    // 天平专用槽函数
    void onBalanceConnected();
    void onBalanceDisconnected();
    void onBalanceReadyRead();
    void onBalanceSocketError(QAbstractSocket::SocketError error);

    // 响应超时处理
    void onResponseTimeout();

public:
    QTcpSocket* m_tcpSocket;

    // 消息队列相关
    QQueue<MessageQueueItem> m_messageQueue;  // 消息队列
    bool m_isProcessingQueue;                  // 是否正在处理队列
    QTimer* m_queueTimer;                     // 队列处理定时器
    bool m_isWaitingForResponse;              // 是否正在等待响应
    bool m_isQueuePaused;                     // 队列是否被用户暂停（新增）

    // 响应等待机制（新）
    QString m_expectedResponse;               // 期望的响应内容
    bool m_expectedAsciiMode;                 // 期望响应是否为ASCII模式
    QElapsedTimer m_responseTimer;            // 响应超时计时器
    int m_retryCount;                         // 当前命令的重试次数
    QByteArray m_currentCommand;              // 当前正在等待响应的命令
    bool m_currentCommandAsciiMode;           // 当前命令是否为ASCII模式
    QTimer* m_responseTimeoutTimer;           // 响应超时定时器
    static const int MAX_RETRIES = 300;                // 普通命令最大重试次数
    static const int MOTOR_MAX_RETRIES = 400;        // 电机到位最大重试次数（400次×50ms = 20秒）
    static const int RESPONSE_TIMEOUT = 90;        // 响应超时时间（毫秒）
    static const int MOTOR_RESPONSE_TIMEOUT = 50;   // 电机到位轮询间隔（毫秒，设备不主动上报需主动查询）

    // 命令发送间隔控制
    QElapsedTimer m_lastSendTime;             // 上次发送命令的时间戳
    bool m_justFinishedWaiting;               // 刚完成响应等待，下一条命令跳过间隔
    static const int MIN_SEND_INTERVAL = 50; // 最小发送间隔（毫秒）

    // 步骤跳过相关
    bool m_isSkippingStep;                    // 是否正在跳过步骤
    QString m_currentSkippingStep;            // 当前正在跳过的步骤名称
    
    // 天平称重相关（静态变量使用 g_ 前缀表示全局共享）
    static double g_expectedWeight;            // 期望重量值（用于对比，默认为0，所有对象共用）
    static bool g_balancePrintEnabled;         // 是否打印天平接收数据（默认关闭，所有对象共用）
    static double g_weightThresholds[3];       // 3个重量阈值（所有对象共用）
    static bool g_thresholdTriggered[3];       // 标记每个阈值是否已触发（所有对象共用）
    static bool g_isWeightPauseActive;         // 重量暂停标志位（防止重复触发，所有对象共用）
    static QMutex g_weightCheckMutex;          // 称重检测互斥锁（防止多线程同时访问）
    
    // 计时器相关
    QMap<QString, QDateTime> m_timerStartTimes;  // 存储各个计时器的开始时间（timerName -> startTime）
    QMap<QString, qint64> m_timerResults;        // 存储各个计时器的耗时结果（timerName -> elapsedMs）
    
    // 自动重连相关
    QString m_lastRemoteIP;                       // 上次连接的远程IP
    quint16 m_lastRemotePort;                     // 上次连接的远程端口
    QString m_lastLocalIP;                        // 上次连接的本地IP
    bool m_lastProxyDisabled;                     // 上次连接是否禁用代理
    bool m_autoReconnectEnabled;                  // 是否启用自动重连
    bool m_manualDisconnect;                      // 是否手动断开（手动断开时不自动重连）
    int m_reconnectAttempts;                      // 当前重连尝试次数
    static const int MAX_RECONNECT_ATTEMPTS = 5;  // 最大重连尝试次数
    QTimer* m_reconnectTimer;                     // 重连定时器

    /**
     * @brief 尝试重连
     */
    void attemptReconnect();
    
    /**
     * @brief 设置是否启用自动重连
     * @param enabled 是否启用
     */
    void setAutoReconnectEnabled(bool enabled) { m_autoReconnectEnabled = enabled; }
    
    /**
     * @brief 处理消息队列
     */
    void processMessageQueue();
    
    /**
     * @brief 检查消息是否需要等待响应（通过分析命令内容）
     * @param content 消息内容
     * @return true 表示需要等待响应
     */
    bool needsWaitForResponse(const QByteArray& content, bool asciiOrHex);
    
    /**
     * @brief 发送消息内部实现（非阻塞版本，供队列使用）
     * @param content 消息内容
     * @param asciiOrHex 是否ASCII模式
     * @param shouldWait 是否应该启动轮询等待
     */
    void sendMessageInternal(const QByteArray& content, bool asciiOrHex, bool shouldWait);


    // 发送配方队列（引用方式，处理多个配方消息队列）
    void sendMessage(QVector<RecipeQueueItem>& recipeMessageQueues);
    
    


};

#endif // TCPCLIENTCORE_H

