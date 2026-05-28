TEMPLATE = lib
CONFIG += staticlib c++17

TARGET = QtColorWidgets

QT += core gui widgets

DEFINES += QTCOLORWIDGETS_STATICALLY_LINKED

INCLUDEPATH += $$PWD/src $$PWD/include $$PWD/..

include(color_widgets.pri)

include(../../Common.pri)
