#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QTimer>
#include <QDateTime>
#include <QGraphicsView>
#include <QResizeEvent>
#include <QShowEvent>
#include <QCloseEvent>
#include <QPainter>
#include <algorithm> // for std::clamp
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QPixmap>
#include <QPalette>
#include <QSettings>
#include <QFileInfo>
#include <QDebug>
#include <QApplication>
#include "rtspplayer.h"
#include "tcpclientcore.h"
#include "qsqldatabase.h"
#include "databasesettingsdialog.h"
#include "networksettingsdialog.h"
#include "motorcontrol.h"
#include <QtSql/QSqlQuery>
#include <QSqlError>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 将“停止”按钮（pushButton_Stop）关联到紧急停止槽
    if (ui->pushButton_Stop) {
        connect(ui->pushButton_Stop, &QPushButton::clicked, this, &MainWindow::onEmergencyStopButtonClicked);
    }


    // 初始化系统组件（转移区域、试剂、TCP通信等）
    initializeSystemComponents();

    // 初始化tcpBalanceCore串口组件
    
    // 初始化data.ini文件。1.检查是否存在文件，不存在则创建文件并且提供默认值。
    initializeDataIni();

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

    /*** 初始化摇床检查定时器（不自动启动，等待TCP连接成功后再启动）***/
    shakeBedCheckTimer = new QTimer(this);
    connect(shakeBedCheckTimer, &QTimer::timeout, this, &MainWindow::checkShakeBedTimeout);
    // 定时器将在点击连接按钮后启动

    /*** 初始化摇床为空检查定时器（10秒执行一次，不自动启动，等待初始化完成后启动）***/
    shakeBedEmptyCheckTimer = new QTimer(this);
    connect(shakeBedEmptyCheckTimer, &QTimer::timeout, this, &MainWindow::onShakeBedEmptyCheckTimeout);
    // 定时器间隔将在 start() 时设置

    /*** 初始化棋盘视图与棋子（封装为 ChessBoardView） ***/
    if (ui->graphicsView) {
        ui->graphicsView->setMinimumSize(650, 440);
        // 让 QGraphicsView 背景跟随全局主题（明/暗）
        ui->graphicsView->setBackgroundBrush(palette().window());
        if (ui->graphicsView->viewport()) {
            ui->graphicsView->viewport()->setAutoFillBackground(false);
        }
    }
    chessBoard = new ChessBoardView(this);
    chessBoard->init(ui->graphicsView);

    /*** 绑定 widget_m 配方队列面板子控件 ***/
    m_recipeCurrentLabel = ui->m_recipeCurrentLabel;
    m_recipeQueueList    = ui->listWidget_recipeQueue;

    // 绑定流程步骤勾选框
    m_processCheckBox_reset = ui->m_processCheckBox_reset;
    m_processCheckBox_takeEmptyBottle = ui->m_processCheckBox_takeEmptyBottle;
    m_processCheckBox_getSolid = ui->m_processCheckBox_getSolid;
    m_processCheckBox_resetXYZ = ui->m_processCheckBox_resetXYZ;
    m_processCheckBox_getLiquid = ui->m_processCheckBox_getLiquid;
    m_processCheckBox_tightenBottle = ui->m_processCheckBox_tightenBottle;

    // 先加载保存的状态（在连接信号之前，避免触发保存）
    loadProcessStepsState();

    // 连接复选框状态改变信号，保存到ini文件
    connect(m_processCheckBox_reset, &QCheckBox::stateChanged, this, &MainWindow::saveProcessStepsState);
    connect(m_processCheckBox_takeEmptyBottle, &QCheckBox::stateChanged, this, &MainWindow::saveProcessStepsState);
    connect(m_processCheckBox_getSolid, &QCheckBox::stateChanged, this, &MainWindow::saveProcessStepsState);
    connect(m_processCheckBox_resetXYZ, &QCheckBox::stateChanged, this, &MainWindow::saveProcessStepsState);
    connect(m_processCheckBox_getLiquid, &QCheckBox::stateChanged, this, &MainWindow::saveProcessStepsState);
    connect(m_processCheckBox_tightenBottle, &QCheckBox::stateChanged, this, &MainWindow::saveProcessStepsState);

    // 绑定运行按钮
    m_runSelectedStepsButton = ui->m_runSelectedStepsButton;

    // 连接运行按钮的点击事件
    connect(m_runSelectedStepsButton, &QPushButton::clicked, this, &MainWindow::runSelectedSteps);

    // 绑定清除状态按钮
    m_clearProcessStateButton = ui->m_clearProcessStateButton;

    // 连接清除状态按钮的点击事件
    connect(m_clearProcessStateButton, &QPushButton::clicked, this, &MainWindow::resetProcessStateDisplay);

    // 初始化流程状态显示为灰色
    resetProcessStateDisplay();

    if (ui->menuStatus) {
        // 创建一个菜单项
        QAction *statusAction = new QAction("查看状态", this);
        QAction *statusAction2 = new QAction("状态", this);
        ui->menuStatus->addAction(statusAction);
        ui->menuStatus->addAction(statusAction2);

        // 连接菜单项的点击事件
        connect(statusAction, &QAction::triggered, this, []{
            qDebug() << "哈哈哈";
        });
        connect(statusAction2, &QAction::triggered, this, []{
            qDebug() << "哈哈哈";
        });
    }

    if (ui->menuSettings) {
        // 创建一个菜单项
        QAction *settingsAction485 = new QAction("485调试", this);
        ui->menuSettings->addAction(settingsAction485);
        connect(settingsAction485, &QAction::triggered, this, [this] {
            if (!settingsPanel) {
                settingsPanel = new SettingsButton(nullptr); // 独立窗口
                settingsPanel->setAttribute(Qt::WA_DeleteOnClose, true);
                settingsPanel->setWindowFlag(Qt::Window, true);
                connect(settingsPanel, &QObject::destroyed, this, [this] { settingsPanel = nullptr; });

                // 方向按键信号 -> 移动 zhua
                const int step = 1; // 每次移动一个网格
                connect(settingsPanel, &SettingsButton::moveUpClicked, this, [this] {
                    int maxRow = chessBoard ? chessBoard->gridMaxRow() : 99;
                    //int maxCol = chessBoard ? chessBoard->gridMaxCol() : 99;
                    zhuaRow = std::clamp(zhuaRow - step, 0, maxRow);
                    moveChessPiece(0, zhuaCol, zhuaRow);
                    settingsPanel->setLocation(zhuaCol, zhuaRow);
                });
                connect(settingsPanel, &SettingsButton::moveDownClicked, this, [this] {
                    int maxRow = chessBoard ? chessBoard->gridMaxRow() : 99;
                    zhuaRow = std::clamp(zhuaRow + step, 0, maxRow);
                    moveChessPiece(0, zhuaCol, zhuaRow);
                    settingsPanel->setLocation(zhuaCol, zhuaRow);
                });
                connect(settingsPanel, &SettingsButton::moveLeftClicked, this, [this] {
                    int maxCol = chessBoard ? chessBoard->gridMaxCol() : 99;
                    zhuaCol = std::clamp(zhuaCol - step, 0, maxCol);
                    moveChessPiece(0, zhuaCol, zhuaRow);
                    settingsPanel->setLocation(zhuaCol, zhuaRow);
                });
                connect(settingsPanel, &SettingsButton::moveRightClicked, this, [this] {
                    int maxCol = chessBoard ? chessBoard->gridMaxCol() : 99;
                    zhuaCol = std::clamp(zhuaCol + step, 0, maxCol);
                    moveChessPiece(0, zhuaCol, zhuaRow);
                    settingsPanel->setLocation(zhuaCol, zhuaRow);
                });

                // 坐标文本框提交后，移动至指定网格
                connect(settingsPanel, &SettingsButton::positionEdited, this, [this](int col, int row){
                    int maxCol = chessBoard ? chessBoard->gridMaxCol() : 199;
                    int maxRow = chessBoard ? chessBoard->gridMaxRow() : 199;
                    zhuaCol = std::clamp(col, 0, maxCol);
                    zhuaRow = std::clamp(row, 0, maxRow);
                    moveChessPiece(0, zhuaCol, zhuaRow);
                    if (settingsPanel) settingsPanel->setLocation(zhuaCol, zhuaRow);
                });

                /******  试管状态机  up ******/
                // 步骤4：UI按钮 -> 触发状态切换
                // 1) 用户点击设置页按钮（Empty/Full/Using/Error/Disable）
                // 2) 这里监听到点击后，调用 chessBoard->setTubeState(...)
                // 3) setTubeState 内部修改状态并调用 applyTubeStyle 套用样式，圆形外观立即变化
                // 将按钮作用于棋盘上(50,50)的试管状态
                if (settingsPanel->findChild<QPushButton*>("pushButtonUsing")) {
                    connect(settingsPanel->findChild<QPushButton*>("pushButtonUsing"), &QPushButton::clicked, this, [this]{
                        if (chessBoard) chessBoard->setTubeState(ChessBoardView::TubeState::Using);
                    });
                }
                if (settingsPanel->findChild<QPushButton*>("pushButtonFull")) {
                    connect(settingsPanel->findChild<QPushButton*>("pushButtonFull"), &QPushButton::clicked, this, [this]{
                        if (chessBoard) chessBoard->setTubeState(ChessBoardView::TubeState::Full);
                    });
                }
                if (settingsPanel->findChild<QPushButton*>("pushButtonError")) {
                    connect(settingsPanel->findChild<QPushButton*>("pushButtonError"), &QPushButton::clicked, this, [this]{
                        if (chessBoard) chessBoard->setTubeState(ChessBoardView::TubeState::Error);
                    });
                }
                if (settingsPanel->findChild<QPushButton*>("pushButtonDisable")) {
                    connect(settingsPanel->findChild<QPushButton*>("pushButtonDisable"), &QPushButton::clicked, this, [this]{
                        if (chessBoard) chessBoard->setTubeState(ChessBoardView::TubeState::Disabled);
                    });
                }
                if (settingsPanel->findChild<QPushButton*>("pushButtonEmpty")) {
                    connect(settingsPanel->findChild<QPushButton*>("pushButtonEmpty"), &QPushButton::clicked, this, [this]{
                        if (chessBoard) chessBoard->setTubeState(ChessBoardView::TubeState::Empty);
                    });
                }
                /******  试管状态机  down ******/
            }
            settingsPanel->show();
            settingsPanel->raise();
            settingsPanel->activateWindow();
            // 打开时同步显示当前位置
            settingsPanel->setLocation(zhuaCol, zhuaRow);
        });

        QAction *settingsActionTcp = new QAction("TCP调试", this);
        ui->menuSettings->addAction(settingsActionTcp);
        connect(settingsActionTcp, &QAction::triggered, this, [this] {
            if (!tcpClientPanel) {
                tcpClientPanel = new TcpClient(nullptr); // 独立窗口
                tcpClientPanel->setAttribute(Qt::WA_DeleteOnClose, true);
                tcpClientPanel->setWindowFlag(Qt::Window, true);
                tcpClientPanel->setWindowTitle("TCP 通信调试工具");
                connect(tcpClientPanel, &QObject::destroyed, this, [this] { tcpClientPanel = nullptr; });
            }
            tcpClientPanel->show();
            tcpClientPanel->raise();
            tcpClientPanel->activateWindow();
            qDebug() << "TCP调试窗口已打开";
        });

        QAction *settingsActionPy = new QAction("配方解析", this);
        ui->menuSettings->addAction(settingsActionPy);
        connect(settingsActionPy, &QAction::triggered, this, [this] { // 配方解析菜单项点击事件
            if (!recipeAnalyzerPanel) {
                recipeAnalyzerPanel = new RecipeAnalyzer(nullptr);
                recipeAnalyzerPanel->setAttribute(Qt::WA_DeleteOnClose, true);
                recipeAnalyzerPanel->setWindowFlag(Qt::Window, true);
                recipeAnalyzerPanel->setWindowTitle("配方解析工具");
                connect(recipeAnalyzerPanel, &QObject::destroyed, this, [this] { recipeAnalyzerPanel = nullptr; });
                
                // 连接配方发送信号到测试槽函数
                connect(recipeAnalyzerPanel, &RecipeAnalyzer::recipeReadyToSend, this, &MainWindow::testRecipeSend);
            }
            recipeAnalyzerPanel->show();
            recipeAnalyzerPanel->raise();
            recipeAnalyzerPanel->activateWindow();
            qDebug() << "配方解析窗口已打开";
        });

        // RTSP播放器菜单项
        QAction *settingsActionRtsp = new QAction("RTSP 播放器", this);
        ui->menuSettings->addAction(settingsActionRtsp);
        connect(settingsActionRtsp, &QAction::triggered, this, [this] {
            if (!rtspPlayerPanel) {
                rtspPlayerPanel = new RtspPlayer(nullptr);
                rtspPlayerPanel->setAttribute(Qt::WA_DeleteOnClose, true);
                rtspPlayerPanel->setWindowFlag(Qt::Window, true);
                rtspPlayerPanel->setWindowTitle("RTSP 播放器");
                connect(rtspPlayerPanel, &QObject::destroyed, this, [this] { rtspPlayerPanel = nullptr; });
            }
            rtspPlayerPanel->show();
            rtspPlayerPanel->raise();
            rtspPlayerPanel->activateWindow();
        });

        // 网络设置菜单项（含数据库与 TCP）
        QAction *settingsActionNetwork = new QAction("网络设置", this);
        ui->menuSettings->addAction(settingsActionNetwork);
        connect(settingsActionNetwork, &QAction::triggered, this, [this] {
            NetworkSettingsDialog dlg(dbm, tcpCore, tcpBalanceCore, this);
            dlg.exec();
        });

        // 电机控制菜单项
        QAction *settingsActionMotor = new QAction("电机控制", this);
        ui->menuSettings->addAction(settingsActionMotor);
        connect(settingsActionMotor, &QAction::triggered, this, [this] {
            MotorControl dlg(tcpCore, tcpBalanceCore, dbm, this);
            dlg.exec();
        });
    }

    if (ui->menuHistory) {
        QAction *sthAction = new QAction("历史按键", this);
        ui->menuHistory->addAction(sthAction);
        connect(sthAction, &QAction::triggered, this, [] {
            qDebug() << "xixihaha";
        });
    }

    /*** 初始化状态栏天平重量实时显示标签 ***/
    m_statusWeightLabel = new QLabel(this);
    m_statusWeightLabel->setFont(QFont("Courier New", 9));
    m_statusWeightLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui->statusbar->addWidget(m_statusWeightLabel, 1);
    // 开机时用占位符填充，确保格式立刻可见
    updateWeightStatusBar(0.0, 0.0, 0.0);

    /*** 初始化状态栏日期时间标签（最右侧固定区域）***/
    m_statusDateTimeLabel = new QLabel(this);
    m_statusDateTimeLabel->setFont(QFont("Courier New", 9));
    m_statusDateTimeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_statusDateTimeLabel->setContentsMargins(8, 0, 4, 0);
    ui->statusbar->addPermanentWidget(m_statusDateTimeLabel);

}

