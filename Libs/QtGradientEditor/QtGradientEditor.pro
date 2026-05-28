TEMPLATE = lib
CONFIG += staticlib c++17

TARGET = QtGradientEditor

QT += core gui widgets xml

INCLUDEPATH += $$PWD $$PWD/.. $$PWD/../Qt-Color-Widgets/include $$PWD/../Qt-Color-Widgets/src

include(../../Common.pri)

# 依赖 QtColorWidgets（DESTDIR 由 Common.pri 设置）
LIBS += -L$$PROJECT_PATH/Bin -lQtColorWidgets
PRE_TARGETDEPS += $$PROJECT_PATH/Bin/$${QMAKE_PREFIX_STATICLIB}QtColorWidgets.$${QMAKE_EXTENSION_STATICLIB}

include(qtgradienteditor.pri)
