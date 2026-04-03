#include "motorcontrol.h"
#include "ui_motorcontrol.h"
#include "tcpclientcore.h"
#include "qsqldatabase.h"

#include <QLabel>
#include <QTimer>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QFrame>
#include <QSettings>
#include <QCloseEvent>

static const char* kMotorIniFile  = "BoxData.ini";
static const char* kMotorGroup    = "MotorCoords";

// ─────────────────────────────────────────────────────────────────────────────
// 工业风浅色主题样式常量（白底灰调）
// ─────────────────────────────────────────────────────────────────────────────
namespace {

const char* S_INIT =
    "QPushButton { background:#2e7d32; color:#fff; font-weight:700;"
    " border:none; border-radius:3px; padding:5px 10px; }"
    "QPushButton:hover { background:#388e3c; }"
    "QPushButton:pressed { background:#1b5e20; }";

const char* S_MOVE =
    "QPushButton { background:#546e7a; color:#fff; font-weight:500;"
    " border:none; border-radius:3px; padding:5px 10px; }"
    "QPushButton:hover { background:#607d8b; }"
    "QPushButton:pressed { background:#37474f; }";

const char* S_EMG =
    "QPushButton { background:#cc0000; color:#fff; font-weight:700;"
    " border:none; border-radius:3px; padding:5px 14px; }"
    "QPushButton:hover { background:#e53935; }"
    "QPushButton:pressed { background:#8b0000; }";

const char* S_ON =
    "QPushButton { background:#1565c0; color:#fff; font-weight:600;"
    " border:none; border-radius:3px; padding:5px 10px; }"
    "QPushButton:hover { background:#1976d2; }"
    "QPushButton:pressed { background:#0d47a1; }";

const char* S_OFF =
    "QPushButton { background:#90a4ae; color:#fff; font-weight:500;"
    " border:none; border-radius:3px; padding:5px 10px; }"
    "QPushButton:hover { background:#78909c; }";

const char* S_DISABLED =
    "QPushButton { background:#eceff1; color:#b0bec5; font-weight:400;"
    " border:1px solid #cfd8dc; border-radius:3px; padding:5px 10px; }";

const char* S_SPIN =
    "QSpinBox { background:#fff; color:#263238; border:1px solid #b0bec5;"
    " border-radius:3px; padding:3px 4px; font-family:Consolas,monospace; font-size:12px; }";

const char* S_LABEL =
    "QLabel { color:#546e7a; font-family:Consolas,monospace; font-size:12px; background:transparent; }";

// GroupBox 样式（左侧蓝色边框、右侧紫色边框、其他灰色边框，背景白/浅灰）
const char* GB_LEFT =
    "QGroupBox { color:#1565c0; font-weight:700; font-size:12px;"
    " border:1px solid #90caf9; border-radius:4px; margin-top:10px;"
    " background:#f5f9ff; padding:4px; }"
    "QGroupBox::title { subcontrol-origin:margin; left:8px;"
    " padding:0 4px; background:#f5f9ff; }";

const char* GB_RIGHT =
    "QGroupBox { color:#6a1b9a; font-weight:700; font-size:12px;"
    " border:1px solid #ce93d8; border-radius:4px; margin-top:10px;"
    " background:#fdf5ff; padding:4px; }"
    "QGroupBox::title { subcontrol-origin:margin; left:8px;"
    " padding:0 4px; background:#fdf5ff; }";

const char* GB_OTHER =
    "QGroupBox { color:#37474f; font-weight:700; font-size:12px;"
    " border:1px solid #b0bec5; border-radius:4px; margin-top:10px;"
    " background:#f5f7f8; padding:4px; }"
    "QGroupBox::title { subcontrol-origin:margin; left:8px;"
    " padding:0 4px; background:#f5f7f8; }";

const char* S_SECTION_HDR_LEFT =
    "QLabel { background:#1565c0; color:#fff; font-weight:700; font-size:13px;"
    " border-radius:4px; padding:4px 12px; }";

const char* S_SECTION_HDR_RIGHT =
    "QLabel { background:#6a1b9a; color:#fff; font-weight:700; font-size:13px;"
    " border-radius:4px; padding:4px 12px; }";

const char* S_SECTION_HDR_OTHER =
    "QLabel { background:#546e7a; color:#fff; font-weight:700; font-size:13px;"
    " border-radius:4px; padding:4px 12px; }";

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// 构造 / 析构
// ─────────────────────────────────────────────────────────────────────────────
MotorControl::MotorControl(TcpClientCore *tcpMain,
                           TcpClientCore *tcpBalance,
                           AppSqlDatabase *db,
                           QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::MotorControl)
    , m_tcpMain(tcpMain)
    , m_tcpBalance(tcpBalance)
    , m_db(db)
{
    ui->setupUi(this);
    setWindowTitle("电机控制面板");
    resize(1280, 820);
    setStyleSheet("QDialog { background-color:#eceff1; } QScrollArea { background:#eceff1; border:none; }");

    buildLayout();

    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, &MotorControl::updateStatusLights);
    m_statusTimer->start(1000);
    updateStatusLights();