MainWindow::~MainWindow()
{
    // 清理资源（在 closeEvent 中已经处理，这里作为保险）
    cleanupResources();
    
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    qDebug() << "主窗口正在关闭，开始清理资源...";
    
    // 清理所有资源
    cleanupResources();
    
    // 接受关闭事件
    event->accept();
    qDebug() << "主窗口关闭完成";
}

// update emergency stop button text and color
void MainWindow::updateEmergencyStopButton()
{
    if (!ui->pushButton_Stop) return;
    if (m_isEmergencyPaused) {
        ui->pushButton_Stop->setText(QString::fromUtf8("继续运行"));
        ui->pushButton_Stop->setStyleSheet(QStringLiteral("color: green;"));
    } else {
        ui->pushButton_Stop->setText(QString::fromUtf8("紧急暂停"));
        ui->pushButton_Stop->setStyleSheet(QStringLiteral("color: red;"));
    }
}

// 放弃当前配方并预载下一条
void MainWindow::abandonCurrentRecipeAndLoadNext()
{
    qDebug() << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━";
    qDebug() << "放弃当前配方并预载下一条";
    qDebug() << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━";

    if (!tcpCore || !dbm) {
        qWarning() << "TCP核心或数据库未初始化";
        return;
    }

    // 1. 清空 TCP 队列
    tcpCore->clearMessageQueue();
    qDebug() << "  步骤1: 已清空 TCP 队列";

    // 2. 更新数据库：将当前正在执行的配方标记为"已放弃"（状态码 8）
    QString updateSql = QString("UPDATE recipeQueue SET processState = 8 WHERE processState = %1")
        .arg(RecipeProcessing);
    QSqlQuery updateQuery = dbm->query(updateSql);
    if (updateQuery.lastError().isValid()) {
        qWarning() << "  步骤2: 更新配方状态为已放弃失败:" << updateQuery.lastError().text();
    } else {
        int rowsAffected = updateQuery.numRowsAffected();
        qDebug() << "  步骤2: 已将" << rowsAffected << "个配方标记为已放弃（状态码 8）";
    }

    // 3. 预载下一条配方（但不启动执行，因为仍处于暂停状态）
    qDebug() << "  步骤3: 预载下一条配方到队列（不自动启动）";

    // 调用 loadAndExecuteNextRecipeFromDatabase，但由于 m_isQueuePaused = true，
    // sendMessage() 不会自动启动定时器
    if (!loadAndExecuteNextRecipeFromDatabase()) {
        qDebug() << "  ⚠ 没有更多待执行的配方";
    } else {
        qDebug() << "  ✓ 下一条配方已预载到队列，等待用户点击「继续运行」";
    }

    qDebug() << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
}

