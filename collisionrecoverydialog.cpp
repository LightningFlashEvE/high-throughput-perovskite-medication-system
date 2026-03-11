#include "collisionrecoverydialog.h"
#include "ui_collisionrecoverydialog.h"
#include "mainwindow.h"
#include <QDebug>
#include <QSlider>
#include <QTime>

CollisionRecoveryDialog::CollisionRecoveryDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CollisionRecoveryDialog)
    , sliderValueLabel(nullptr)
    , m_tcpCore(nullptr)
    , m_tcpBalanceCore(nullptr)
{
    ui->setupUi(this);
    setWindowTitle("碰撞恢复");
    
    // 从MainWindow单例获取TCP对象并缓存
    MainWindow *mainWindow = MainWindow::getInstance();
    if (mainWindow) {
        m_tcpCore = mainWindow->getTcpCore();
        m_tcpBalanceCore = mainWindow->getTcpBalanceCore();
    }
    
    // 初始化滑块值显示标签
    if (ui->horizontalSliderTight) {
        sliderValueLabel = new QLabel(this);
        sliderValueLabel->setStyleSheet(
            "QLabel {"
            "  background-color: rgba(0, 0, 0, 200);"
            "  color: white;"
            "  border-radius: 4px;"
            "  padding: 2px 6px;"
            "  font-size: 12px;"
            "}"
        );
        sliderValueLabel->setAlignment(Qt::AlignCenter);
        sliderValueLabel->hide();  // 初始隐藏
        
        // 更新标签位置的辅助函数
        auto updateLabelPosition = [this](int value) {
            if (!sliderValueLabel || !ui->horizontalSliderTight) return;
            
            QSlider *slider = ui->horizontalSliderTight;
            
            // 更新标签文本
            sliderValueLabel->setText(QString::number(value));
            sliderValueLabel->adjustSize();
            
            // 计算滑块值对应的位置比例
            double ratio = 0.0;
            if (slider->maximum() != slider->minimum()) {
                ratio = (value - slider->minimum()) / double(slider->maximum() - slider->minimum());
            }
            
            // 获取滑块的几何位置（相对于父控件）
            QRect sliderRect = slider->geometry();
            
            // 估算滑块的可用宽度（考虑左右边距，通常QSlider左右各留约12-15像素）
            const int margin = 15;  // 滑块左右边距
            int availableWidth = sliderRect.width() - 2 * margin;
            
            // 计算滑块拖动点在滑块控件内的相对X坐标
            int handleXRelative = margin + int(ratio * availableWidth);
            
            // 将滑块内的相对坐标转换为窗口坐标
            QPoint sliderLocalPos(handleXRelative, sliderRect.height() / 2);
            QPoint globalPos = slider->mapToGlobal(sliderLocalPos);
            QPoint windowPos = this->mapFromGlobal(globalPos);
            
            // 设置标签位置（在滑块上方居中）
            int labelX = windowPos.x() - sliderValueLabel->width() / 2;
            int labelY = windowPos.y() - sliderRect.height() / 2 - sliderValueLabel->height() - 8;  // 在滑块上方8像素
            
            sliderValueLabel->move(labelX, labelY);
            sliderValueLabel->show();
        };
        
        // 当开始拖动时显示标签
        connect(ui->horizontalSliderTight, &QSlider::sliderPressed, this, [this, updateLabelPosition]() {
            if (sliderValueLabel && ui->horizontalSliderTight) {
                updateLabelPosition(ui->horizontalSliderTight->value());
            }
        });
        
        // 拖动时更新标签位置和文本
        connect(ui->horizontalSliderTight, &QSlider::valueChanged, this, [updateLabelPosition](int value) {
            updateLabelPosition(value);
        });
        
        // 当滑块停止拖动时延迟隐藏标签
        connect(ui->horizontalSliderTight, &QSlider::sliderReleased, this, [this]() {
            if (sliderValueLabel) {
                QTimer::singleShot(1000, this, [this]() {  // 1秒后隐藏
                    if (sliderValueLabel) {
                        sliderValueLabel->hide();
                    }
                });
            }
        });
    }
    
    // 连接 pushButton_Enable 按钮
    connect(ui->pushButton_Enable, &QPushButton::clicked, this, [=]() {
        // TODO: 在这里添加你的逻辑
    });
    
    // 连接 pushButton_Release 按钮
    connect(ui->pushButton_Release, &QPushButton::clicked, this, [=]() {
        // 1号 
    });

    // 手动连接按钮点击信号到对应槽函数（去掉 on_ 前缀后的命名）
    if (ui->pushButton_Tight) {
        connect(ui->pushButton_Tight, &QPushButton::clicked,
                this, &CollisionRecoveryDialog::pushButton_Tight_clicked);
    }
    if (ui->pushButton_xMove) {
        connect(ui->pushButton_xMove, &QPushButton::clicked,
                this, &CollisionRecoveryDialog::pushButton_xMove_clicked);
    }
    if (ui->pushButton_yMove) {
        connect(ui->pushButton_yMove, &QPushButton::clicked,
                this, &CollisionRecoveryDialog::pushButton_yMove_clicked);
    }
    if (ui->pushButton_zMove) {
        connect(ui->pushButton_zMove, &QPushButton::clicked,
                this, &CollisionRecoveryDialog::pushButton_zMove_clicked);
    }
    if (ui->pushButton_Liquid) {
        connect(ui->pushButton_Liquid, &QPushButton::clicked,
                this, &CollisionRecoveryDialog::pushButton_Liquid_clicked);
    }
    if (ui->pushButton_Magnet) {
        connect(ui->pushButton_Magnet, &QPushButton::clicked,
                this, &CollisionRecoveryDialog::pushButton_Magnet_clicked);
    }
}