    loadCoordsFromIni();
}

MotorControl::~MotorControl()
{
    delete ui;
}

void MotorControl::closeEvent(QCloseEvent *event)
{
    saveCoordsToIni();
    QDialog::closeEvent(event);
}

void MotorControl::loadCoordsFromIni()
{
    QSettings ini(kMotorIniFile, QSettings::IniFormat);
    ini.beginGroup(kMotorGroup);
    m_coordLeftX->setValue(     ini.value("leftX",      0).toInt());
    m_coordLeftY->setValue(     ini.value("leftY",      0).toInt());
    m_coordLeftZ->setValue(     ini.value("leftZ",      0).toInt());
    m_coordRightX->setValue(    ini.value("rightX",     0).toInt());
    m_coordRightY->setValue(    ini.value("rightY",     0).toInt());
    m_coordRightZPump->setValue(ini.value("rightZPump", 0).toInt());
    m_coordRightZClaw->setValue(  ini.value("rightZClaw",    0).toInt());
    m_spinRightGrip->setValue(    ini.value("gripAngle",      0).toInt());
    m_spinRightGripRot->setValue( ini.value("gripRotAngle",   0).toInt());
    m_spinFixedGrip->setValue(    ini.value("fixedGripAngle", 0).toInt());
    ini.endGroup();
}

void MotorControl::saveCoordsToIni()
{
    QSettings ini(kMotorIniFile, QSettings::IniFormat);
    ini.beginGroup(kMotorGroup);
    ini.setValue("leftX",      m_coordLeftX->value());
    ini.setValue("leftY",      m_coordLeftY->value());
    ini.setValue("leftZ",      m_coordLeftZ->value());
    ini.setValue("rightX",     m_coordRightX->value());
    ini.setValue("rightY",     m_coordRightY->value());
    ini.setValue("rightZPump", m_coordRightZPump->value());
    ini.setValue("rightZClaw",   m_coordRightZClaw->value());
    ini.setValue("gripAngle",      m_spinRightGrip->value());
    ini.setValue("gripRotAngle",   m_spinRightGripRot->value());
    ini.setValue("fixedGripAngle", m_spinFixedGrip->value());
    ini.endGroup();
}

// ─────────────────────────────────────────────────────────────────────────────
// LED 辅助
// ─────────────────────────────────────────────────────────────────────────────
void MotorControl::setLed(QLabel *led, bool on)
{
    if (!led) return;
    if (on) {
        led->setStyleSheet("QLabel { background:#4caf50; border-radius:6px; border:1px solid #388e3c; }");
        led->setToolTip("已连接");
    } else {
        led->setStyleSheet("QLabel { background:#f44336; border-radius:6px; border:1px solid #c62828; }");
        led->setToolTip("未连接");
    }
}

void MotorControl::updateStatusLights()
{
    setLed(m_ledMain,    m_tcpMain    && m_tcpMain->isConnected());
    setLed(m_ledBalance, m_tcpBalance && m_tcpBalance->isConnected());
    setLed(m_ledDb,      m_db         && m_db->isConnected());
}

// ─────────────────────────────────────────────────────────────────────────────
// 按钮 / 旋钮工厂
// ─────────────────────────────────────────────────────────────────────────────
QPushButton* MotorControl::makeBtn(const QString &text, const QString &role)
{
    auto *btn = new QPushButton(text, this);
    if      (role == "init")     btn->setStyleSheet(S_INIT);
    else if (role == "move")     btn->setStyleSheet(S_MOVE);
    else if (role == "emg")      btn->setStyleSheet(S_EMG);
    else if (role == "on")       btn->setStyleSheet(S_ON);
    else if (role == "off")      btn->setStyleSheet(S_OFF);
    else if (role == "disabled") { btn->setStyleSheet(S_DISABLED); btn->setEnabled(false); }
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return btn;
}

QSpinBox* MotorControl::makeStepSpin()
{
    auto *s = new QSpinBox(this);
    s->setRange(1, 99999);
    s->setValue(4096);
    s->setStyleSheet(S_SPIN);
    s->setFixedWidth(88);
    return s;
}