void MainWindow::onEmergencyStopButtonClicked()
{
    if (!tcpCore) {
        qWarning() << "tcpCore not initialized";
        return;
    }

    if (!m_isEmergencyPaused) {
        // running -> pause: 先暂停队列并刹停所有设备
        tcpCore->pauseQueue();

        QString stop02 = tcpCore->buildDeviceCommand("02", "K", 0, 1);
        tcpCore->sendMessage(stop02.toUtf8(), true);
        QString stop03 = tcpCore->buildDeviceCommand("03", "K", 0, 1);
        tcpCore->sendMessage(stop03.toUtf8(), true);
        QString stop04 = tcpCore->buildDeviceCommand("04", "K", 0, 1);
        tcpCore->sendMessage(stop04.toUtf8(), true);
        QString stop06 = tcpCore->buildDeviceCommand("06", "K", 0, 1);
        tcpCore->sendMessage(stop06.toUtf8(), true);
        QString stop09 = tcpCore->buildDeviceCommand("09", "K", 0, 1);
        tcpCore->sendMessage(stop09.toUtf8(), true);
        QString stop0A = tcpCore->buildDeviceCommand("0A", "K", 0, 1);
        tcpCore->sendMessage(stop0A.toUtf8(), true);

        m_isEmergencyPaused = true;
        updateEmergencyStopButton();

        // 弹出选择对话框
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(QString::fromUtf8("紧急暂停"));
        msgBox.setText(QString::fromUtf8("已暂停当前配方执行，请选择操作："));
        QPushButton *btnPauseOnly = msgBox.addButton(
            QString::fromUtf8("仅暂停"), QMessageBox::AcceptRole);
        QPushButton *btnAbandon   = msgBox.addButton(
            QString::fromUtf8("放弃当前配方"), QMessageBox::DestructiveRole);
        msgBox.setDefaultButton(btnPauseOnly);
        msgBox.exec();

        if (msgBox.clickedButton() == btnAbandon) {
            abandonCurrentRecipeAndLoadNext();
        }
        // 若选"仅暂停"，保持暂停状态等待用户点「继续运行」

    } else {
        // paused -> resume
        tcpCore->resumeQueue();

        m_isEmergencyPaused = false;
        updateEmergencyStopButton();
    }
}

void MainWindow::cleanupResources()
{
    // 停止定时器
    if (timer) {
        timer->stop();
        timer->disconnect(); // 断开所有信号连接
    }
    
    if (shakeBedCheckTimer) {
        shakeBedCheckTimer->stop();
        shakeBedCheckTimer->disconnect(); // 断开所有信号连接
    }
    
    if (shakeBedEmptyCheckTimer) {
        shakeBedEmptyCheckTimer->stop();
        shakeBedEmptyCheckTimer->disconnect(); // 断开所有信号连接
    }
    
    // 断开TCP连接（首先断开，避免阻塞）
    if (tcpCore) {
        qDebug() << "开始清理TCP连接...";
        tcpCore->disconnectFromTcp();
        // 处理事件，让断开操作完成（增加等待时间确保完全断开）
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 500);
        qDebug() << "TCP连接清理完成";
        // 显式销毁，确保析构中完全清理内部资源
        tcpCore->disconnect();
        delete tcpCore;
        tcpCore = nullptr;
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 50);
    }
    
    // 关闭数据库连接
    if (dbm) {
        qDebug() << "开始关闭数据库连接...";
        dbm->close();
        qDebug() << "数据库连接已关闭";
        delete dbm;
        dbm = nullptr;
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 50);
    }
    
    // 清理RTSP播放器面板（直接删除，不使用deleteLater）
    if (rtspPlayerPanel) {
        // 先断开所有信号连接，避免在删除时触发回调
        rtspPlayerPanel->disconnect();
        rtspPlayerPanel->close();
        rtspPlayerPanel->hide(); // 先隐藏，避免闪烁
        delete rtspPlayerPanel;
        rtspPlayerPanel = nullptr;
        // 处理事件，让删除操作完成
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 50);
    }
    
    // 清理TCP客户端面板（直接删除）
    if (tcpClientPanel) {
        tcpClientPanel->disconnect();
        tcpClientPanel->close();
        tcpClientPanel->hide();
        delete tcpClientPanel;
        tcpClientPanel = nullptr;
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 50);
    }
    
    // 清理配方解析面板（直接删除）
    if (recipeAnalyzerPanel) {
        recipeAnalyzerPanel->disconnect();
        recipeAnalyzerPanel->close();
        recipeAnalyzerPanel->hide();
        delete recipeAnalyzerPanel;
        recipeAnalyzerPanel = nullptr;
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 50);
    }
    
    // 清理设置面板（直接删除）
    if (settingsPanel) {
        settingsPanel->disconnect();
        settingsPanel->close();
        settingsPanel->hide();
        delete settingsPanel;
        settingsPanel = nullptr;
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 50);
    }
    
    // 清理棋盘（是 this 的子对象，会自动清理）
    if (chessBoard) {
        chessBoard = nullptr; // 是 this 的子对象，会在析构时自动删除
    }
    
    // 最后处理一次事件，确保所有删除操作完成
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 100);
    
    qDebug() << "资源清理完成";
}

