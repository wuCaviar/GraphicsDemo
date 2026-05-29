TEMPLATE = lib
CONFIG += shared c++17
DEFINES += QTGRADIENTEDITOR_LIBRARY

TARGET = QtGradientEditor

QT += core gui widgets xml

INCLUDEPATH += $$PWD $$PWD/.. $$PWD/../Qt-Color-Widgets/include $$PWD/../Qt-Color-Widgets/src

include(../../Common.pri)

# 依赖 QtColorWidgets（DESTDIR 由 Common.pri 设置）
LIBS += -L$$PROJECT_PATH/Bin -lQtColorWidgets

include(qtgradienteditor.pri)
