
TEMPLATE = app

QT += core gui widgets

TARGET = GraphicsDemo

DEFINES += QTCOLORWIDGETS_STATICALLY_LINKED

include(Qt-Color-Widgets/color_widgets.pri)
include(QtGradientEditor/qtgradienteditor.pri)

# 启用 RTTI（AlignmentUtils 使用 dynamic_cast<IGraphicsItem*>）
CONFIG += rtti

HEADERS += \
    UI/mainwindow.h \
    UI/qatgraphicsview.h \
    UI/PropertyPanel.h \
    UI/NewFileDialog.h \
    UI/RulerBar.h \
    UI/GradientDialog.h \
    UI/AlignLayoutDialog.h \
    UI/ImportImageDialog.h \
    UI/ExportImageDialog.h \
    Items/IGraphicsItem.h \
    Items/RectItem.h \
    Items/EllipseItem.h \
    Items/LineItem.h \
    Items/BezierCurveItem.h \
    Items/TextItem.h \
    Items/ImageItem.h \
    Items/FreehandItem.h \
    Items/CanvasItem.h \
    Items/ResizeHandleItem.h \
    Commands/Commands.h \
    Utils/ImageUtils.h \
    Utils/ColorUtils.h \
    Utils/AlignmentUtils.h


SOURCES += \
    App/main.cpp \
    UI/mainwindow.cpp \
    UI/qatgraphicsview.cpp \
    UI/PropertyPanel.cpp \
    UI/NewFileDialog.cpp \
    UI/RulerBar.cpp \
    UI/GradientDialog.cpp \
    UI/AlignLayoutDialog.cpp \
    UI/ImportImageDialog.cpp \
    UI/ExportImageDialog.cpp \
    Items/IGraphicsItem.cpp \
    Items/RectItem.cpp \
    Items/EllipseItem.cpp \
    Items/LineItem.cpp \
    Items/BezierCurveItem.cpp \
    Items/TextItem.cpp \
    Items/ImageItem.cpp \
    Items/FreehandItem.cpp \
    Items/CanvasItem.cpp \
    Items/ResizeHandleItem.cpp \
    Commands/Commands.cpp \
    Utils/ImageUtils.cpp \
    Utils/ColorUtils.cpp \
    Utils/AlignmentUtils.cpp

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
    $$PWD/Utils

include(Common.pri)
