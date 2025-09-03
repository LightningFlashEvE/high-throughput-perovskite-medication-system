QT       += core gui
QT += widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp 

HEADERS += \
    mainwindow.h 

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

include($$PWD/FlowViewManager/flowviewmanager.pri)  #include()包含的文件会显示在工程结构目录中
INCLUDEPATH += $$PWD/FlowViewManager   # 工程编译时，会去INCLUDEPATH列表下的目录搜索文件
include($$PWD/ChessBoardView/chessboardview.pri)  #include()包含的文件会显示在工程结构目录中
INCLUDEPATH += $$PWD/chessboardview   # 工程编译时，会去INCLUDEPATH列表下的目录搜索文件

include($$PWD/TcpFramedClient/tcpframedclient.pri)  # 引入 tcp 客户端模块
INCLUDEPATH += $$PWD/TcpFramedClient  # 让编译器能找到该目录的头文件（可选；pri 已经加了 INCLUDEPATH）
# include($$PWD/ModbusDevice/modbusdevice.pri)
# INCLUDEPATH += $$PWD/ModbusDevice
include($$PWD/SettingsButton/settingsbutton.pri)
INCLUDEPATH += $$PWD/SettingsButton

RESOURCES += \
    picture.qrc
