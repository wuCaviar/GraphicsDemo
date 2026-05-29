TEMPLATE = lib
CONFIG += shared c++17
DEFINES += COLORTRANS_LIBRARY

TARGET = ColorTrans

QT += core

INCLUDEPATH += $$PWD/..

HEADERS += \
    colortrans_global.h \
    colortransform.h \
    ../Common/ColorTypes.h \
    ../Common/IColorTransform.h

SOURCES += \
    colortransform.cpp

include(../../Common.pri)
