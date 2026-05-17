
TEMPLATE = app

QT += core gui widgets network xml

TARGET = GraphicsDemo

DEFINES += QTCOLORWIDGETS_STATICALLY_LINKED

include(Qt-Color-Widgets/color_widgets.pri)
include(QtGradientEditor/qtgradienteditor.pri)

# 启用 RTTI（AlignmentUtils 使用 dynamic_cast<IGraphicsItem*>）
CONFIG += rtti c++17

# 编译器警告
QMAKE_CXXFLAGS += -Wall

HEADERS += \
    App/SingleInstance.h \
    UI/mainwindow.h \
    UI/qatgraphicsview.h \
    UI/GraphicsScene.h \
    UI/PropertyPanel.h \
    UI/NewFileDialog.h \
    UI/RulerBar.h \
    UI/GradientDialog.h \
    UI/AlignLayoutDialog.h \
    UI/ImportImageDialog.h \
    UI/ExportImageDialog.h \
    UI/SettingsDialog.h \
    Items/IGraphicsItem.h \
    Items/RectItem.h \
    Items/EllipseItem.h \
    Items/LineItem.h \
    Items/BezierCurveItem.h \
    Items/TextItem.h \
    Items/ImageItem.h \
    Items/FreehandItem.h \
    Items/GraphicsItemGroup.h \
    Items/CanvasItem.h \
    Items/ResizeHandleItem.h \
    Commands/Commands.h \
    Utils/ImageUtils.h \
    Utils/ColorUtils.h \
    Utils/AlignmentUtils.h \
    ColorTrans/colortransform.h \
    NetWork/QHttp.h \
    CommonDefs.h \
    QCommonDefs.h \
    Utils/NetWorkDefs.h \
    Utils/NetWorkUtils.h

SOURCES += \
    App/SingleInstance.cpp \
    App/main.cpp \
    UI/mainwindow.cpp \
    UI/qatgraphicsview.cpp \
    UI/GraphicsScene.cpp \
    UI/PropertyPanel.cpp \
    UI/NewFileDialog.cpp \
    UI/RulerBar.cpp \
    UI/GradientDialog.cpp \
    UI/AlignLayoutDialog.cpp \
    UI/ImportImageDialog.cpp \
    UI/ExportImageDialog.cpp \
    UI/SettingsDialog.cpp \
    Items/IGraphicsItem.cpp \
    Items/RectItem.cpp \
    Items/EllipseItem.cpp \
    Items/LineItem.cpp \
    Items/BezierCurveItem.cpp \
    Items/TextItem.cpp \
    Items/ImageItem.cpp \
    Items/FreehandItem.cpp \
    Items/GraphicsItemGroup.cpp \
    Items/CanvasItem.cpp \
    Items/ResizeHandleItem.cpp \
    Commands/Commands.cpp \
    Utils/ImageUtils.cpp \
    Utils/ColorUtils.cpp \
    Utils/AlignmentUtils.cpp \
    ColorTrans/colortransform.cpp \
    NetWork/QHttp.cpp \
    Utils/NetWorkUtils.cpp

FORMS += \
    UI/mainwindow.ui \

RESOURCES += \
    translations/translations.qrc \
    resources/resources.qrc

TRANSLATIONS += \
    translations/GraphicsDemo_zh_CN.ts


INCLUDEPATH += \
    $$PWD/App \
    $$PWD/UI \
    $$PWD/Common \
    $$PWD/Items \
    $$PWD/Commands \
    $$PWD/Utils \
    $$PWD/ColorTrans \
    $$PWD/NetWork

include(../Common.pri)
