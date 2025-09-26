#include <QApplication>
#include "RtspPlayer/rtspplayer.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    RtspPlayer player;
    player.show();
    
    return app.exec();
}
