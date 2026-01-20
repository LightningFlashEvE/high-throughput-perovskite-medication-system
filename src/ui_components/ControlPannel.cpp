#include "ControlPannel.h"
#include <QDebug>
#include <QTcpSocket>
#include <QLabel>
#include <QLayout>
#include <QGridLayout>
#include <QPushButton>

ControlPannel::ControlPannel(QWidget* parent)
    : QWidget(parent)
{
    //setStyleSheet("background-color: yellow;");
    setStyleSheet("background-color: #11ffff00;");

    QVBoxLayout* vlayout = new QVBoxLayout;

    QGridLayout* gridLayout = new QGridLayout;

    QLabel* label00 = new QLabel("xy平面：");
    QLabel* label01 = new QLabel("ControlPannel");
    QLabel* label02 = new QLabel("ControlPannel");

    QLabel* label10 = new QLabel("ControlPannel");
    QLabel* label11 = new QLabel("ControlPannel");
    QLabel* label12 = new QLabel("ControlPannel");


    QLabel* label20 = new QLabel("ControlPannel");
    QLabel* label21 = new QLabel("ControlPannel");
    QLabel* label22 = new QLabel("ControlPannel");

    QPushButton* btn01 = new QPushButton("上");
    QPushButton* btn10 = new QPushButton("左");
    QPushButton* btn12 = new QPushButton("右");
    QPushButton* btn21 = new QPushButton("下");

    QLabel* label30 = new QLabel("z爪：");
    QLabel* label31 = new QLabel("ControlPannel");
    QLabel* label41 = new QLabel("ControlPannel");

    QPushButton* btn31 = new QPushButton("上");
    QPushButton* btn41 = new QPushButton("下");
    QPushButton* btn32 = new QPushButton("开爪");
    QPushButton* btn42 = new QPushButton("闭爪");

    QLabel* label50 = new QLabel("x复位：");
    QLabel* label60 = new QLabel("y复位：");
    QLabel* label70 = new QLabel("z复位：");

    QLabel* label51 = new QLabel("x");
    QLabel* label61 = new QLabel("y");
    QLabel* label71 = new QLabel("z");

    QPushButton* btn51 = new QPushButton("x复位");
    QPushButton* btn61 = new QPushButton("y复位");
    QPushButton* btn71 = new QPushButton("z复位");

    // QPushButton* btn01 = new QPushButton("上");
    // QPushButton* btn10 = new QPushButton("左");
    // QPushButton* btn12 = new QPushButton("右");
    // QPushButton* btn21 = new QPushButton("下");

    gridLayout->addWidget(label00, 0, 0);
    gridLayout->addWidget(btn01, 0, 1);
    gridLayout->addWidget(label02, 0, 2);
    gridLayout->addWidget(btn10, 1, 0);
    gridLayout->addWidget(label11, 1, 1);
    gridLayout->addWidget(btn12, 1, 2);
    gridLayout->addWidget(label20, 2, 0);
    gridLayout->addWidget(btn21, 2, 1);
    gridLayout->addWidget(label22, 2, 2);

    gridLayout->addWidget(label30, 3, 0);
    gridLayout->addWidget(btn31, 3, 1);
    gridLayout->addWidget(btn41, 4, 1);
    gridLayout->addWidget(btn32, 3, 2);
    gridLayout->addWidget(btn42, 4, 2);

    gridLayout->addWidget(label50, 5, 0);
    gridLayout->addWidget(label60, 6, 0);
    gridLayout->addWidget(label70, 7, 0);

    gridLayout->addWidget(btn51, 5, 1);
    gridLayout->addWidget(btn61, 6, 1);
    gridLayout->addWidget(btn71, 7, 1);

    vlayout->addLayout(gridLayout);
    vlayout->addStretch();

    setLayout(vlayout);

    connect(btn01, &QPushButton::clicked, this, &ControlPannel::clickBtn01);
    connect(btn10, &QPushButton::clicked, this, &ControlPannel::clickBtn10);
    connect(btn12, &QPushButton::clicked, this, &ControlPannel::clickBtn12);
    connect(btn21, &QPushButton::clicked, this, &ControlPannel::clickBtn21);

    connect(btn31, &QPushButton::clicked, this, &ControlPannel::clickBtn31);
    connect(btn41, &QPushButton::clicked, this, &ControlPannel::clickBtn41);
    connect(btn32, &QPushButton::clicked, this, &ControlPannel::clickBtn32);
    connect(btn42, &QPushButton::clicked, this, &ControlPannel::clickBtn42);

    connect(btn51, &QPushButton::clicked, this, &ControlPannel::clickBtn51);
    connect(btn61, &QPushButton::clicked, this, &ControlPannel::clickBtn61);
    connect(btn71, &QPushButton::clicked, this, &ControlPannel::clickBtn71);
}

void ControlPannel::clickBtn01() {
    //qDebug() << "clickBtn01";
}
void ControlPannel::clickBtn10() {
     //qDebug() << "clickBtn10";
}
void ControlPannel::clickBtn12() {
     //qDebug() << "clickBtn12";
}
void ControlPannel::clickBtn21() {
     //qDebug() << "clickBtn21";
}

void ControlPannel::clickBtn31() {

}

void ControlPannel::clickBtn41() {

}

void ControlPannel::clickBtn32() {

}

void ControlPannel::clickBtn42() {

}

void ControlPannel::clickBtn51() {

}

void ControlPannel::clickBtn61() {

}

void ControlPannel::clickBtn71() {

}

