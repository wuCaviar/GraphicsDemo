TEMPLATE = lib
CONFIG += staticlib c++17

TARGET = QSimpleUpdater

QT += core gui widgets network

INCLUDEPATH += $$PWD/src $$PWD/include

include(qsimpleupdater.pri)

include(../../Common.pri)