// 更新状态栏天平重量显示（三列：当前 / 目标 / 目的，各占8位，靠左对齐）
void MainWindow::updateWeightStatusBar(double current, double target, double goal)
{
    if (!m_statusWeightLabel) return;

    // 将数值格式化为宽度8、保留4位小数、靠左显示的字段
    // 若值为0则以 "--" 占位，保持列宽一致
    auto fmtField = [](double v) -> QString {
        if (v == 0.0)
            return QString("--").leftJustified(8);
        return QString::number(v, 'f', 4).leftJustified(8);
    };
    auto fmtCurrent = [](double v) -> QString {
        return QString::number(v, 'f', 4).leftJustified(8);
    };

    QString text = QString("当前：<b>%1</b>  目标：<b>%2</b>  目的：<b>%3</b>")
                       .arg(fmtCurrent(current))
                       .arg(fmtField(target))
                       .arg(fmtField(goal));
    m_statusWeightLabel->setText(text);
}

// 每秒刷新日期与时间显示（底部状态栏最右侧）
void MainWindow::updateTime()
{
    QDateTime currentDateTime = QDateTime::currentDateTime();
    if (m_statusDateTimeLabel) {
        m_statusDateTimeLabel->setText(
            currentDateTime.toString("yyyy.M.d") + "  " + currentDateTime.toString("hh:mm:ss"));
    }
    updateRecipeQueuePanel();
}

// 刷新 widget_m 配方队列面板（当前执行 + 即将执行）
void MainWindow::updateRecipeQueuePanel()
{
    if (!dbm || !m_recipeCurrentLabel || !m_recipeQueueList) return;

    // 当前执行（processState = 1）
    QSqlQuery curQ = dbm->query(
        "SELECT id, recipeName, createTime FROM recipeQueue "
        "WHERE processState = 1 LIMIT 1");
    if (curQ.next()) {
        QString id   = curQ.value("id").toString();
        QString name = curQ.value("recipeName").toString();
        QString dt   = curQ.value("createTime").toString();
        m_recipeCurrentLabel->setText(QString("%1 - %2 - %3").arg(id, name, dt));
    } else {
        m_recipeCurrentLabel->setText(tr("无"));
    }

    // 即将执行（processState = 0，按 executionOrder 排序）
    QSqlQuery pendQ = dbm->query(
        "SELECT id, recipeName, createTime FROM recipeQueue "
        "WHERE processState = 0 ORDER BY executionOrder ASC");
    m_recipeQueueList->clear();
    while (pendQ.next()) {
        QString id   = pendQ.value("id").toString();
        QString name = pendQ.value("recipeName").toString();
        QString dt   = pendQ.value("createTime").toString();
        m_recipeQueueList->addItem(QString("%1 - %2 - %3").arg(id, name, dt));
    }
}

// 摇床为空检查定时器回调：每10秒执行一次，检查摇床是否为空并停止摇床
void MainWindow::onShakeBedEmptyCheckTimeout()
{
    // 如果 表shakeBedArea的isEmpty全为1的记录，则停止摇床
    if (!dbm) {
        qWarning() << "数据库对象未初始化，无法检查摇床是否为空";
        return;
    }

    if (!tcpCore) {
        qWarning() << "TCP核心对象未初始化，无法停止摇床";
        return;
    }

    // qDebug() << "\n================";
    // qDebug() << "|  检查摇床为空  |";
    // qDebug() << "================";

    // 查询shakeBedArea表中所有记录的isEmpty字段
    QString sql = "SELECT isEmpty FROM shakeBedArea";
    QSqlQuery query = dbm->query(sql);

    if (query.lastError().isValid()) {
        qWarning() << "查询shakeBedArea表失败:" << query.lastError().text();
        return;
    }

    // 检查是否所有记录的isEmpty都为1
    bool allEmpty = true;
    bool hasRecords = false;
    
    while (query.next()) {
        hasRecords = true;
        int isEmpty = query.value("isEmpty").toInt();
        if (isEmpty != 1) {
            allEmpty = false;
            break;
        }
    }

    // 如果表中有记录且所有记录的isEmpty都为1，则停止摇床
    if (hasRecords && allEmpty) {
        // qDebug() << "摇床区域全部为空，正在停止摇床...";
        controlShakeBed(false, true);
    }
}

