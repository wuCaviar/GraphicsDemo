TEMPLATE = app

QT += widgets

CONFIG += c++17

TARGET = WidgetTest

include(../Common.pri)

SOURCES += \
    colormanager.cpp \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    colormanager.h \
    mainwindow.h

FORMS += \
    mainwindow.ui