// ─────────────────────────────────────────────────────────────────────────────
// GroupBox 工厂：CStep 标准组（Init + 负向 + 正向 + 步数旋钮 + 急停）
// ─────────────────────────────────────────────────────────────────────────────
QGroupBox* MotorControl::makeCStepGroup(
    const QString &title,
    const QString &negLabel,
    const QString &posLabel,
    const QString &groupStyle,
    QSpinBox **spinOut,
    QObject *initTarget, const char *initSlot,
    QObject *negTarget,  const char *negSlot,
    QObject *posTarget,  const char *posSlot,
    QObject *emgTarget,  const char *emgSlot)
{
    auto *gb = new QGroupBox(title, this);
    gb->setStyleSheet(groupStyle);

    auto *vl = new QVBoxLayout(gb);
    vl->setSpacing(5);
    vl->setContentsMargins(8, 18, 8, 8);

    // 第一行：Init  负向  正向
    auto *hl1 = new QHBoxLayout;
    hl1->setSpacing(5);
    auto *bInit = makeBtn("初始化", "init");
    auto *bNeg  = makeBtn(negLabel, "move");
    auto *bPos  = makeBtn(posLabel, "move");
    hl1->addWidget(bInit);
    hl1->addWidget(bNeg);
    hl1->addWidget(bPos);

    // 第二行：步数标签  旋钮  弹簧  急停
    auto *hl2 = new QHBoxLayout;
    hl2->setSpacing(6);
    auto *lbl = new QLabel("步数:", this);
    lbl->setStyleSheet(S_LABEL);
    auto *spin = makeStepSpin();
    if (spinOut) *spinOut = spin;
    auto *bEmg = makeBtn("■ 急停", "emg");
    hl2->addWidget(lbl);
    hl2->addWidget(spin);
    hl2->addStretch();
    hl2->addWidget(bEmg);

    vl->addLayout(hl1);
    vl->addLayout(hl2);

    connect(bInit, SIGNAL(clicked()), initTarget, initSlot);
    connect(bNeg,  SIGNAL(clicked()), negTarget,  negSlot);
    connect(bPos,  SIGNAL(clicked()), posTarget,  posSlot);
    connect(bEmg,  SIGNAL(clicked()), emgTarget,  emgSlot);

    return gb;
}

// ─────────────────────────────────────────────────────────────────────────────
// GroupBox 工厂：绝对坐标移动组（Init + 急停 / 坐标输入 + 移动）
// ─────────────────────────────────────────────────────────────────────────────
QGroupBox* MotorControl::makeCoordMoveGroup(
    const QString &title,
    const QString &groupStyle,
    QSpinBox **coordOut,
    QObject *initTarget,    const char *initSlot,
    QObject *moveTarget,    const char *moveSlot,
    QObject *emgTarget,     const char *emgSlot,
    QObject *enableTarget,  const char *enableSlot,
    QObject *releaseTarget, const char *releaseSlot)
{
    auto *gb = new QGroupBox(title, this);
    gb->setStyleSheet(groupStyle);
    auto *hl = new QHBoxLayout(gb);
    hl->setSpacing(6);
    hl->setContentsMargins(8, 18, 8, 8);

    auto *bInit    = makeBtn("初始化",  "init");
    auto *lbl      = new QLabel("坐标:", this);
    lbl->setStyleSheet(S_LABEL);
    auto *spin     = new QSpinBox(this);
    spin->setRange(-99999999, 99999999);
    spin->setValue(0);
    spin->setStyleSheet(S_SPIN);
    spin->setFixedWidth(110);
    if (coordOut) *coordOut = spin;
    auto *bMove    = makeBtn("移动",    "move");
    auto *bEnable  = makeBtn("使能",    "on");
    auto *bRelease = makeBtn("释放",    "off");
    auto *bEmg     = makeBtn("■ 急停", "emg");

    hl->addWidget(bInit);
    hl->addWidget(lbl);
    hl->addWidget(spin);
    hl->addWidget(bMove);
    hl->addWidget(bEnable);
    hl->addWidget(bRelease);
    hl->addWidget(bEmg);

    connect(bInit,    SIGNAL(clicked()), initTarget,    initSlot);
    connect(bMove,    SIGNAL(clicked()), moveTarget,    moveSlot);
    connect(bEmg,     SIGNAL(clicked()), emgTarget,     emgSlot);
    connect(bEnable,  SIGNAL(clicked()), enableTarget,  enableSlot);
    connect(bRelease, SIGNAL(clicked()), releaseTarget, releaseSlot);

    return gb;
}

// ─────────────────────────────────────────────────────────────────────────────
// GroupBox 工厂：仅含 Init + 急停
// ─────────────────────────────────────────────────────────────────────────────
QGroupBox* MotorControl::makeInitEmgGroup(
    const QString &title,
    const QString &groupStyle,
    QObject *initTarget, const char *initSlot,
    QObject *emgTarget,  const char *emgSlot)
{
    auto *gb = new QGroupBox(title, this);
    gb->setStyleSheet(groupStyle);

    auto *hl = new QHBoxLayout(gb);
    hl->setSpacing(8);
    hl->setContentsMargins(8, 18, 8, 8);

    auto *bInit = makeBtn("初始化", "init");
    auto *bEmg  = makeBtn("■ 急停", "emg");
    hl->addWidget(bInit);
    hl->addWidget(bEmg);

    connect(bInit, SIGNAL(clicked()), initTarget, initSlot);
    connect(bEmg,  SIGNAL(clicked()), emgTarget,  emgSlot);

    return gb;
}

