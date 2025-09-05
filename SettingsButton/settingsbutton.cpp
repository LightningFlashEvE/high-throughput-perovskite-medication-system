#include "settingsbutton.h"
#include "ui_settingsbutton.h"
#include <QPainter> // for 试管状态机
#include <QPaintEvent> // for 试管状态机
#include <QTextEdit>
#include <QScrollArea>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QSlider>
#include "Modbus485/modbus485.h"

SettingsButton::SettingsButton(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SettingsButton)
{
    ui->setupUi(this);

    // 连接方向按键 -> 发出对应信号
    if (ui->pushButtonUp)    connect(ui->pushButtonUp,    &QPushButton::clicked, this, &SettingsButton::moveUpClicked);
    if (ui->pushButtonRight) connect(ui->pushButtonRight, &QPushButton::clicked, this, &SettingsButton::moveRightClicked);
    if (ui->pushButtonLeft)  connect(ui->pushButtonLeft,  &QPushButton::clicked, this, &SettingsButton::moveLeftClicked);
    if (ui->pushButtonDown)  connect(ui->pushButtonDown,  &QPushButton::clicked, this, &SettingsButton::moveDownClicked);

    /******  试管状态机 ******/
    // 初始化一个试管，中心(50,50)，半径8
    Tube t; t.center = QPointF(50, 50); t.radius = 8; t.state = TubeState::Empty;
    m_tubes = { t };

    // 坐标编辑框：回车或失焦时发送 positionEdited(col,row)
    if (auto x = findChild<QLineEdit*>("lineEditX")) {
        connect(x, &QLineEdit::editingFinished, this, [this]{
            auto xEdit = findChild<QLineEdit*>("lineEditX");
            auto yEdit = findChild<QLineEdit*>("lineEditY");
            if (!xEdit || !yEdit) return;
            bool okx=false, oky=false; int col = xEdit->text().toInt(&okx); int row = yEdit->text().toInt(&oky);
            if (okx && oky) emit positionEdited(col, row);
        });
    }
    if (auto y = findChild<QLineEdit*>("lineEditY")) {
        connect(y, &QLineEdit::editingFinished, this, [this]{
            auto xEdit = findChild<QLineEdit*>("lineEditX");
            auto yEdit = findChild<QLineEdit*>("lineEditY");
            if (!xEdit || !yEdit) return;
            bool okx=false, oky=false; int col = xEdit->text().toInt(&okx); int row = yEdit->text().toInt(&oky);
            if (okx && oky) emit positionEdited(col, row);
        });
    }

    /****** 485通信初始化 ******/
    init485UI();
}

SettingsButton::~SettingsButton()
{
    delete ui;
}

void SettingsButton::LED(bool changeColor) // 灯
{
    if(changeColor)
    {
        // 显示绿色
        ui->LED->setStyleSheet("background-color: qradialgradient(spread:pad, cx:0.5, cy:0.5, radius:0.5, fx:0.5, fy:0.5, stop:0 rgba(0, 229, 0, 255), stop:1 rgba(255, 255, 255, 255));border-radius:12px;");
    }
    else
    {
        // 显示红色
        ui->LED->setStyleSheet("background-color: qradialgradient(spread:pad, cx:0.5, cy:0.5, radius:0.5, fx:0.5, fy:0.5, stop:0 rgba(255, 0, 0, 255), stop:1 rgba(255, 255, 255, 255));border-radius:12px;");
    }
}

void SettingsButton::setLocation(int col, int row)
{
    if (auto x = findChild<QLineEdit*>("lineEditX")) x->setText(QString::number(col));
    if (auto y = findChild<QLineEdit*>("lineEditY")) y->setText(QString::number(row));
}

