QT += core gui quick quickcontrols2 testlib
CONFIG += c++17 testcase
TARGET = halo_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/eqresponse.h ../src/headset.h ../src/hidtransport.h ../src/protocol.h ../src/settings.h \
           ../src/theme.h ../src/translator.h ../src/transport.h
SOURCES += halo_tests.cpp ../src/eqresponse.cpp ../src/headset.cpp ../src/hidtransport.cpp ../src/protocol.cpp \
           ../src/settings.cpp ../src/theme.cpp ../src/translator.cpp