// ─────────────────────────────────────────────────────────────────────────────
// 状态栏
// ─────────────────────────────────────────────────────────────────────────────
void MotorControl::buildStatusBar(QWidget *container)
{
    auto *hl = new QHBoxLayout(container);
    hl->setContentsMargins(12, 4, 12, 4);
    hl->setSpacing(16);

    auto makeLedRow = [&](QLabel **ledOut, const QString &text) {
        auto *led = new QLabel(this);
        led->setFixedSize(12, 12);
        setLed(led, false);
        *ledOut = led;
        auto *lbl = new QLabel(text, this);
        lbl->setStyleSheet("QLabel { color:#546e7a; font-size:12px; background:transparent; }");
        hl->addWidget(led);
        hl->addWidget(lbl);
        hl->addSpacing(4);
    };

    makeLedRow(&m_ledMain,    "主机TCP");
    makeLedRow(&m_ledBalance, "天平TCP");
    makeLedRow(&m_ledDb,      "数据库");

    hl->addStretch();

    auto *title = new QLabel("电机控制面板", this);
    title->setStyleSheet("QLabel { color:#1565c0; font-weight:700; font-size:14px; background:transparent; }");
    hl->addWidget(title);
}

// ─────────────────────────────────────────────────────────────────────────────
// 左列（左X04, 左Y02, 左Z03, 研磨01）
// ─────────────────────────────────────────────────────────────────────────────
QWidget* MotorControl::buildLeftColumn()
{
    auto *w  = new QWidget(this);
    w->setStyleSheet("QWidget { background:transparent; }");
    auto *vl = new QVBoxLayout(w);
    vl->setSpacing(6);
    vl->setContentsMargins(0, 0, 6, 0);

    auto *hdr = new QLabel("  左侧轴  ", w);
    hdr->setStyleSheet(S_SECTION_HDR_LEFT);
    vl->addWidget(hdr);

    vl->addWidget(makeCoordMoveGroup(
        "左X轴  [04]", GB_LEFT, &m_coordLeftX,
        this, SLOT(onBtnInitLeftX()),
        this, SLOT(onBtnMoveLeftX()),
        this, SLOT(onBtnEmgLeftX()),
        this, SLOT(onBtnEnableLeftX()),
        this, SLOT(onBtnReleaseLeftX())));

    vl->addWidget(makeCoordMoveGroup(
        "左Y轴  [03]", GB_LEFT, &m_coordLeftY,
        this, SLOT(onBtnInitLeftY()),
        this, SLOT(onBtnMoveLeftY()),
        this, SLOT(onBtnEmgLeftY()),
        this, SLOT(onBtnEnableLeftY()),
        this, SLOT(onBtnReleaseLeftY())));

    vl->addWidget(makeCoordMoveGroup(
        "左Z轴  [02]", GB_LEFT, &m_coordLeftZ,
        this, SLOT(onBtnInitLeftZ()),
        this, SLOT(onBtnMoveLeftZ()),
        this, SLOT(onBtnEmgLeftZ()),
        this, SLOT(onBtnEnableLeftZ()),
        this, SLOT(onBtnReleaseLeftZ())));

    // 研磨电机 — 仅 Init + 急停
    vl->addWidget(makeInitEmgGroup(
        "研磨电机  [01]", GB_LEFT,
        this, SLOT(onBtnInitGrinding()),
        this, SLOT(onBtnEmgGrinding())));

    vl->addStretch();
    return w;
}