CollisionRecoveryDialog::~CollisionRecoveryDialog()
{
    if (sliderValueLabel) {
        delete sliderValueLabel;
        sliderValueLabel = nullptr;
    }
    delete ui;
}

void CollisionRecoveryDialog::printDebug(const QString& message)
{
    if (ui->label_debug) {
        QString currentTime = QTime::currentTime().toString("HH:mm:ss");
        QString displayMessage = QString("%1  --%2").arg(message).arg(currentTime);
        ui->label_debug->setText(displayMessage);
    }
}

bool CollisionRecoveryDialog::checkTcpConnection()
{
    // 检查 tcpCore 对象是否存在
    if (!m_tcpCore) {
        printDebug("TCP核心对象未初始化，无法发送命令");
        return false;
    }
    
    // 检查网络是否连接
    if (!m_tcpCore->isConnected()) {
        printDebug("TCP未连接，无法发送命令");
        return false;
    }
    
    return true;
}

void CollisionRecoveryDialog::pushButton_Tight_clicked()
{
    // 检查TCP连接状态
    if (!checkTcpConnection()) {
        return;
    }

    int openAngle = ui->horizontalSliderTight->value();
    QString tightType = ui->comboBoxTight->currentText();

    if (tightType == "移动夹爪") {
        QString gripperCommand = m_tcpCore->buildDeviceCommand("05", "06", "0105", openAngle, 4);
        m_tcpCore->sendMessage(gripperCommand.toUtf8(), false);
        printDebug(QString("发送移动夹爪命令，角度：%1").arg(openAngle));
    } else if (tightType == "固定夹爪") {
        QString gripperCommand = m_tcpCore->buildDeviceCommand("0B", "06", "0105", openAngle, 4);
        m_tcpCore->sendMessage(gripperCommand.toUtf8(), false);
        printDebug(QString("发送固定夹爪命令，角度：%1").arg(openAngle));
    }
}