/******  试管状态机  up ******/
void SettingsButton::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // 简化样式：根据状态设定笔刷
    auto styleOf = [](TubeState s){
        struct TubeStyle { QBrush fill; QPen border; QString mark; };
        static const TubeStyle S[] = {
            {Qt::NoBrush,            QPen(QColor("#d9534f"), 2), QStringLiteral("×")},
            {QColor("#5cb85c"),     QPen(QColor("#2e7d32"), 1), QString()},
            {QColor(92,184,92,120),  QPen(QColor("#0275d8"), 2), QStringLiteral("•")},
            {QColor(217,83,79,60),   QPen(QColor("#d9534f"), 3), QStringLiteral("!")},
            {QColor(0,0,0,20),       QPen(QColor("#9e9e9e"), 1, Qt::DashLine), QString()}
        };
        return S[static_cast<int>(s)];
    };

    for (const Tube &t : m_tubes) {
        const auto st = styleOf(t.state);
        p.setPen(st.border);
        p.setBrush(st.fill);
        QRectF r(t.center.x()-t.radius, t.center.y()-t.radius, 2*t.radius, 2*t.radius);
        p.drawEllipse(r);
        if (!st.mark.isEmpty()) {
            QFont f = p.font(); f.setBold(true); p.setFont(f);
            p.drawText(r, Qt::AlignCenter, st.mark);
        }
    }
}

void SettingsButton::setTubeState(int idx, TubeState s)
{
    if (idx < 0 || idx >= m_tubes.size()) return;
    if (m_tubes[idx].state == s) return;
    m_tubes[idx].state = s;
    update();
}

QRect SettingsButton::tubeRect(int idx) const
{
    if (idx < 0 || idx >= m_tubes.size()) return QRect();
    const Tube &t = m_tubes[idx];
    return QRect(qRound(t.center.x()-t.radius), qRound(t.center.y()-t.radius), qRound(2*t.radius), qRound(2*t.radius));
}

/******  试管状态机 down ******/

