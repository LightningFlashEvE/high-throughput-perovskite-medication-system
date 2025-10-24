#include "mainwindow.h"
#include "ui_mainwindow.h"


#include "box.h"
#include "reagentbottle.h"
#include "slot.h"

void MainWindow::ForTempTest()
{

    // 这里传入this是为了将MainWindow作为Box的父对象，从而利用Qt的对象树管理Box的生命周期
    auto *box = new Box(15, this);

    // 放一只瓶（Box/Slot 接管内存）
    auto *b = new ReagentBottle();
    b->setName("盐酸");
    b->setInitial(100);
    b->setRemaining(100);
    b->setHeight(95);
    b->setPos(12, 18);


    box->addReagentBottleToSlot(3, b);

    // 更新剩余量
    box->updateRemaining(3, 95);

    // 读取
    if (box->hasBottle(3)) {
        auto *rb = box->bottleAt(3);
        qDebug() << rb->getName() << rb->getRemaining();
    }

    // 移除
    box->removeBottleFromSlot(3);
}

