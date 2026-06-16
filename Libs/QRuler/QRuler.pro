TEMPLATE = lib
CONFIG += shared c++17
DEFINES += QRULER_LIBRARY

TARGET = QRuler

QT += core gui widgets

INCLUDEPATH += $$PWD/include $$PWD/src $$PWD/..

HEADERS += \
    include/QRuler.h \
    include/Unit.h \
    include/ViewConverter.h \
    include/qruler_export.h \
    src/QRuler_p.h

SOURCES += \
    src/QRuler.cpp \
    src/Unit.cpp \
    src/ViewConverter.cpp

include(../../Common.pri)
