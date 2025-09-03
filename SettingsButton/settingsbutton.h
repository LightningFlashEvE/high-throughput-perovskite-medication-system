#ifndef SETTINGSBUTTON_H
#define SETTINGSBUTTON_H

#include <QWidget>
#include <QVector>
#include <QPointF>

namespace Ui {
class SettingsButton;
}

class SettingsButton : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsButton(QWidget *parent = nullptr);
    ~SettingsButton();

    void LED(bool changeColor);

    // 在设置页面上显示当前位置，如 "(col,row)"
    void setLocation(int col, int row);

signals:
    void moveUpClicked();
    void moveRightClicked();
    void moveLeftClicked();
    void moveDownClicked();
    // 坐标编辑信号：当 X/Y 文本框提交（回车或失焦）后触发
    void positionEdited(int col, int row);
private:
    Ui::SettingsButton *ui;

/******  试管状态机 ******/
public:
    enum class TubeState {
        Empty,
        Full,
        Using,
        Error,
        Disabled
    };

    struct Tube {
        QPointF center;
        qreal   radius;
        TubeState state = TubeState::Empty;
    };

    // 设置指定试管状态
    void setTubeState(int idx, TubeState s);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QVector<Tube> m_tubes; // 此处仅使用一个，中心(50,50)
    QRect tubeRect(int idx) const;
/******  试管状态机 ******/
};

#endif // SETTINGSBUTTON_H