// ─────────────────────────────────────────────────────────────────────────────
// 右列（右X0A, 右Y09, 右Z泵08, 右泵07, 右Z爪06, 右爪05）
// ─────────────────────────────────────────────────────────────────────────────
QWidget* MotorControl::buildRightColumn()
{
    auto *w  = new QWidget(this);
    w->setStyleSheet("QWidget { background:transparent; }");
    auto *vl = new QVBoxLayout(w);
    vl->setSpacing(6);
    vl->setContentsMargins(6, 0, 0, 0);

    auto *hdr = new QLabel("  右侧轴  ", w);
    hdr->setStyleSheet(S_SECTION_HDR_RIGHT);
    vl->addWidget(hdr);

    vl->addWidget(makeCoordMoveGroup(
        "右X轴  [0A]", GB_RIGHT, &m_coordRightX,
        this, SLOT(onBtnInitRightX()),
        this, SLOT(onBtnMoveRightX()),
        this, SLOT(onBtnEmgRightX()),
        this, SLOT(onBtnEnableRightX()),
        this, SLOT(onBtnReleaseRightX())));

    vl->addWidget(makeCoordMoveGroup(
        "右Y轴  [09]", GB_RIGHT, &m_coordRightY,
        this, SLOT(onBtnInitRightY()),
        this, SLOT(onBtnMoveRightY()),
        this, SLOT(onBtnEmgRightY()),
        this, SLOT(onBtnEnableRightY()),
        this, SLOT(onBtnReleaseRightY())));

    vl->addWidget(makeCoordMoveGroup(
        "右Z泵  [08]", GB_RIGHT, &m_coordRightZPump,
        this, SLOT(onBtnInitRightZPump()),
        this, SLOT(onBtnMoveRightZPump()),
        this, SLOT(onBtnEmgRightZPump()),
        this, SLOT(onBtnEnableRightZPump()),
        this, SLOT(onBtnReleaseRightZPump())));

    // 右泵 — 仅 Init
    {
        auto *gb = new QGroupBox("右泵  [07]", this);
        gb->setStyleSheet(GB_RIGHT);
        auto *hl = new QHBoxLayout(gb);
        hl->setContentsMargins(8, 18, 8, 8);
        hl->setSpacing(6);
        auto *bInit = makeBtn("初始化", "init");
        hl->addWidget(bInit);
        connect(bInit, SIGNAL(clicked()), this, SLOT(onBtnInitRightPump()));
        vl->addWidget(gb);
    }

    vl->addWidget(makeCoordMoveGroup(
        "右Z爪电机  [06]", GB_RIGHT, &m_coordRightZClaw,
        this, SLOT(onBtnInitRightZClaw()),
        this, SLOT(onBtnMoveRightZClaw()),
        this, SLOT(onBtnEmgRightZClaw()),
        this, SLOT(onBtnEnableRightZClaw()),
        this, SLOT(onBtnReleaseRightZClaw())));

    // 右爪 [05] — 两行布局
    {
        auto *gb = new QGroupBox("右爪  [05]", this);
        gb->setStyleSheet(GB_RIGHT);

        auto *vl2 = new QVBoxLayout(gb);
        vl2->setSpacing(5);
        vl2->setContentsMargins(8, 18, 8, 8);

        // 第一行：初始化 | 旋转初始化 | 张开角度(0-100) | 电爪运动 | 急停
        auto *hl1 = new QHBoxLayout;
        hl1->setSpacing(6);
        auto *bInit    = makeBtn("初始化",    "init");
        auto *bRotInit = makeBtn("旋转初始化", "init");
        auto *lbl1     = new QLabel("张开角度:", this);
        lbl1->setStyleSheet(S_LABEL);
        m_spinRightGrip = new QSpinBox(this);
        m_spinRightGrip->setRange(0, 100);
        m_spinRightGrip->setValue(0);
        m_spinRightGrip->setStyleSheet(S_SPIN);
        m_spinRightGrip->setFixedWidth(70);
        auto *bMove = makeBtn("电爪运动", "move");
        auto *bEmg  = makeBtn("■ 急停",  "emg");
        hl1->addWidget(bInit);
        hl1->addWidget(bRotInit);
        hl1->addWidget(lbl1);
        hl1->addWidget(m_spinRightGrip);
        hl1->addWidget(bMove);
        hl1->addWidget(bEmg);

        // 第二行：旋转角度(-400~+400) | 旋转运动
        auto *hl2 = new QHBoxLayout;
        hl2->setSpacing(6);
        auto *lbl2 = new QLabel("旋转角度:", this);
        lbl2->setStyleSheet(S_LABEL);
        m_spinRightGripRot = new QSpinBox(this);
        m_spinRightGripRot->setRange(-400, 400);
        m_spinRightGripRot->setValue(0);
        m_spinRightGripRot->setStyleSheet(S_SPIN);
        m_spinRightGripRot->setFixedWidth(80);
        auto *bRotMove = makeBtn("旋转运动", "move");
        hl2->addWidget(lbl2);
        hl2->addWidget(m_spinRightGripRot);
        hl2->addWidget(bRotMove);

        vl2->addLayout(hl1);
        vl2->addLayout(hl2);

        connect(bInit,    SIGNAL(clicked()), this, SLOT(onBtnInitRightGripper()));
        connect(bRotInit, SIGNAL(clicked()), this, SLOT(onBtnRotInitRightGripper()));
        connect(bMove,    SIGNAL(clicked()), this, SLOT(onBtnMoveRightGripper()));
        connect(bRotMove, SIGNAL(clicked()), this, SLOT(onBtnRotMoveRightGripper()));
        connect(bEmg,     SIGNAL(clicked()), this, SLOT(onBtnEmgRightGripper()));

        vl->addWidget(gb);
    }

    vl->addStretch();
    return w;
}

