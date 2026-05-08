QT += testlib gui widgets

TARGET = tst_alignment
CONFIG += c++17 console testcase
CONFIG -= app_bundle

# RTTI needed for dynamic_cast in AlignmentUtils
CONFIG += rtti

SOURCES += \
    tst_alignment.cpp \
    ../Src/Utils/AlignmentUtils.cpp

HEADERS += \
    ../Src/Utils/AlignmentUtils.h \
    ../Src/Items/IGraphicsItem.h

INCLUDEPATH += $$PWD/../Src \
               $$PWD/../Src/Utils \
               $$PWD/../Src/Items \
               $$PWD/../Src/Commands \
               $$PWD/../Src/UI