/****** 485通信实现  up ******/
void SettingsButton::init485UI()
{
    // 初始化Modbus485实例
    modbus485 = new Modbus485(this);
    
    // 连接485信号
    connect(modbus485, &Modbus485::connectionStatusChanged, this, &SettingsButton::onSerialConnectionChanged);
    connect(modbus485, &Modbus485::dataReceived, this, &SettingsButton::onSerialDataReceived);
    connect(modbus485, &Modbus485::errorOccurred, this, &SettingsButton::onSerialError);
    
    // 转发485信号到公共接口
    connect(modbus485, &Modbus485::connectionStatusChanged, this, &SettingsButton::serial485Connected);
    connect(modbus485, &Modbus485::dataReceived, this, [this](const QByteArray &data){
        emit serial485DataReceived(data);
        emit serial485HexReceived(QString(data.toHex().toUpper()));
    });
    connect(modbus485, &Modbus485::errorOccurred, this, &SettingsButton::serial485Error);
    
    // 在滚动区域里放置文本框用于显示日志
    m_serverLog = new QTextEdit(ui->scrollAreaServer);
    m_serverLog->setReadOnly(false);  // 可编辑
    m_serverLog->setAcceptRichText(false);
    ui->scrollAreaServer->setWidget(m_serverLog);
    
    // 初始化串口列表
    if (auto comboBox = findChild<QComboBox*>("comboBoxCOM")) {
        comboBox->clear();
        comboBox->addItems(modbus485->availablePorts());
        comboBox->setCurrentText("COM8");
    }
    
    // 初始化波特率列表
    if (auto comboBox = findChild<QComboBox*>("comboBoxRate")) {
        comboBox->clear();
        comboBox->addItems({"9600", "19200", "38400", "57600", "115200"});
        comboBox->setCurrentText("115200");
    }
    
    // 连接按钮
    if (auto button = findChild<QPushButton*>("pushButton_2")) {  // 根据UI文件，连接按钮是pushButton_2
        connect(button, &QPushButton::clicked, this, &SettingsButton::onConnectClicked);
    }
    
    // 发送按钮
    if (auto button = findChild<QPushButton*>("pushButton")) {    // 发送按钮是pushButton
        connect(button, &QPushButton::clicked, this, [this]{
            if (auto textEdit = findChild<QTextEdit*>("textEditServer")) {
                QString text = textEdit->toPlainText().trimmed();
                QByteArray hexData;
                QString sourceInfo;
                
                if (!text.isEmpty()) {
                    // textEditServer不为空，发送其中的内容
                    text = text.replace(" ", "");
                    hexData = QByteArray::fromHex(text.toLatin1());
                    
                    if (!hexData.isEmpty()) {
                        // 自动补全CRC
                        quint16 crc = calculateCRC16(hexData);
                        hexData.append(static_cast<char>(crc & 0xFF));
                        hexData.append(static_cast<char>((crc >> 8) & 0xFF));
                        sourceInfo = QString("textEdit内容");
                    }
                } else {
                    // textEditServer为空，从输入框拼接数据
                    auto idEdit = findChild<QLineEdit*>("lineEditID");
                    auto funcEdit = findChild<QLineEdit*>("lineEditFunc");
                    auto addrEdit = findChild<QLineEdit*>("lineEditAddress");
                    auto dataEdit = findChild<QLineEdit*>("lineEditData");
                    auto crcEdit = findChild<QLineEdit*>("lineEditCRC16");
                    
                    if (idEdit && funcEdit && addrEdit && dataEdit && crcEdit) {
                        QString combined = idEdit->text() + funcEdit->text() + addrEdit->text() + dataEdit->text() + crcEdit->text();
                        combined = combined.replace(" ", "");
                        hexData = QByteArray::fromHex(combined.toLatin1());
                        sourceInfo = QString("输入框拼接");
                    }
                }
                
                if (!hexData.isEmpty() && modbus485) {
                    // 发送数据
                    QString fullHex = QString(hexData.toHex().toUpper());
                    for (int i = fullHex.length() - 2; i > 0; i -= 2) {
                        fullHex.insert(i, ' ');
                    }
                    
                    if (m_serverLog) {
                        m_serverLog->append(QString("[发送] %1 (%2)").arg(fullHex).arg(sourceInfo));
                    }
                    
                    modbus485->sendData(hexData);
                    
                    // 清空textEditServer（如果有内容的话）
                    if (!text.isEmpty()) {
                        //textEdit->clear();
                    }
                } else {
                    if (m_serverLog) {
                        m_serverLog->append(QString("[错误] 无有效数据可发送"));
                    }
                }
            }
        });
    }
    
    // 连接Modbus输入框的变化信号，自动更新CRC
    if (auto lineEdit = findChild<QLineEdit*>("lineEditID")) {
        connect(lineEdit, &QLineEdit::textChanged, this, &SettingsButton::onModbusDataChanged);
    }
    if (auto lineEdit = findChild<QLineEdit*>("lineEditFunc")) {
        connect(lineEdit, &QLineEdit::textChanged, this, &SettingsButton::onModbusDataChanged);
    }
    if (auto lineEdit = findChild<QLineEdit*>("lineEditAddress")) {
        connect(lineEdit, &QLineEdit::textChanged, this, &SettingsButton::onModbusDataChanged);
    }
    if (auto lineEdit = findChild<QLineEdit*>("lineEditData")) {
        connect(lineEdit, &QLineEdit::textChanged, this, &SettingsButton::onModbusDataChanged);
    }

    // 初始化LED状态
    LED(false);
    
    // 初始计算一次CRC（如果输入框已有内容）
    onModbusDataChanged();
    
    // 初始化旋转角度滑动条
    initRotationSlider();
}

void SettingsButton::onConnectClicked()
{
    if (!modbus485) return;
    
    if (modbus485->isPortOpen()) {
        // 当前已连接，执行断开
        modbus485->closePort();
    } else {
        // 当前未连接，执行连接
        auto comPort = findChild<QComboBox*>("comboBoxCOM");
        auto baudRate = findChild<QComboBox*>("comboBoxRate");
        
        if (comPort && baudRate) {
            QString port = comPort->currentText();
            int baud = baudRate->currentText().toInt();
            
            if (!port.isEmpty()) {
                modbus485->openPort(port, baud);
            } else {
                onSerialError("请选择串口");
            }
        }
    }
}

void SettingsButton::onSerialDataReceived(const QByteArray &data)
{
    // 接收到的原始字节数据直接以16进制显示
    if (m_serverLog) {
        m_serverLog->append(QString("[接收] %1").arg(QString(data.toHex().toUpper())));
    }
}

void SettingsButton::onSerialConnectionChanged(bool connected)
{
    // 更新LED状态
    LED(connected);
    
    // 更新连接按钮文本
    if (auto button = findChild<QPushButton*>("pushButton_2")) {
        button->setText(connected ? "断开" : "连接");
    }
    
    // 根据连接状态启用/禁用旋转滑动条
    if (auto slider = findChild<QSlider*>("horizontalSliderRotationAngle")) {
        slider->setEnabled(connected);
    }
    
    // 更新状态信息
    onSerialError(connected ? "串口连接成功" : "串口已断开");
}