// 检查摇床超时并停止
void MainWindow::checkShakeBedTimeout()
{
    // 获取当前时间（用于比较）
    QDateTime currentDateTime = QDateTime::currentDateTime();

    if (!dbm) {
        qWarning() << "数据库对象未初始化，无法检查摇床超时";
        return;
    }

    if (!tcpCore) {
        qWarning() << "TCP核心对象未初始化，无法停止摇床";
        return;
    }

    if (m_allDevicesInitialized) { // 如果已经初始化,则开启摇床为空检查定时器
        if (shakeBedEmptyCheckTimer && !shakeBedEmptyCheckTimer->isActive()) {
            shakeBedEmptyCheckTimer->start(10000);  // 10秒 = 10000毫秒
            qDebug() << "摇床为空检查定时器已启动（10秒执行一次，当摇床为空时停止摇床）";
        }
    }





    // 查询shakeBedArea表中isEmpty为3的记录
    QString sql = "SELECT selfLocation, endTime, isEmpty FROM shakeBedArea WHERE isEmpty = 3";
    QSqlQuery query = dbm->query(sql);

    if (query.lastError().isValid()) {
        qWarning() << "查询shakeBedArea表失败:" << query.lastError().text();
        return;
    }
    // 遍历查询结果
    while (query.next()) {
        int selfLocation = query.value("selfLocation").toInt();
        QString endTimeStr = query.value("endTime").toString().trimmed();
        int isEmpty = query.value("isEmpty").toInt();
        qDebug() << "检查摇床位置:" << selfLocation << "endTimeStr:" << endTimeStr << "isEmpty():" << endTimeStr.isEmpty() << "length:" << endTimeStr.length();
        // 如果endTime为空，跳过
        if (endTimeStr.isEmpty()) {
            qDebug() << "摇床位置" << selfLocation << "的endTime为空，跳过此记录";
            continue;
        }
        // 将字符串时间转换为QDateTime进行比较
        QDateTime endDateTime = QDateTime::fromString(endTimeStr, "yyyy-MM-dd hh:mm:ss");
        if (!endDateTime.isValid()) {
            qWarning() << QString("摇床位置 %1 的结束时间格式无效: %2").arg(selfLocation).arg(endTimeStr);
            continue;
        }

        if (currentDateTime > endDateTime && isEmpty == 3) {

            // ++++ 1.创建配方 ++++
            RecipeQueueItem newRecipeTianPing;
            newRecipeTianPing.recipeName = "tianPing";
            newRecipeTianPing.createTime = QDateTime::currentDateTime();
            newRecipeTianPing.processState = RecipeNotProcessed;

            // ++++ 2.填充配方内容 ++++
            qDebug() << QString("摇床位置 %1 的结束时间已到，正在停止摇床...").arg(selfLocation);
            // 停止摇床
            controlShakeBed(false, newRecipeTianPing.messageQueue, true);
            // 取到放置区（摇床到成品区）
            moveShakeBedToFinishedProductArea(selfLocation, newRecipeTianPing.messageQueue);
            // 通过 AAleaveTheShaker 指令，将 selfLocation 里面的状态4改1
            QString leaveCmd = QString("AAleaveTheShaker:%1").arg(selfLocation);
            newRecipeTianPing.messageQueue.enqueue(MessageQueueItem(leaveCmd.toUtf8(), true));

            resetXYZMotorsToZero(newRecipeTianPing.messageQueue);

            // ++++ 3.保存到数据库并执行（插队模式：插入到第一个未执行配方之前） ++++
            saveAndExecuteRecipe(newRecipeTianPing, true);

            // ++++ 4.isEmpty自加1 ++++（表示已记录在流程里，值变为4，放置后4改1）
            QString updateSql = QString("UPDATE shakeBedArea SET isEmpty = isEmpty+1 WHERE selfLocation = %1").arg(selfLocation);
            QSqlQuery updateQuery = dbm->query(updateSql);
            if (updateQuery.lastError().isValid()) {
                qWarning() << "更新shakeBedArea表失败:" << updateQuery.lastError().text();
            } else {
                qDebug() << QString("已更新摇床位置 %1 的状态为空").arg(selfLocation);
            }

            break;
        }
    }
}

