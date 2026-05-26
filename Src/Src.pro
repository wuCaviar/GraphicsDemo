
TEMPLATE = app

QT += core gui widgets network xml concurrent

TARGET = ATGraphics

DEFINES += QTCOLORWIDGETS_STATICALLY_LINKED

include(Qt-Color-Widgets/color_widgets.pri)
include(QtGradientEditor/qtgradienteditor.pri)
include(QSimpleUpdater/qsimpleupdater.pri)

# 启用 RTTI（AlignmentUtils 使用 dynamic_cast<IGraphicsItem*>）
CONFIG += rtti c++17

# 编译器警告
QMAKE_CXXFLAGS += -Wall

HEADERS += \
    App/SingleInstance.h \
    App/AppConfig.h \
    UI/FitCanvasDlg.h \
    UI/MergeTiffProcessor.h \
    UI/mainwindow.h \
    UI/qatgraphicsview.h \
    UI/GraphicsScene.h \
    UI/PropertyPanel.h \
    UI/NewFileDialog.h \
    UI/RulerBar.h \
    UI/GradientDialog.h \
    UI/AlignLayoutDialog.h \
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
    Utils/ImageWorker.h \
    Utils/ProcessGuard.h \
    Utils/ProgressManager.h \
    Utils/ColorUtils.h \
    Utils/AlignmentUtils.h \
    ColorTrans/colortransform.h \
    NetWork/QHttp.h \
    Action/QATActionBase.h \
    CommonDefs.h \
    QCommonDefs.h \
    Utils/NetWorkDefs.h \
    Utils/NetWorkUtils.h \
    Utils/ProjectFile.h \
    Tiff/tifffile.h \
    version.h \

SOURCES += \
    App/SingleInstance.cpp \
    App/AppConfig.cpp \
    App/main.cpp \
    UI/FitCanvasDlg.cpp \
    UI/MergeTiffProcessor.cpp \
    UI/mainwindow.cpp \
    UI/qatgraphicsview.cpp \
    UI/GraphicsScene.cpp \
    UI/PropertyPanel.cpp \
    UI/NewFileDialog.cpp \
    UI/RulerBar.cpp \
    UI/GradientDialog.cpp \
    UI/AlignLayoutDialog.cpp \
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
    Utils/ImageWorker.cpp \
    Utils/ProcessGuard.cpp \
    Utils/ProgressManager.cpp \
    Utils/ColorUtils.cpp \
    Utils/AlignmentUtils.cpp \
    ColorTrans/colortransform.cpp \
    NetWork/QHttp.cpp \
    Action/QATActionBase.cpp \
    Utils/NetWorkUtils.cpp \
    Utils/ProjectFile.cpp \
    Tiff/tifffile.cpp \

FORMS += \
    UI/FitCanvasDlg.ui \
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
    $$PWD/NetWork \
    $$PWD/Tiff \
    
include(../Common.pri)