// ─────────────────────────────────────────────────────────────────────────────
// 其他设备区（固定夹爪0B, 电磁铁0D, 摇床0C）
// ─────────────────────────────────────────────────────────────────────────────
QWidget* MotorControl::buildOtherSection()
{
    auto *w  = new QWidget(this);
    w->setStyleSheet("QWidget { background:transparent; }");
    auto *hl = new QHBoxLayout(w);
    hl->setSpacing(8);
    hl->setContentsMargins(0, 0, 0, 0);

    // 固定夹爪 [0B] — 初始化 | 夹爪角度(0-100) | 夹爪运动 | 急停
    {
        auto *gb = new QGroupBox("固定夹爪  [0B]", this);
        gb->setStyleSheet(GB_OTHER);
        auto *hl2 = new QHBoxLayout(gb);
        hl2->setContentsMargins(8, 18, 8, 8);
        hl2->setSpacing(6);

        auto *bInit = makeBtn("初始化", "init");
        auto *lbl   = new QLabel("夹爪角度:", this);
        lbl->setStyleSheet(S_LABEL);
        m_spinFixedGrip = new QSpinBox(this);
        m_spinFixedGrip->setRange(0, 100);
        m_spinFixedGrip->setValue(0);
        m_spinFixedGrip->setStyleSheet(S_SPIN);
        m_spinFixedGrip->setFixedWidth(70);
        auto *bMove = makeBtn("夹爪运动", "move");
        auto *bEmg  = makeBtn("■ 急停",  "emg");

        hl2->addWidget(bInit);
        hl2->addWidget(lbl);
        hl2->addWidget(m_spinFixedGrip);
        hl2->addWidget(bMove);
        hl2->addWidget(bEmg);

        connect(bInit, SIGNAL(clicked()), this, SLOT(onBtnInitFixedGripper()));
        connect(bMove, SIGNAL(clicked()), this, SLOT(onBtnMoveFixedGripper()));
        connect(bEmg,  SIGNAL(clicked()), this, SLOT(onBtnEmgFixedGripper()));
        hl->addWidget(gb);
    }

    // 电磁铁 [0D]
    {
        auto *gb = new QGroupBox("电磁铁  [0D]", this);
        gb->setStyleSheet(GB_OTHER);
        auto *hl2 = new QHBoxLayout(gb);
        hl2->setContentsMargins(8, 18, 8, 8);
        hl2->setSpacing(8);
        auto *bOn  = makeBtn("开", "on");
        auto *bOff = makeBtn("关", "off");
        hl2->addWidget(bOn);
        hl2->addWidget(bOff);
        connect(bOn,  SIGNAL(clicked()), this, SLOT(onBtnElectromagnetOn()));
        connect(bOff, SIGNAL(clicked()), this, SLOT(onBtnElectromagnetOff()));
        hl->addWidget(gb);
    }

    // 摇床
    {
        auto *gb = new QGroupBox("摇床  [0C]", this);
        gb->setStyleSheet(GB_OTHER);
        auto *hl2 = new QHBoxLayout(gb);
        hl2->setContentsMargins(8, 18, 8, 8);
        hl2->setSpacing(8);
        auto *bInit  = makeBtn("初始化", "init");
        auto *bOpen  = makeBtn("开",     "on");
        auto *bClose = makeBtn("关",     "off");
        hl2->addWidget(bInit);
        hl2->addWidget(bOpen);
        hl2->addWidget(bClose);
        connect(bInit,  SIGNAL(clicked()), this, SLOT(onBtnInitShaker()));
        connect(bOpen,  SIGNAL(clicked()), this, SLOT(onBtnShakerOpen()));
        connect(bClose, SIGNAL(clicked()), this, SLOT(onBtnShakerClose()));
        hl->addWidget(gb);
    }

    hl->addStretch();
    return w;
}