// 摇床到成品区（从摇床区取瓶子并放置到成品区）
void MainWindow::moveShakeBedToFinishedProductArea(int selfLocation, QQueue<MessageQueueItem>& messageQueue)
{
    if (!dbm) {
        qCritical() << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━";
        qCritical() << "数据库对象未初始化，无法执行摇床到成品区操作，触发紧急暂停";
        qCritical() << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━";
        if (ui && ui->pushButton_Stop)
            QMetaObject::invokeMethod(ui->pushButton_Stop, "click", Qt::QueuedConnection);
        return;
    }

    if (!tcpCore) {
        qCritical() << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━";
        qCritical() << "TCP核心对象未初始化，无法执行摇床到成品区操作，触发紧急暂停";
        qCritical() << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━";
        if (ui && ui->pushButton_Stop)
            QMetaObject::invokeMethod(ui->pushButton_Stop, "click", Qt::QueuedConnection);
        return;
    }

    qDebug() << QString("开始执行摇床位置 %1 到成品区的操作").arg(selfLocation);

    // 去shakeBedArea找字段isEmpty的值为3的记录，然后取字段selfLocation的值出来待用
    QString shakeBedAreaSql = "SELECT selfLocation FROM shakeBedArea WHERE isEmpty = 3";
    QSqlQuery shakeBedAreaQuery = dbm->query(shakeBedAreaSql);
    int shakeBedAreaSelfLocation=0;
    if (shakeBedAreaQuery.next()) {
        shakeBedAreaSelfLocation = shakeBedAreaQuery.value("selfLocation").toInt();
        qDebug() << "+++++++++++++1" << shakeBedAreaSelfLocation;
    }
    else {
        qCritical() << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━";
        qCritical() << "未找到 shakeBedArea 中 isEmpty = 3 的记录，触发紧急暂停";
        qCritical() << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━";
        if (ui && ui->pushButton_Stop)
            QMetaObject::invokeMethod(ui->pushButton_Stop, "click", Qt::QueuedConnection);
        return;
    }
    
    // 关闭摇床
    QString stopShakeCommand = tcpCore->buildMessageWithCrc(QStringLiteral(">0Cxi30100000000"));
    qDebug() << "stopShakeCommand" << stopShakeCommand;
    //tcpCore->sendMessageAsync(stopShakeCommand.toUtf8(), true, "0Cxi301");
    messageQueue.enqueue(MessageQueueItem(stopShakeCommand.toUtf8(), true, "0Cxi301"));


    // 去other表，获取originX, originY, gripperZ, rightSpacing, bottomSpacing, cols, rows, currentIndex
    QString otherSql = "SELECT originX, originY, gripperZ, rightSpacing, bottomSpacing, cols, `rows`, currentIndex FROM other WHERE name = 'shakeBedArea'";
    QSqlQuery otherQuery = dbm->query(otherSql);
    int otherOriginX=0, otherOriginY=0, otherGripperZ=0;
    double otherRightSpacing=0, otherBottomSpacing=0;
    int otherCols=0, otherRows=0, otherCurrentIndex=0;
    if (otherQuery.next()) {
        otherOriginX = otherQuery.value("originX").toInt();
        otherOriginY = otherQuery.value("originY").toInt();
        otherGripperZ = otherQuery.value("gripperZ").toInt();
        otherRightSpacing = otherQuery.value("rightSpacing").toDouble();
        otherBottomSpacing = otherQuery.value("bottomSpacing").toDouble();
        otherCols = otherQuery.value("cols").toInt();
        otherRows = otherQuery.value("rows").toInt();
        otherCurrentIndex = otherQuery.value("currentIndex").toInt();
        qDebug() << "+++++++++++++2" << otherOriginX << otherOriginY << otherGripperZ << otherRightSpacing << otherBottomSpacing << otherCols << otherRows << otherCurrentIndex;
    }
    else {
        qCritical() << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━";
        qCritical() << "未找到 other 表中 shakeBedArea 记录，触发紧急暂停";
        qCritical() << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━";
        if (ui && ui->pushButton_Stop)
            QMetaObject::invokeMethod(ui->pushButton_Stop, "click", Qt::QueuedConnection);
        return;
    }
    // 通过originX, originY, gripperZ, rightSpacing, bottomSpacing, cols, rows, currentIndex计算出摇床的空位坐标
    SlotPositionConfig shakeBedAreaConfig(otherOriginX, otherOriginY, otherCols, otherRows, otherRightSpacing, otherBottomSpacing);
    QPoint shakeBedAreaTargetPos = calculateSlotPosition(shakeBedAreaConfig, shakeBedAreaSelfLocation);
    int shakeBedAreaTargetX = shakeBedAreaTargetPos.x();
    int shakeBedAreaTargetY = shakeBedAreaTargetPos.y();
    // 移动到摇床区指定位置
    QString moveToShakeBedAreaXCommand = tcpCore->buildDeviceCommand("0A", "D", shakeBedAreaTargetX, 8);
    //tcpCore->sendMessageAsync(moveToShakeBedAreaXCommand.toUtf8(), true);
    messageQueue.enqueue(MessageQueueItem(moveToShakeBedAreaXCommand.toUtf8(), true, "0AD"));
    QString waitShakeBedAreaXCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    //tcpCore->sendMessageAsync(waitShakeBedAreaXCommand.toUtf8(), true, "0Ad01");
    messageQueue.enqueue(MessageQueueItem(waitShakeBedAreaXCommand.toUtf8(), true, "0Ad01"));
    QString moveToShakeBedAreaYCommand = tcpCore->buildDeviceCommand("09", "D", shakeBedAreaTargetY, 8);
    //tcpCore->sendMessageAsync(moveToShakeBedAreaYCommand.toUtf8(), true);
    messageQueue.enqueue(MessageQueueItem(moveToShakeBedAreaYCommand.toUtf8(), true, "09D"));
    QString waitShakeBedAreaYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    //tcpCore->sendMessageAsync(waitShakeBedAreaYCommand.toUtf8(), true, "09d01");
    messageQueue.enqueue(MessageQueueItem(waitShakeBedAreaYCommand.toUtf8(), true, "09d01"));
    QString moveToShakeBedAreaZCommand = tcpCore->buildDeviceCommand("06", "D", otherGripperZ, 8);
    //tcpCore->sendMessageAsync(moveToShakeBedAreaZCommand.toUtf8(), true);
    messageQueue.enqueue(MessageQueueItem(moveToShakeBedAreaZCommand.toUtf8(), true, "06D"));
    QString waitShakeBedAreaZCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    //tcpCore->sendMessageAsync(waitShakeBedAreaZCommand.toUtf8(), true, "06d01");
    messageQueue.enqueue(MessageQueueItem(waitShakeBedAreaZCommand.toUtf8(), true, "06d01"));


    // 5号电机夹住瓶子
    QString enableGripperCommand = tcpCore->buildDeviceCommand("05", "06", "0105", 100, 4);
    //tcpCore->sendMessageAsync(enableGripperCommand.toUtf8(), false);
    messageQueue.enqueue(MessageQueueItem(enableGripperCommand.toUtf8(), false));
    QString waitGripperEnableCommand = tcpCore->buildDeviceCommand("05", "03", "0202", 1, 4);
    //tcpCore->sendMessageAsync(waitGripperEnableCommand.toUtf8(), false, "0503020002");
    messageQueue.enqueue(MessageQueueItem(waitGripperEnableCommand.toUtf8(), false, "0503020002"));
    // 6号电机上移
    QString raiseTransferZCommand = tcpCore->buildDeviceCommand("06", "D", 100, 8);
    //tcpCore->sendMessageAsync(raiseTransferZCommand.toUtf8(), true);
    messageQueue.enqueue(MessageQueueItem(raiseTransferZCommand.toUtf8(), true, "06D"));
    QString waitTransferZRaisedCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    //tcpCore->sendMessageAsync(waitTransferZRaisedCommand.toUtf8(), true, "06d01");
    messageQueue.enqueue(MessageQueueItem(waitTransferZRaisedCommand.toUtf8(), true, "06d01"));

    // 从 pan_init 获取成品区盘首坐标和网格参数
    QString finishedPanSql = "SELECT x, y, gripperZ, rightSpacing, bottomSpacing, cols, `rows` FROM pan_init WHERE name = 'finashedPositon'";
    QSqlQuery finishedPanQuery = dbm->query(finishedPanSql);
    int finishedX=0, finishedY=0, finishedZ=0;
    int finishedRightSpacing=0, finishedBottomSpacing=0, finishedCols=1, finishedRows=1;
    if (finishedPanQuery.next()) {
        finishedX             = finishedPanQuery.value("x").toInt();
        finishedY             = finishedPanQuery.value("y").toInt();
        finishedZ             = finishedPanQuery.value("gripperZ").toInt();
        finishedRightSpacing  = finishedPanQuery.value("rightSpacing").toInt();
        finishedBottomSpacing = finishedPanQuery.value("bottomSpacing").toInt();
        finishedCols          = finishedPanQuery.value("cols").toInt();
        finishedRows          = finishedPanQuery.value("rows").toInt();
    } else {
        qCritical() << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━";
        qCritical() << "未找到 pan_init 中 finashedPositon 记录，触发紧急暂停";
        qCritical() << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━";
        if (ui && ui->pushButton_Stop)
            QMetaObject::invokeMethod(ui->pushButton_Stop, "click", Qt::QueuedConnection);
        return;
    }

    // 从 pan_FinishedProductLocation 找 drug_name 为空的最小 slot_index
    QString finishedSlotSql = "SELECT slot_index FROM pan_FinishedProductLocation WHERE (drug_name IS NULL OR drug_name = '') ORDER BY slot_index ASC LIMIT 1";
    QSqlQuery finishedSlotQuery = dbm->query(finishedSlotSql);
    int finishedSlotIndex = -1;
    if (finishedSlotQuery.next()) {
        finishedSlotIndex = finishedSlotQuery.value("slot_index").toInt();
    } else {
        qCritical() << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━";
        qCritical() << "pan_FinishedProductLocation 中无空槽（drug_name 均非空），触发紧急暂停";
        qCritical() << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━";
        if (ui && ui->pushButton_Stop)
            QMetaObject::invokeMethod(ui->pushButton_Stop, "click", Qt::QueuedConnection);
        return;
    }

    // 校验槽位范围
    int finishedMaxSlot = finishedCols * finishedRows - 1;
    if (finishedSlotIndex < 0 || finishedSlotIndex > finishedMaxSlot) {
        qWarning() << "slot_index" << finishedSlotIndex << "超出范围 [0," << finishedMaxSlot << "]，使用 0";
        finishedSlotIndex = 0;
    }

    // 计算目标坐标
    SlotPositionConfig finishedConfig(finishedX, finishedY, finishedCols, finishedRows, finishedRightSpacing, finishedBottomSpacing);
    QPoint finishedTargetPos = calculateSlotPosition(finishedConfig, finishedSlotIndex);
    int finishedTargetX = finishedTargetPos.x();
    int finishedTargetY = finishedTargetPos.y();

    // 移动到成品区指定位置
    QString moveToTransferRightAreaXCommand = tcpCore->buildDeviceCommand("0A", "D", finishedTargetX, 8);
    messageQueue.enqueue(MessageQueueItem(moveToTransferRightAreaXCommand.toUtf8(), true, "0AD"));
    QString waitTransferRightAreaXCommand = tcpCore->buildDeviceCommand("0A", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitTransferRightAreaXCommand.toUtf8(), true, "0Ad01"));
    QString moveToTransferRightAreaYCommand = tcpCore->buildDeviceCommand("09", "D", finishedTargetY, 8);
    messageQueue.enqueue(MessageQueueItem(moveToTransferRightAreaYCommand.toUtf8(), true, "09D"));
    QString waitTransferRightAreaYCommand = tcpCore->buildDeviceCommand("09", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitTransferRightAreaYCommand.toUtf8(), true, "09d01"));
    QString moveToTransferRightAreaZCommand = tcpCore->buildDeviceCommand("06", "D", finishedZ, 8);
    messageQueue.enqueue(MessageQueueItem(moveToTransferRightAreaZCommand.toUtf8(), true, "06D"));
    QString waitTransferRightAreaZCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    messageQueue.enqueue(MessageQueueItem(waitTransferRightAreaZCommand.toUtf8(), true, "06d01"));

    // 松夹爪
    QString releaseGripperCommand = tcpCore->buildDeviceCommand("05", "06", "0105", 0, 4);
    messageQueue.enqueue(MessageQueueItem(releaseGripperCommand.toUtf8(), false));
    QString waitGripperReleaseCommand = tcpCore->buildDeviceCommand("05", "03", "0202", 1, 4);
    messageQueue.enqueue(MessageQueueItem(waitGripperReleaseCommand.toUtf8(), false, "0503020001"));
    // 上移Z轴
    // QString raiseTransferZCommand = tcpCore->buildDeviceCommand("06", "D", 100, 8);
    //tcpCore->sendMessageAsync(raiseTransferZCommand.toUtf8(), true);
    messageQueue.enqueue(MessageQueueItem(raiseTransferZCommand.toUtf8(), true, "06D"));
    // QString waitTransferZRaisedCommand = tcpCore->buildDeviceCommand("06", "d", 0, 0);
    //tcpCore->sendMessageAsync(waitTransferZRaisedCommand.toUtf8(), true, "06d01");
    messageQueue.enqueue(MessageQueueItem(waitTransferZRaisedCommand.toUtf8(), true, "06d01"));

    // 摇床启动
    QString startShakeCommand = tcpCore->buildMessageWithCrc(QStringLiteral(">0Cxi3000000012c0000012c"));
    qDebug() << "startShakeCommand" << startShakeCommand;
    //tcpCore->sendMessageAsync(startShakeCommand.toUtf8(), true, "0Cxi300");
    messageQueue.enqueue(MessageQueueItem(startShakeCommand.toUtf8(), true, "0Cxi300"));

    qDebug() << QString("摇床位置 %1 的瓶子已成功移动到成品区").arg(selfLocation);
}

