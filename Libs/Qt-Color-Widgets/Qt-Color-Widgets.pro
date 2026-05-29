TEMPLATE = lib
CONFIG += shared c++17

TARGET = QtColorWidgets

QT += core gui widgets

DEFINES += QTCOLORWIDGETS_LIBRARY

INCLUDEPATH += $$PWD/src $$PWD/include $$PWD/../ColorTrans $$PWD/..

include(color_widgets.pri)

include(../../Common.pri)
