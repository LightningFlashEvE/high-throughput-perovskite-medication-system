#include "settingsbutton.h"
#include "ui_settingsbutton.h"
#include <QPainter> // for 试管状态机
#include <QPaintEvent> // for 试管状态机

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
