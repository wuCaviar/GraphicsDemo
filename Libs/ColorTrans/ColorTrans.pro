TEMPLATE = lib
CONFIG += staticlib c++17

TARGET = ColorTrans

QT += core

INCLUDEPATH += $$PWD/..

HEADERS += \
    colortransform.h \
    ../Common/ColorTypes.h \
    ../Common/IColorTransform.h

SOURCES += \
    colortransform.cpp

include(../../Common.pri)
