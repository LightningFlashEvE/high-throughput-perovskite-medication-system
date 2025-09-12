QT += network widgets core gui

INCLUDEPATH += $$PWD/..
DEPENDPATH += $$PWD/..

FORMS += \
    $$PWD/tcpclient.ui

HEADERS += \
    $$PWD/tcpclient.h \
    $$PWD/tcpclient_crc.h \
    $$PWD/protocolbase.h \
    $$PWD/protocol_adp1000.h \
    $$PWD/protocol_modbus.h \
    $$PWD/protocol_cstep.h

SOURCES += \
    $$PWD/tcpclient.cpp \
    $$PWD/tcpclient_crc.cpp \
    $$PWD/protocolbase.cpp \
    $$PWD/protocol_adp1000.cpp \
    $$PWD/protocol_modbus.cpp \
    $$PWD/protocol_cstep.cpp 
