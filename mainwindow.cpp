#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QTimer>
#include <QDateTime>
#include <QGraphicsView>
#include <QResizeEvent>
#include <QShowEvent>
#include <QCloseEvent>
#include <QPainter>
#include <algorithm> // for std::clamp
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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

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

    /*** 初始化流程视图（封装到 FlowViewManager，挂载 frame_2） ***/
    if (ui->frame_2) {
        ui->frame_2->setMinimumSize(450, 440);
        // 让 frame_2 背景随主题，由样式/调色板统一控制
        ui->frame_2->setAttribute(Qt::WA_StyledBackground, true);
        ui->frame_2->setStyleSheet("background-color: palette(window);");
    }
    flowManager = new FlowViewManager(this);
    flowManager->init(ui->frame_2);


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
    }

    if (ui->menuHistory) {
        QAction *sthAction = new QAction("历史按键", this);
        ui->menuHistory->addAction(sthAction);
        connect(sthAction, &QAction::triggered, this, [] {
            qDebug() << "xixihaha";
        });
    }



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

void MainWindow::cleanupResources()
{
    // 停止定时器
    if (timer) {
        timer->stop();
        timer->disconnect(); // 断开所有信号连接
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
    
    // 清理棋盘和流程图（它们是 this 的子对象，会自动清理）
    // 但为了确保，我们可以显式停止它们
    if (chessBoard) {
        chessBoard = nullptr; // 是 this 的子对象，会在析构时自动删除
    }
    
    if (flowManager) {
        flowManager = nullptr; // 是 this 的子对象，会在析构时自动删除
    }
    
    // 最后处理一次事件，确保所有删除操作完成
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 100);
    
    qDebug() << "资源清理完成";
}

// 每秒刷新日期与时间显示
void MainWindow::updateTime()
{
    QDateTime currentDateTime = QDateTime::currentDateTime();

    // 更新日期显示 (格式: 2025.10.1)DD
    QString dateStr = currentDateTime.toString("yyyy.M.d");
    ui->labelDate->setText(dateStr);

    // 更新时间显示 (格式: 12:33:21)
    QString timeStr = currentDateTime.toString("hh:mm:ss");
    ui->labelTime->setText(timeStr);
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

        // 确保文件被写入磁盘
        settings.sync();
        
        qDebug() << "data.ini文件创建成功，默认配置已写入";
    } else {
        qDebug() << "data.ini文件已存在，跳过初始化";
    }
}
