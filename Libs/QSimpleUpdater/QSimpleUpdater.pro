TEMPLATE = lib
CONFIG += shared c++17
DEFINES += QSU_SHARED

TARGET = QSimpleUpdater

QT += core gui widgets network

INCLUDEPATH += $$PWD/src $$PWD/include

include(qsimpleupdater.pri)

include(../../Common.pri)