void SettingsButton::onSerialError(const QString &error)
{
    if (m_serverLog) {
        m_serverLog->append(QString("[状态] %1").arg(error));
    }
}

void SettingsButton::onModbusDataChanged()
{
    // 获取各个输入框
    auto idEdit = findChild<QLineEdit*>("lineEditID");
    auto funcEdit = findChild<QLineEdit*>("lineEditFunc");
    auto addrEdit = findChild<QLineEdit*>("lineEditAddress");
    auto dataEdit = findChild<QLineEdit*>("lineEditData");
    auto crcEdit = findChild<QLineEdit*>("lineEditCRC16");
    
    if (!idEdit || !funcEdit || !addrEdit || !dataEdit || !crcEdit) {
        return; // 某个输入框不存在
    }
    
    // 获取输入内容并移除空格
    QString id = idEdit->text().replace(" ", "");
    QString func = funcEdit->text().replace(" ", "");
    QString addr = addrEdit->text().replace(" ", "");
    QString data = dataEdit->text().replace(" ", "");
    
    // 如果任何一个必要字段为空，清空CRC
    if (id.isEmpty() || func.isEmpty() || addr.isEmpty() || data.isEmpty()) {
        crcEdit->setText("");
        return;
    }
    
    // 拼接数据并计算CRC
    QString combined = id + func + addr + data;
    QByteArray hexData = QByteArray::fromHex(combined.toLatin1());
    
    if (!hexData.isEmpty()) {
        quint16 crc = calculateCRC16(hexData);
        // 格式化CRC为4位16进制字符串（小端序）
        QString crcStr = QString("%1%2")
                        .arg(static_cast<quint8>(crc & 0xFF), 2, 16, QChar('0'))
                        .arg(static_cast<quint8>((crc >> 8) & 0xFF), 2, 16, QChar('0'));
        crcEdit->setText(crcStr.toUpper());
    } else {
        crcEdit->setText("");
    }
}
// CRC-16计算函数（Modbus标准）
quint16 SettingsButton::calculateCRC16(const QByteArray &data) const
{
    quint16 crc = 0xFFFF;  // 初始值
    
    for (int i = 0; i < data.length(); ++i) {
        crc ^= static_cast<quint8>(data[i]);  // XOR字节到CRC
        
        for (int j = 0; j < 8; ++j) {  // 处理8位
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;  // Modbus多项式（反向）
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc;
}

// 备用CRC计算函数（按您提供的示例实现）
// 冗余多项式：0x8005（反转后为0xA001），初始值：0xFFFF，LSB first
quint16 SettingsButton::calculateCRC16_Alternative(const QByteArray &data) const
{
    quint16 crc = 0xFFFF;  // 初始值
    const quint8* dataPtr = reinterpret_cast<const quint8*>(data.constData());
    const int length = data.size();

    for (int idx = 0; idx < length; ++idx) {
        crc ^= dataPtr[idx];
        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}


/****** 485通信公共接口实现  up ******/
// 串口连接管理
bool SettingsButton::connect485(const QString &portName, int baudRate)
{
    if (modbus485) {
        return modbus485->openPort(portName, baudRate);
    }
    return false;
}

void SettingsButton::disconnect485()
{
    if (modbus485) {
        modbus485->closePort();
    }
}

bool SettingsButton::is485Connected() const
{
    return modbus485 ? modbus485->isPortOpen() : false;
}

QStringList SettingsButton::getAvailablePorts() const
{
    return modbus485 ? modbus485->availablePorts() : QStringList();
}

// 数据发送接口
void SettingsButton::send485Data(const QByteArray &data)
{
    if (modbus485) {
        modbus485->sendData(data);
        if (m_serverLog) {
            QString fullHex = QString(data.toHex().toUpper());
            for (int i = fullHex.length() - 2; i > 0; i -= 2) {
                fullHex.insert(i, ' ');
            }
            m_serverLog->append(QString("[API发送] %1").arg(fullHex));
        }
    }
}

void SettingsButton::send485HexString(const QString &hexString)
{
    QString cleanHex = hexString;
    cleanHex = cleanHex.replace(" ", "");
    QByteArray hexData = QByteArray::fromHex(cleanHex.toLatin1());
    
    if (!hexData.isEmpty()) {
        // 自动补全CRC
        quint16 crc = calculateCRC16(hexData);
        hexData.append(static_cast<char>(crc & 0xFF));
        hexData.append(static_cast<char>((crc >> 8) & 0xFF));
        
        send485Data(hexData);
    }
}

bool SettingsButton::sendModbusCommand()
{
    auto idEdit = findChild<QLineEdit*>("lineEditID");
    auto funcEdit = findChild<QLineEdit*>("lineEditFunc");
    auto addrEdit = findChild<QLineEdit*>("lineEditAddress");
    auto dataEdit = findChild<QLineEdit*>("lineEditData");
    auto crcEdit = findChild<QLineEdit*>("lineEditCRC16");
    
    if (!idEdit || !funcEdit || !addrEdit || !dataEdit || !crcEdit) {
        return false;
    }
    
    QString combined = idEdit->text() + funcEdit->text() + addrEdit->text() + dataEdit->text() + crcEdit->text();
    combined = combined.replace(" ", "");
    QByteArray hexData = QByteArray::fromHex(combined.toLatin1());
    
    if (!hexData.isEmpty() && modbus485) {
        send485Data(hexData);
        return true;
    }
    return false;
}

// Modbus数据设置/获取
void SettingsButton::setModbusID(const QString &id)
{
    if (auto edit = findChild<QLineEdit*>("lineEditID")) {
        edit->setText(id);
    }
}

void SettingsButton::setModbusFunction(const QString &func)
{
    if (auto edit = findChild<QLineEdit*>("lineEditFunc")) {
        edit->setText(func);
    }
}

void SettingsButton::setModbusAddress(const QString &addr)
{
    if (auto edit = findChild<QLineEdit*>("lineEditAddress")) {
        edit->setText(addr);
    }
}

void SettingsButton::setModbusData(const QString &data)
{
    if (auto edit = findChild<QLineEdit*>("lineEditData")) {
        edit->setText(data);
    }
}

QString SettingsButton::getModbusID() const
{
    if (auto edit = findChild<QLineEdit*>("lineEditID")) {
        return edit->text();
    }
    return QString();
}

QString SettingsButton::getModbusFunction() const
{
    if (auto edit = findChild<QLineEdit*>("lineEditFunc")) {
        return edit->text();
    }
    return QString();
}

QString SettingsButton::getModbusAddress() const
{
    if (auto edit = findChild<QLineEdit*>("lineEditAddress")) {
        return edit->text();
    }
    return QString();
}

QString SettingsButton::getModbusData() const
{
    if (auto edit = findChild<QLineEdit*>("lineEditData")) {
        return edit->text();
    }
    return QString();
}

QString SettingsButton::getModbusCRC() const
{
    if (auto edit = findChild<QLineEdit*>("lineEditCRC16")) {
        return edit->text();
    }
    return QString();
}

// CRC计算工具
QString SettingsButton::calculateModbusCRC(const QString &hexData) const
{
    QString cleanHex = hexData;
    cleanHex = cleanHex.replace(" ", "");
    QByteArray data = QByteArray::fromHex(cleanHex.toLatin1());
    
    if (!data.isEmpty()) {
        quint16 crc = calculateCRC16(data);
        return QString("%1%2")
                .arg(static_cast<quint8>(crc & 0xFF), 2, 16, QChar('0'))
                .arg(static_cast<quint8>((crc >> 8) & 0xFF), 2, 16, QChar('0'))
                .toUpper();
    }
    return QString();
}

// 日志管理
void SettingsButton::clearLog()
{
    if (m_serverLog) {
        m_serverLog->clear();
    }
}

void SettingsButton::appendLog(const QString &message)
{
    if (m_serverLog) {
        m_serverLog->append(message);
    }
}

QString SettingsButton::getLogContent() const
{
    return m_serverLog ? m_serverLog->toPlainText() : QString();
}

// 旋转角度控制
void SettingsButton::setRotationAngle(int angle)
{
    auto slider = findChild<QSlider*>("horizontalSliderRotationAngle");
    if (slider) {
        // 限制角度范围
        int clampedAngle = std::clamp(angle, -360, 360);
        slider->setValue(clampedAngle);
    }
}

int SettingsButton::getRotationAngle() const
{
    auto slider = findChild<QSlider*>("horizontalSliderRotationAngle");
    return slider ? slider->value() : 0;
}
/****** 485通信公共接口实现  down ******/

// 初始化旋转角度滑动条
void SettingsButton::initRotationSlider()
{
    auto slider = findChild<QSlider*>("horizontalSliderRotationAngle");
    if (!slider) return;
    
    /****** 滑动条初始化修正 up ******/
    // 设置滑动条范围：-360到+360度
    slider->setRange(-360, 360);
    // 强制重置到0（中间位置）
    slider->blockSignals(true);  // 临时阻止信号，避免初始化时触发
    slider->setValue(0);
    slider->blockSignals(false); // 恢复信号
    
    // 确认当前值
    qDebug() << "[调试] 滑动条初始化后当前值:" << slider->value() << "，范围:" << slider->minimum() << "~" << slider->maximum();
    
    // 初始状态禁用（需要连接485后才能使用）
    slider->setEnabled(false);
    /****** 滑动条初始化修正 down ******/
    
    /****** 刻度 up ******/
    // 设置刻度标记
    slider->setTickPosition(QSlider::TicksBelow);  // 刻度显示在下方
    slider->setTickInterval(90);                   // 主要刻度间隔90度（-360, -270, -180, -90, 0, 90, 180, 270, 360）
    
    // 添加角度显示标签（布局兼容方式）
    if (auto parent = slider->parentWidget()) {
        // 查找是否已有角度显示标签
        QLabel *angleLabel = parent->findChild<QLabel*>("labelRotationAngle");
        if (!angleLabel) {
            // 创建角度显示标签
            angleLabel = new QLabel("0°", parent);
            angleLabel->setObjectName("labelRotationAngle");
            angleLabel->setAlignment(Qt::AlignCenter);
            angleLabel->setStyleSheet("QLabel { font-weight: bold; color: #0275d8; }");
            angleLabel->show(); // 确保标签可见
            
            // 如果父控件有布局，尝试将标签添加到布局中
            if (auto layout = parent->layout()) {
                if (auto vLayout = qobject_cast<QVBoxLayout*>(layout)) {
                    // 查找滑动条在布局中的位置
                    int sliderIndex = vLayout->indexOf(slider);
                    if (sliderIndex >= 0) {
                        // 将标签插入到滑动条上方
                        vLayout->insertWidget(sliderIndex, angleLabel);
                    } else {
                        // 如果找不到滑动条，添加到布局末尾
                        vLayout->addWidget(angleLabel);
                    }
                } else {
                    // 非垂直布局，使用固定定位（延迟设置）
                    QTimer::singleShot(200, this, [angleLabel, slider](){
                        if (slider && angleLabel) {
                            QRect sliderGeom = slider->geometry();
                            angleLabel->setGeometry(sliderGeom.x(), sliderGeom.y() - 25, sliderGeom.width(), 20);
                        }
                    });
                }
            } else {
                // 没有布局，使用固定定位
                QTimer::singleShot(200, this, [angleLabel, slider](){
                    if (slider && angleLabel) {
                        QRect sliderGeom = slider->geometry();
                        angleLabel->setGeometry(sliderGeom.x(), sliderGeom.y() - 25, sliderGeom.width(), 20);
                    }
                });
            }
        }
        
        // 连接滑动条值变化到标签更新
        connect(slider, &QSlider::valueChanged, angleLabel, [angleLabel](int value){
            angleLabel->setText(QString("%1°").arg(value));
            // 0度时使用特殊颜色标注
            if (value == 0) {
                angleLabel->setStyleSheet("QLabel { font-weight: bold; color: #d9534f; background-color: #fff3cd; border-radius: 3px; padding: 2px; }");
            } else {
                angleLabel->setStyleSheet("QLabel { font-weight: bold; color: #0275d8; }");
            }
        });
        
        // 初始显示（强制更新到0°）
        angleLabel->setText("0°");
        angleLabel->setStyleSheet("QLabel { font-weight: bold; color: #d9534f; background-color: #fff3cd; border-radius: 3px; padding: 2px; }");
        
        // 强制触发一次标签更新，确保与滑动条值同步
        if (slider->value() == 0) {
            angleLabel->setText("0°");
            angleLabel->setStyleSheet("QLabel { font-weight: bold; color: #d9534f; background-color: #fff3cd; border-radius: 3px; padding: 2px; }");
        }
    }
    /****** 刻度 down ******/
    
    /****** 松手发送控制 up ******/
    // 连接滑动条松手信号（松手后才发送指令）
    connect(slider, &QSlider::sliderReleased, this, [this](){
        auto s = findChild<QSlider*>("horizontalSliderRotationAngle");
        if (s) onRotationAngleChanged(s->value());
    });
    /****** 松手发送控制 down ******/
    
    if (m_serverLog) {
        m_serverLog->append("[系统] 旋转角度滑动条已初始化：范围-360°~+360°，默认0°，刻度间隔90°");
        m_serverLog->append("[系统] 角度映射：滑动条正数=反转，滑动条负数=正转，0=停止");
    }
    
    /****** 强制重置到0度 up ******/
    // 延迟强制重置，确保UI完全加载后再设置
    QTimer::singleShot(100, this, [slider](){
        if (slider) {
            slider->blockSignals(true);
            slider->setValue(0);
            slider->blockSignals(false);
            qDebug() << "[调试] 延迟重置后滑动条值:" << slider->value();
            
            // 手动触发标签更新
            if (auto angleLabel = slider->parentWidget()->findChild<QLabel*>("labelRotationAngle")) {
                angleLabel->setText("0°");
                angleLabel->setStyleSheet("QLabel { font-weight: bold; color: #d9534f; background-color: #fff3cd; border-radius: 3px; padding: 2px; }");
            }
        }
    });
    /****** 强制重置到0度 down ******/
}

// 处理旋转角度变化
void SettingsButton::onRotationAngleChanged(int angle)
{
    if (!modbus485 || !modbus485->isPortOpen()) {
        return; // 485未连接时不发送指令
    }
    
    // 构建Modbus指令：01 06 01 08 [角度数据] [CRC]
    QByteArray command;
    command.append(static_cast<char>(0x01)); // 设备ID
    command.append(static_cast<char>(0x06)); // 功能码：写单个寄存器
    command.append(static_cast<char>(0x01)); // 地址高位
    command.append(static_cast<char>(0x08)); // 地址低位：旋转运行
    
    /****** 角度反转映射 up ******/
    // 角度数据处理（正数=反转，负数=正转）
    // 滑动条正数角度 -> 反转指令（负数数据）
    // 滑动条负数角度 -> 正转指令（正数数据）
    quint16 angleData;
    int mappedAngle = -angle; // 反转角度映射
    
    if (mappedAngle >= 0) {
        angleData = static_cast<quint16>(mappedAngle);
    } else {
        // 负数使用16位补码表示
        angleData = static_cast<quint16>(0x10000 + mappedAngle);
    }
    /****** 角度反转映射 down ******/
    
    command.append(static_cast<char>((angleData >> 8) & 0xFF)); // 数据高位
    command.append(static_cast<char>(angleData & 0xFF));        // 数据低位
    
    // 计算并添加CRC
    quint16 crc = calculateCRC16(command);
    command.append(static_cast<char>(crc & 0xFF));        // CRC低位
    command.append(static_cast<char>((crc >> 8) & 0xFF)); // CRC高位
    
    // 发送指令
    modbus485->sendData(command);
    
    // 记录日志
    if (m_serverLog) {
        QString hexStr = QString(command.toHex().toUpper());
        for (int i = hexStr.length() - 2; i > 0; i -= 2) {
            hexStr.insert(i, ' ');
        }
        QString direction = (angle > 0) ? "反转" : (angle < 0) ? "正转" : "停止";
        m_serverLog->append(QString("[旋转控制] 滑动条: %1° (%2) -> %3")
                           .arg(angle).arg(direction).arg(hexStr));
    }
    
    // 发出角度变化信号
    emit rotationAngleChanged(angle);
}

/****** 485通信实现  down ******/