// ─────────────────────────────────────────────────────────────────────────────
// 主布局入口
// ─────────────────────────────────────────────────────────────────────────────
void MotorControl::buildLayout()
{
    auto *outerVl = new QVBoxLayout(this);
    outerVl->setSpacing(0);
    outerVl->setContentsMargins(0, 0, 0, 0);

    // 状态栏
    auto *statusFrame = new QFrame(this);
    statusFrame->setFixedHeight(38);
    statusFrame->setStyleSheet(
        "QFrame { background:#e3eaf0; border-bottom:1px solid #b0bec5; }");
    buildStatusBar(statusFrame);
    outerVl->addWidget(statusFrame);

    // 滚动区
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setStyleSheet(
        "QScrollArea { background:#eceff1; border:none; }"
        "QScrollBar:vertical { background:#eceff1; width:8px; }"
        "QScrollBar::handle:vertical { background:#b0bec5; border-radius:4px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }");

    auto *contentW = new QWidget(this);
    contentW->setStyleSheet("QWidget { background:#eceff1; }");
    auto *contentVl = new QVBoxLayout(contentW);
    contentVl->setSpacing(10);
    contentVl->setContentsMargins(12, 12, 12, 12);

    // 左侧/右侧 标题行
    auto *sectionHdr = new QHBoxLayout;
    auto *lblLeft  = new QLabel("  左侧设备  ", contentW);
    lblLeft->setStyleSheet(S_SECTION_HDR_LEFT);
    auto *lblRight = new QLabel("  右侧设备  ", contentW);
    lblRight->setStyleSheet(S_SECTION_HDR_RIGHT);
    sectionHdr->addWidget(lblLeft);
    sectionHdr->addStretch();
    sectionHdr->addWidget(lblRight);
    contentVl->addLayout(sectionHdr);

    // 左列 + 右列
    auto *columnsHl = new QHBoxLayout;
    columnsHl->setSpacing(8);
    columnsHl->addWidget(buildLeftColumn(),  1);
    columnsHl->addWidget(buildRightColumn(), 1);
    contentVl->addLayout(columnsHl);

    // 其他设备标题
    auto *otherHdr = new QLabel("  其他设备  ", contentW);
    otherHdr->setStyleSheet(S_SECTION_HDR_OTHER);
    contentVl->addWidget(otherHdr);

    contentVl->addWidget(buildOtherSection());
    contentVl->addStretch();

    scroll->setWidget(contentW);
    outerVl->addWidget(scroll, 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// 命令辅助函数
// ─────────────────────────────────────────────────────────────────────────────
void MotorControl::sendCStep(const QString &devId, const QString &func, qint64 data, int width)
{
    if (!m_tcpMain) return;
    QString cmd = m_tcpMain->buildDeviceCommand(devId, func, data, width);
    m_tcpMain->sendMessage(cmd.toUtf8(), true);
}

void MotorControl::sendModBus(const QString &devId, const QString &reg, qint64 data, int width)
{
    if (!m_tcpMain) return;
    QString cmd = m_tcpMain->buildDeviceCommand(devId, "06", reg, data, width);
    m_tcpMain->sendMessage(cmd.toUtf8(), true);
}

// ModBus hex模式（夹爪05专用）
void MotorControl::sendModBusHex(const QString &devId, const QString &reg, qint64 data, int width)
{
    if (!m_tcpMain) return;
    QString cmd = m_tcpMain->buildDeviceCommand(devId, "06", reg, data, width);
    m_tcpMain->sendMessage(cmd.toUtf8(), false);
}

void MotorControl::cstepInit(const QString &devId) { sendCStep(devId, "G", 0, 0); }
void MotorControl::cstepEmg(const QString &devId)  { sendCStep(devId, "K", 0, 1); }

// ─────────────────────────────────────────────────────────────────────────────
// Slots — 左侧
// ─────────────────────────────────────────────────────────────────────────────
void MotorControl::onBtnInitLeftX()    { cstepInit("04"); }
void MotorControl::onBtnMoveLeftX()    { sendCStep("04", "D", m_coordLeftX->value(), 8); }
void MotorControl::onBtnEmgLeftX()     { cstepEmg("04"); }
void MotorControl::onBtnEnableLeftX()  { sendCStep("04", "a", 1, 1); }
void MotorControl::onBtnReleaseLeftX() { sendCStep("04", "a", 0, 1); }

void MotorControl::onBtnInitLeftY()    { cstepInit("03"); }
void MotorControl::onBtnMoveLeftY()    { sendCStep("03", "D", m_coordLeftY->value(), 8); }
void MotorControl::onBtnEmgLeftY()     { cstepEmg("03"); }
void MotorControl::onBtnEnableLeftY()  { sendCStep("03", "a", 1, 1); }
void MotorControl::onBtnReleaseLeftY() { sendCStep("03", "a", 0, 1); }

void MotorControl::onBtnInitLeftZ()    { cstepInit("02"); }
void MotorControl::onBtnMoveLeftZ()    { sendCStep("02", "D", m_coordLeftZ->value(), 8); }
void MotorControl::onBtnEmgLeftZ()     { cstepEmg("02"); }
void MotorControl::onBtnEnableLeftZ()  { sendCStep("02", "a", 1, 1); }
void MotorControl::onBtnReleaseLeftZ() { sendCStep("02", "a", 0, 1); }

void MotorControl::onBtnInitGrinding() { cstepInit("01"); }
void MotorControl::onBtnEmgGrinding()  { cstepEmg("01"); }

// ─────────────────────────────────────────────────────────────────────────────
// Slots — 右侧
// ─────────────────────────────────────────────────────────────────────────────
void MotorControl::onBtnInitRightX()      { cstepInit("0A"); }
void MotorControl::onBtnMoveRightX()      { sendCStep("0A", "D", m_coordRightX->value(), 8); }
void MotorControl::onBtnEmgRightX()       { cstepEmg("0A"); }
void MotorControl::onBtnEnableRightX()    { sendCStep("0A", "a", 1, 1); }
void MotorControl::onBtnReleaseRightX()   { sendCStep("0A", "a", 0, 1); }

void MotorControl::onBtnInitRightY()      { cstepInit("09"); }
void MotorControl::onBtnMoveRightY()      { sendCStep("09", "D", m_coordRightY->value(), 8); }
void MotorControl::onBtnEmgRightY()       { cstepEmg("09"); }
void MotorControl::onBtnEnableRightY()    { sendCStep("09", "a", 1, 1); }
void MotorControl::onBtnReleaseRightY()   { sendCStep("09", "a", 0, 1); }

void MotorControl::onBtnInitRightZPump()     { cstepInit("08"); }
void MotorControl::onBtnMoveRightZPump()     { sendCStep("08", "D", m_coordRightZPump->value(), 8); }
void MotorControl::onBtnEmgRightZPump()      { cstepEmg("08"); }
void MotorControl::onBtnEnableRightZPump()   { sendCStep("08", "a", 1, 1); }
void MotorControl::onBtnReleaseRightZPump()  { sendCStep("08", "a", 0, 1); }

void MotorControl::onBtnInitRightPump() { cstepInit("07"); }

void MotorControl::onBtnInitRightZClaw()     { cstepInit("06"); }
void MotorControl::onBtnMoveRightZClaw()     { sendCStep("06", "D", m_coordRightZClaw->value(), 8); }
void MotorControl::onBtnEmgRightZClaw()      { cstepEmg("06"); }
void MotorControl::onBtnEnableRightZClaw()   { sendCStep("06", "a", 1, 1); }
void MotorControl::onBtnReleaseRightZClaw()  { sendCStep("06", "a", 0, 1); }

void MotorControl::onBtnInitRightGripper()    { sendModBusHex("05", "0100", 1); }
void MotorControl::onBtnRotInitRightGripper() { sendModBusHex("05", "0101", 1); }
void MotorControl::onBtnMoveRightGripper()    { sendModBusHex("05", "0105", m_spinRightGrip->value()); }
void MotorControl::onBtnRotMoveRightGripper() { sendModBusHex("05", "0108", m_spinRightGripRot->value()); }
void MotorControl::onBtnEmgRightGripper()     { sendModBusHex("05", "0102", 1); }

// ─────────────────────────────────────────────────────────────────────────────
// Slots — 其他设备
// ─────────────────────────────────────────────────────────────────────────────
void MotorControl::onBtnInitFixedGripper() { sendModBusHex("0B", "0100", 1); }
void MotorControl::onBtnMoveFixedGripper() { sendModBusHex("0B", "0105", m_spinFixedGrip->value()); }
void MotorControl::onBtnEmgFixedGripper()  { sendModBusHex("0B", "0102", 1); }

void MotorControl::onBtnElectromagnetOn()
{
    if (!m_tcpMain) return;
    QString cmd = m_tcpMain->buildDeviceCommand("0D", "05", "0000FF00", 0, 0);
    m_tcpMain->sendMessage(cmd.toUtf8(), false);
}

void MotorControl::onBtnElectromagnetOff()
{
    if (!m_tcpMain) return;
    QString cmd = m_tcpMain->buildDeviceCommand("0D", "05", "00000000", 0, 0);
    m_tcpMain->sendMessage(cmd.toUtf8(), false);
}

void MotorControl::onBtnInitShaker()
{
    if (!m_tcpMain) return;
    QString cmd = m_tcpMain->buildMessageWithCrc(">0CG");
    m_tcpMain->sendMessage(cmd.toUtf8(), true);
}

void MotorControl::onBtnShakerOpen()
{
    if (!m_tcpMain) return;
    // 开摇床：期望回复 "0Cxi300"
    QString cmd = m_tcpMain->buildMessageWithCrc(">0Cxi3000000012c0000012c");
    m_tcpMain->sendMessage(cmd.toUtf8(), true);
}

void MotorControl::onBtnShakerClose()
{
    if (!m_tcpMain) return;
    // 关摇床：期望回复 "0Cxi301"
    QString cmd = m_tcpMain->buildMessageWithCrc(">0Cxi30100000000");
    m_tcpMain->sendMessage(cmd.toUtf8(), true);
}