// 将指定棋子移动到网格(col,row)
void MainWindow::moveChessPiece(int pieceIndex, int col, int row)
{
    if (!chessBoard) return;
    chessBoard->movePiece(pieceIndex, col, row);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (chessBoard) chessBoard->relayout();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    // 首次显示后再次自适应
    if (chessBoard) chessBoard->relayout();
}

// 初始化data.ini文件G
void MainWindow::initializeDataIni()
{
    QString iniFilePath = "BoxData.ini";
    QFileInfo fileInfo(iniFilePath);
    
    // 检查文件是否存在
    if (!fileInfo.exists()) {
        qDebug() << "BoxData.ini文件不存在，正在创建默认配置文件...";
        
        // 创建QSettings对象来写入INI文件
        QSettings settings(iniFilePath, QSettings::IniFormat);
        
        // 设置默认配置值
        settings.beginGroup("Box-Solid-Top"); // 盒子-固体-上面
        settings.setValue("axisX", "0");
        settings.setValue("axisY", "0");
        settings.setValue("gripperDepth", "0"); // 夹爪深度 /ˈɡrɪpər/
        settings.setValue("liquidExtractionDepth", "0"); // 取液深度 /ɪkˈstrækʃ(ə)n/
        settings.setValue("solidDepth", "0"); // 固体深度 /ˈsɑːlɪd/
        settings.endGroup();
        
        settings.beginGroup("Box-Solid-Bottom"); // 盒子-固体-下面
        settings.setValue("axisX", "0");
        settings.setValue("axisY", "0");
        settings.setValue("gripperDepth", "0"); // 夹爪深度
        settings.setValue("liquidExtractionDepth", "0"); // 取液深度
        settings.setValue("solidDepth", "0"); // 固体深度
        settings.endGroup();
        
        settings.beginGroup("Box-Tips-Left"); // 盒子-tips左边
        settings.setValue("axisX", "0");
        settings.setValue("axisY", "0");
        settings.setValue("gripperDepth", "0"); // 夹爪深度
        settings.setValue("liquidExtractionDepth", "0"); // 取液深度
        settings.setValue("solidDepth", "0"); // 固体深度
        settings.endGroup();
        
        settings.beginGroup("Box-Tips-Right"); // 盒子-tips右边
        settings.setValue("axisX", "0");
        settings.setValue("axisY", "0");
        settings.setValue("gripperDepth", "0"); // 夹爪深度
        settings.setValue("liquidExtractionDepth", "0"); // 取液深度
        settings.setValue("solidDepth", "0"); // 固体深度
        settings.endGroup();
        
        settings.beginGroup("Box-Shake-Bed"); // 盒子-摇床 /ʃeɪk/
        settings.setValue("axisX", "0");
        settings.setValue("axisY", "0");
        settings.setValue("gripperDepth", "0"); // 夹爪深度
        settings.setValue("liquidExtractionDepth", "0"); // 取液深度
        settings.setValue("solidDepth", "0"); // 固体深度
        settings.endGroup();
        
        settings.beginGroup("Box-Liquid-Material"); // 盒子-液体材料
        settings.setValue("axisX", "0");
        settings.setValue("axisY", "0");
        settings.setValue("gripperDepth", "0"); // 夹爪深度
        settings.setValue("liquidExtractionDepth", "0"); // 取液深度
        settings.setValue("solidDepth", "0"); // 固体深度
        settings.endGroup();
        
        settings.beginGroup("Box-Empty-Bottle"); // 盒子-空瓶
        settings.setValue("axisX", "0");
        settings.setValue("axisY", "0");
        settings.setValue("gripperDepth", "0"); // 夹爪深度
        settings.setValue("liquidExtractionDepth", "0"); // 取液深度
        settings.setValue("solidDepth", "0"); // 固体深度
        settings.endGroup();
        
        settings.beginGroup("Box-Transfer-Area-Left"); // 盒子-转移区左边
        settings.setValue("axisX", "00003A99");
        settings.setValue("axisY", "000058DF");
        settings.setValue("gripperDepth", "00041AC7"); // 夹爪深度
        settings.setValue("liquidExtractionDepth", "0"); // 取液深度
        settings.setValue("solidDepth", "0"); // 固体深度
        settings.endGroup();
        
        settings.beginGroup("Box-Transfer-Area-Right"); // 盒子-转移区右边
        settings.setValue("axisX", "0");
        settings.setValue("axisY", "0");
        settings.setValue("gripperDepth", "0"); // 夹爪深度
        settings.setValue("liquidExtractionDepth", "0"); // 取液深度
        settings.setValue("solidDepth", "0"); // 固体深度
        settings.endGroup();
        
        settings.beginGroup("Box-Hold-Region");  // 夹持区域 /ˈriːdʒən/
        settings.setValue("axisX", "20326");
        settings.setValue("axisY", "35517");
        settings.setValue("gripperDepth", "265192"); // 夹爪深度
        settings.setValue("liquidExtractionDepth", "0"); // 取液深度
        settings.setValue("solidDepth", "0"); // 固体深度
        settings.endGroup();

        settings.beginGroup("TCP-Info");
        settings.setValue("LocalIP", "192.168.5.22");        // 本机IP
        settings.setValue("RemoteIP", "192.168.5.201");      // 远端IP
        settings.setValue("RemotePort", "4196");      // 远端端口
        settings.setValue("ProxyDisabled", true); // 是否禁用代理
        settings.endGroup();

        settings.beginGroup("Liquid-Info");
        settings.setValue("LocalIP", "192.168.5.22");
        settings.setValue("RemoteIP", "192.168.5.201");
        settings.setValue("RemotePort", "4196");
        settings.setValue("ProxyDisabled", true); // 是否禁用代理
        settings.endGroup();

        // 流程步骤选中状态（默认全部勾选）
        settings.beginGroup("Process-Steps");
        settings.setValue("reset", true);           // 复位
        settings.setValue("takeEmptyBottle", true); // 取空瓶
        settings.setValue("getSolid", true);        // 获取固体
        settings.setValue("resetXYZ", true);        // 重置XYZ
        settings.setValue("getLiquid", true);       // 获取液体
        settings.setValue("capBottleAndTransferToShaker", true);   // 拧盖并送入摇床
        settings.endGroup();

        // 确保文件被写入磁盘
        settings.sync();

        qDebug() << "data.ini文件创建成功，默认配置已写入";
    } else {
        qDebug() << "data.ini文件已存在，跳过初始化";
    }
}

