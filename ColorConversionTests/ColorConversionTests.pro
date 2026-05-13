QT += testlib gui widgets

TARGET = tst_colorconversion
CONFIG += c++17 console testcase
CONFIG -= app_bundle

QMAKE_CXXFLAGS += -Wall -Wextra -Wpedantic

include(../Common.pri)

SOURCES += \
    tst_colorconversion.cpp