void CollisionRecoveryDialog::pushButton_xMove_clicked()
{
    // 检查TCP连接状态
    if (!checkTcpConnection()) {
        return;
    }

    // 获取comboBox_xMove里面的内容是左侧还是右侧
    QString xMoveDirection = ui->comboBox_xMove->currentText();
    if (xMoveDirection == "左侧") {
        QString xMoveCommand = m_tcpCore->buildDeviceCommand("04", "G", 0, 0); // 归零
        m_tcpCore->sendMessage(xMoveCommand.toUtf8(), false);
        printDebug("发送X轴左侧归零命令");
    } else if (xMoveDirection == "右侧") {
        QString xMoveCommand = m_tcpCore->buildDeviceCommand("04", "G", 0, 0); // 归零
        m_tcpCore->sendMessage(xMoveCommand.toUtf8(), false);
        printDebug("发送X轴右侧归零命令");
    }
}

void CollisionRecoveryDialog::pushButton_yMove_clicked()
{
    // 检查TCP连接状态
    if (!checkTcpConnection()) {
        return;
    }

    // 获取comboBox_yMove里面的内容是左侧还是右侧
    QString yMoveDirection = ui->comboBox_yMove->currentText();
    if (yMoveDirection == "左侧") {
        // 3号电机左侧初始化
        QString yMoveCommand = m_tcpCore->buildDeviceCommand("03", "G", 0, 0); // 归零
        m_tcpCore->sendMessage(yMoveCommand.toUtf8(), false);
        printDebug("发送Y轴左侧(3号电机)归零命令");
    } else if (yMoveDirection == "右侧") {
        // 9号电机右侧初始化
        QString yMoveCommand = m_tcpCore->buildDeviceCommand("09", "G", 0, 0); // 归零
        m_tcpCore->sendMessage(yMoveCommand.toUtf8(), false);
        printDebug("发送Y轴右侧(9号电机)归零命令");
    }
}

void CollisionRecoveryDialog::pushButton_zMove_clicked()
{
    // 检查TCP连接状态
    if (!checkTcpConnection()) {
        return;
    }

    // 获取comboBox_zMove里面的内容是右侧移动夹爪,右侧移液器,左侧固体
    QString zMoveDirection = ui->comboBox_zMove->currentText();
    if (zMoveDirection == "右侧移动夹爪") {
        // 6号电机右侧移动夹爪  
        QString zMoveCommand = m_tcpCore->buildDeviceCommand("06", "G", 0, 0); // 归零
        m_tcpCore->sendMessage(zMoveCommand.toUtf8(), false);
        printDebug("发送Z轴右侧移动夹爪(6号电机)归零命令");
    } else if (zMoveDirection == "右侧移液器") {
        // 8号电机右侧移液器
        QString zMoveCommand = m_tcpCore->buildDeviceCommand("08", "G", 0, 0); // 归零
        m_tcpCore->sendMessage(zMoveCommand.toUtf8(), false);
        printDebug("发送Z轴右侧移液器(8号电机)归零命令");
    } else if (zMoveDirection == "左侧固体") {
        // 2号电机左侧固体
        QString zMoveCommand = m_tcpCore->buildDeviceCommand("02", "G", 0, 0); // 归零
        m_tcpCore->sendMessage(zMoveCommand.toUtf8(), false);
        printDebug("发送Z轴左侧固体(2号电机)归零命令");
    }
}

void CollisionRecoveryDialog::pushButton_Liquid_clicked()
{
    // 检查TCP连接状态
    if (!checkTcpConnection()) {
        return;
    }

    // 移液枪闭合释放：发送07号设备的释放命令
    // 使用Q命令释放tip head（丢弃tip head命令）
    QString releaseCommand = m_tcpCore->buildDeviceCommand("07", "Q", 0, 0);
    m_tcpCore->sendMessage(releaseCommand.toUtf8(), true);
    printDebug("发送移液枪释放命令(07号设备)");
}

void CollisionRecoveryDialog::pushButton_Magnet_clicked()
{
    // 磁吸闭合释放：断开天平接收
    if (!m_tcpBalanceCore) {
        printDebug("TCP天平核心对象未初始化，无法断开接收");
        return;
    }

}