// 流程步骤定义（顺序即执行顺序）
static const QStringList PROCESS_STEPS = {
    "reset",
    "takeEmptyBottle",
    "getSolid",
    "resetXYZ",
    "getLiquid",
    "capBottleAndTransferToShaker"
};

// 保存流程步骤的选中状态到ini文件
void MainWindow::saveProcessStepsState()
{
    QSettings settings("BoxData.ini", QSettings::IniFormat);
    settings.beginGroup("Process-Steps");

    if (m_processCheckBox_reset)
        settings.setValue("reset", m_processCheckBox_reset->isChecked());
    if (m_processCheckBox_takeEmptyBottle)
        settings.setValue("takeEmptyBottle", m_processCheckBox_takeEmptyBottle->isChecked());
    if (m_processCheckBox_getSolid)
        settings.setValue("getSolid", m_processCheckBox_getSolid->isChecked());
    if (m_processCheckBox_resetXYZ)
        settings.setValue("resetXYZ", m_processCheckBox_resetXYZ->isChecked());
    if (m_processCheckBox_getLiquid)
        settings.setValue("getLiquid", m_processCheckBox_getLiquid->isChecked());
    if (m_processCheckBox_tightenBottle)
        settings.setValue("capBottleAndTransferToShaker", m_processCheckBox_tightenBottle->isChecked());

    settings.endGroup();
    settings.sync();
}

// 从ini文件加载流程步骤的选中状态
void MainWindow::loadProcessStepsState()
{
    QSettings settings("BoxData.ini", QSettings::IniFormat);
    settings.beginGroup("Process-Steps");

    if (m_processCheckBox_reset)
        m_processCheckBox_reset->setChecked(settings.value("reset", true).toBool());
    if (m_processCheckBox_takeEmptyBottle)
        m_processCheckBox_takeEmptyBottle->setChecked(settings.value("takeEmptyBottle", true).toBool());
    if (m_processCheckBox_getSolid)
        m_processCheckBox_getSolid->setChecked(settings.value("getSolid", true).toBool());
    if (m_processCheckBox_resetXYZ)
        m_processCheckBox_resetXYZ->setChecked(settings.value("resetXYZ", true).toBool());
    if (m_processCheckBox_getLiquid)
        m_processCheckBox_getLiquid->setChecked(settings.value("getLiquid", true).toBool());
    if (m_processCheckBox_tightenBottle)
        m_processCheckBox_tightenBottle->setChecked(settings.value("capBottleAndTransferToShaker", true).toBool());

    settings.endGroup();
}

void MainWindow::resetProcessStateDisplay()
{
    m_completedSteps.clear();
    m_skippedSteps.clear();
    const QString grayStyle = "color: gray;";
    if (m_processCheckBox_reset)           m_processCheckBox_reset->setStyleSheet(grayStyle);
    if (m_processCheckBox_takeEmptyBottle) m_processCheckBox_takeEmptyBottle->setStyleSheet(grayStyle);
    if (m_processCheckBox_getSolid)        m_processCheckBox_getSolid->setStyleSheet(grayStyle);
    if (m_processCheckBox_resetXYZ)        m_processCheckBox_resetXYZ->setStyleSheet(grayStyle);
    if (m_processCheckBox_getLiquid)       m_processCheckBox_getLiquid->setStyleSheet(grayStyle);
    if (m_processCheckBox_tightenBottle)   m_processCheckBox_tightenBottle->setStyleSheet(grayStyle);
}

void MainWindow::updateProcessStateDisplay(const QString& stateName)
{
    // 状态名称 -> 对应的 QCheckBox 指针
    QMap<QString, QCheckBox*> checkBoxMap = {
        {"reset",           m_processCheckBox_reset},
        {"takeEmptyBottle", m_processCheckBox_takeEmptyBottle},
        {"getSolid",        m_processCheckBox_getSolid},
        {"resetXYZ",        m_processCheckBox_resetXYZ},
        {"getLiquid",       m_processCheckBox_getLiquid},
        {"capBottleAndTransferToShaker",   m_processCheckBox_tightenBottle}
    };

    const QString doneStyle    = "color: green; font-weight: bold;";
    const QString currentStyle = "color: #00aa00; font-weight: bold; text-decoration: underline;";
    const QString pendingStyle = "color: gray;";
    const QString skippedStyle = "color: gray; text-decoration: line-through;";

    // 渲染规则（按优先级）：
    // 1. skipped（灰+删除线）- 最高优先级
    // 2. current（绿+下划线）
    // 3. completed（绿加粗）
    // 4. selected-but-not-run-yet（灰，无删除线）
    // 5. unselected（灰+删除线）
    for (const QString& step : PROCESS_STEPS) {
        QCheckBox* checkBox = checkBoxMap.value(step, nullptr);
        if (!checkBox) continue;

        if (m_skippedSteps.contains(step)) {
            // 已跳过：灰色+删除线
            checkBox->setStyleSheet(skippedStyle);
        } else if (step == stateName && !stateName.isEmpty()) {
            // 当前执行：绿色加粗+下划线
            checkBox->setStyleSheet(currentStyle);
        } else if (m_completedSteps.contains(step)) {
            // 已完成：绿色加粗
            checkBox->setStyleSheet(doneStyle);
        } else if (m_selectedSteps.contains(step)) {
            // 已选中但未执行：灰色（无删除线）
            checkBox->setStyleSheet(pendingStyle);
        } else {
            // 未选中：灰色+删除线
            checkBox->setStyleSheet(skippedStyle);
        }
    }
}

// 处理流程状态变更（接收 TcpClientCore 的 processStateChanged 信号）
void MainWindow::onProcessStateChanged(const QString& stateName)
{
    qDebug() << "流程状态变更:" << stateName;

    // 如果存在当前步骤，且它属于本次选中且未被跳过，则标记为已完成
    if (!m_currentStep.isEmpty()
        && m_selectedSteps.contains(m_currentStep)
        && !m_skippedSteps.contains(m_currentStep)) {
        m_completedSteps.insert(m_currentStep);
    }

    // 更新当前步骤
    m_currentStep = stateName;

    // 刷新UI显示
    updateProcessStateDisplay(stateName);
}

// 处理步骤跳过（接收 TcpClientCore 的 stepSkipped 信号）
void MainWindow::onStepSkipped(const QString& stepName)
{
    qDebug() << "步骤已跳过:" << stepName;

    // 将步骤加入跳过集合
    m_skippedSteps.insert(stepName);

    // 如果跳过的是当前步骤，清空当前步骤
    if (m_currentStep == stepName) {
        m_currentStep.clear();
    }

    // 刷新UI显示
    updateProcessStateDisplay(m_currentStep);
}





