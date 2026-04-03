#ifndef MOTORCONTROL_H
#define MOTORCONTROL_H

#include <QDialog>
#include <QTimer>
#include <QPushButton>
#include <QSpinBox>
#include <QLabel>
#include <QGroupBox>

class TcpClientCore;
class AppSqlDatabase;

namespace Ui { class MotorControl; }

class MotorControl : public QDialog
{
    Q_OBJECT

public:
    explicit MotorControl(TcpClientCore *tcpMain,
                          TcpClientCore *tcpBalance,
                          AppSqlDatabase *db,
                          QWidget *parent = nullptr);
    ~MotorControl();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void updateStatusLights();

    // ── 左侧 ────────────────────────────────────────────────────
    // 左X轴 (04, CStep)
    void onBtnInitLeftX();
    void onBtnMoveLeftX();
    void onBtnEmgLeftX();
    void onBtnEnableLeftX();
    void onBtnReleaseLeftX();

    // 左Y轴 (03, CStep)
    void onBtnInitLeftY();
    void onBtnMoveLeftY();
    void onBtnEmgLeftY();
    void onBtnEnableLeftY();
    void onBtnReleaseLeftY();

    // 左Z轴 (02, CStep)
    void onBtnInitLeftZ();
    void onBtnMoveLeftZ();
    void onBtnEmgLeftZ();
    void onBtnEnableLeftZ();
    void onBtnReleaseLeftZ();

    // 研磨电机 (01, CStep) — 仅初始化和急停
    void onBtnInitGrinding();
    void onBtnEmgGrinding();

    // ── 右侧 ────────────────────────────────────────────────────
    // 右X轴 (0A, CStep)
    void onBtnInitRightX();
    void onBtnMoveRightX();
    void onBtnEmgRightX();
    void onBtnEnableRightX();
    void onBtnReleaseRightX();

    // 右Y轴 (09, CStep)
    void onBtnInitRightY();
    void onBtnMoveRightY();
    void onBtnEmgRightY();
    void onBtnEnableRightY();
    void onBtnReleaseRightY();

    // 右Z泵 (08, CStep)
    void onBtnInitRightZPump();
    void onBtnMoveRightZPump();
    void onBtnEmgRightZPump();
    void onBtnEnableRightZPump();
    void onBtnReleaseRightZPump();

    // 右泵 (07) — 仅初始化
    void onBtnInitRightPump();

    // 右Z爪电机 (06, CStep)
    void onBtnInitRightZClaw();
    void onBtnMoveRightZClaw();
    void onBtnEmgRightZClaw();
    void onBtnEnableRightZClaw();
    void onBtnReleaseRightZClaw();

    // 右爪 (05, ModBus) — 初始化 + 旋转初始化 + 电爪运动 + 旋转运动 + 急停
    void onBtnInitRightGripper();
    void onBtnRotInitRightGripper();
    void onBtnMoveRightGripper();
    void onBtnRotMoveRightGripper();
    void onBtnEmgRightGripper();

    // ── 其他设备 ─────────────────────────────────────────────────
    // 固定夹爪 (0B, ModBus hex)
    void onBtnInitFixedGripper();
    void onBtnMoveFixedGripper();
    void onBtnEmgFixedGripper();

    // 电磁铁 (0D)
    void onBtnElectromagnetOn();
    void onBtnElectromagnetOff();

    // 摇床 (0C)
    void onBtnInitShaker();
    void onBtnShakerOpen();
    void onBtnShakerClose();

private:
    // ── 布局构建 ─────────────────────────────────────────────────
    void buildLayout();
    void buildStatusBar(QWidget *container);
    QWidget* buildLeftColumn();
    QWidget* buildRightColumn();
    QWidget* buildOtherSection();

    // 生成 CStep 标准组（Init + 负向移动 + 正向移动 + 步数旋钮 + 急停）
    QGroupBox* makeCStepGroup(const QString &title,
                               const QString &negLabel,
                               const QString &posLabel,
                               const QString &groupStyle,
                               QSpinBox **spinOut,
                               QObject *initTarget, const char *initSlot,
                               QObject *negTarget,  const char *negSlot,
                               QObject *posTarget,  const char *posSlot,
                               QObject *emgTarget,  const char *emgSlot);

    // 生成仅含 Init + 急停 的组
    QGroupBox* makeInitEmgGroup(const QString &title,
                                 const QString &groupStyle,
                                 QObject *initTarget, const char *initSlot,
                                 QObject *emgTarget,  const char *emgSlot);

    QGroupBox* makeCoordMoveGroup(const QString &title,
                                   const QString &groupStyle,
                                   QSpinBox **coordOut,
                                   QObject *initTarget,    const char *initSlot,
                                   QObject *moveTarget,    const char *moveSlot,
                                   QObject *emgTarget,     const char *emgSlot,
                                   QObject *enableTarget,  const char *enableSlot,
                                   QObject *releaseTarget, const char *releaseSlot);

    // ── 按钮/旋钮工厂 ────────────────────────────────────────────
    QPushButton* makeBtn(const QString &text, const QString &role);
    QSpinBox*    makeStepSpin();

    // ── INI 持久化 ───────────────────────────────────────────────
    void loadCoordsFromIni();
    void saveCoordsToIni();

    // ── LED 辅助 ─────────────────────────────────────────────────
    void setLed(QLabel *led, bool on);

    // ── 命令辅助 ─────────────────────────────────────────────────
    void sendCStep(const QString &devId, const QString &func, qint64 data, int width);
    void sendModBus(const QString &devId, const QString &reg, qint64 data, int width = 4);
    void sendModBusHex(const QString &devId, const QString &reg, qint64 data, int width = 4);
    void cstepInit(const QString &devId);
    void cstepEmg(const QString &devId);

    // ── 成员变量 ─────────────────────────────────────────────────
    Ui::MotorControl *ui;
    TcpClientCore  *m_tcpMain    = nullptr;
    TcpClientCore  *m_tcpBalance = nullptr;
    AppSqlDatabase *m_db         = nullptr;
    QTimer         *m_statusTimer = nullptr;

    // 状态指示灯（代码创建，非来自 .ui）
    QLabel *m_ledMain    = nullptr;
    QLabel *m_ledBalance = nullptr;
    QLabel *m_ledDb      = nullptr;

    // 各轴步数旋钮
    QSpinBox *m_coordLeftX     = nullptr;
    QSpinBox *m_coordLeftY     = nullptr;
    QSpinBox *m_coordLeftZ     = nullptr;
    QSpinBox *m_coordRightX    = nullptr;
    QSpinBox *m_coordRightY    = nullptr;
    QSpinBox *m_coordRightZPump= nullptr;
    QSpinBox *m_coordRightZClaw= nullptr;
    QSpinBox *m_spinRightGrip    = nullptr;  // 右爪张开角度 (0-100)
    QSpinBox *m_spinRightGripRot = nullptr;  // 右爪旋转角度 (-400~+400)
    QSpinBox *m_spinFixedGrip    = nullptr;  // 固定夹爪角度 (0-100)
};

#endif // MOTORCONTROL_H
