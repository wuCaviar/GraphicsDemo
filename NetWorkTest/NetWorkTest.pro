QT = core network

CONFIG += c++17 cmdline

include(../Common.pri)

HEADERS += \
        $$PWD/../Src/NetWork/QHttp.h \
        $$PWD/../Src/Utils/NetWorkUtils.h

SOURCES += \
        main.cpp \
        $$PWD/../Src/NetWork/QHttp.cpp \
        $$PWD/../Src/Utils/NetWorkUtils.cpp
