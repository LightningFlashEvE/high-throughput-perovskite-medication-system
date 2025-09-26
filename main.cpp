#include "mainwindow.h"
#include "RtspPlayer/rtspplayer.h"

#include <QApplication>
#include <qqml.h>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    // 不再暴露 QWidget 给 QML，改为在 QWidget 内部加载 QML
    MainWindow w;
    w.show();
    return a.exec();
}
