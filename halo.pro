QT += core gui qml quick quickcontrols2

CONFIG += c++17 release
TARGET = halo
TEMPLATE = app

HEADERS += \
    src/cli.h \
    src/eqresponse.h \
    src/headset.h \
    src/hidtransport.h \
    src/protocol.h \
    src/settings.h \
    src/theme.h \
    src/translator.h \
    src/transport.h

SOURCES += \
    src/main.cpp \
    src/cli.cpp \
    src/eqresponse.cpp \
    src/headset.cpp \
    src/hidtransport.cpp \
    src/protocol.cpp \
    src/settings.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
