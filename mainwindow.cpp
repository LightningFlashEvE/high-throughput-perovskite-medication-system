#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QTimer>
#include <QDateTime>
#include <QGraphicsView>
#include <QResizeEvent>
#include <QShowEvent>
#include <QPainter>
#include <algorithm> // for std::clamp

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

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
    }
    chessBoard = new ChessBoardView(this);
    chessBoard->init(ui->graphicsView);

    /*** 初始化流程视图（封装到 FlowViewManager，挂载 frame_2） ***/
    if (ui->frame_2) {
        ui->frame_2->setMinimumSize(450, 440);
    }
    flowManager = new FlowViewManager(this);
    flowManager->init(ui->frame_2);

    /*** 绑定测试按钮 - 启动/暂停 TCP 接收并解析 ***/
    tcpClient = new TcpFramedClient(this);
    
    // 设置字节序（true=大端序，false=小端序）
    // 如果你的数据是 AA 55 格式（大端），设置为 true
    // 如果你的数据是 55 AA 格式（小端），设置为 false
    tcpClient->setByteOrder(true);  // 使用大端序
    
    connect(tcpClient, &TcpFramedClient::frameReceived, this, [](quint16 type, const QByteArray &payload){
        qDebug() << "[TCP] frame type=" << type << ", len=" << payload.size();
    });
    connect(tcpClient, &TcpFramedClient::logMessage, this, [](const QString &msg){ qDebug() << msg; });
    connect(tcpClient, &TcpFramedClient::errorOccurred, this, [](const QString &msg){ qWarning() << msg; });

    // 测试按键
    if (ui->pushButtonTest) {
        connect(ui->pushButtonTest, &QPushButton::clicked, this, [this]{
            static bool receiving = false;
            receiving = !receiving;
            if (receiving) {
                ui->pushButtonTest->setText(QStringLiteral("暂停接收"));
                tcpClient->start(QStringLiteral("127.0.0.1"), 9000);
            } else {
                ui->pushButtonTest->setText(QStringLiteral("开始接收"));
                tcpClient->stop();
            }
        });
        ui->pushButtonTest->setText(QStringLiteral("开始接收"));
    }

    // 设置按钮：频繁打开/关闭 SettingsButton 窗口
    if (ui->pushButtonSetting) {
        connect(ui->pushButtonSetting, &QPushButton::clicked, this, [this] {
            if (!settingsPanel) {
                settingsPanel = new SettingsButton(nullptr); // 独立窗口
                settingsPanel->setAttribute(Qt::WA_DeleteOnClose, true);
                settingsPanel->setWindowFlag(Qt::Window, true);
                connect(settingsPanel, &QObject::destroyed, this, [this] { settingsPanel = nullptr; });

                // 方向按键信号 -> 移动 zhua
                const int step = 1; // 每次移动一个网格
                connect(settingsPanel, &SettingsButton::moveUpClicked, this, [this] {
                    int maxRow = chessBoard ? chessBoard->gridMaxRow() : 99;
                    int maxCol = chessBoard ? chessBoard->gridMaxCol() : 99;
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
            }
            settingsPanel->show();
            settingsPanel->raise();
            settingsPanel->activateWindow();
            // 打开时同步显示当前位置
            settingsPanel->setLocation(zhuaCol, zhuaRow);

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
        });
    }


    // 状态按键
    if (ui->pushButtonStatus) {
        connect(ui->pushButtonStatus, &QPushButton::clicked, this, []{
            qDebug() << "状态点击";
        });
    }
}

MainWindow::~MainWindow()
{
    delete ui;
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
