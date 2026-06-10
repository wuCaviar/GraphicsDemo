
TEMPLATE = app

QT += core gui widgets network xml concurrent openglwidgets

TARGET = ATGraphics

# 启用 RTTI（AlignmentUtils 使用 dynamic_cast<IGraphicsItem*>）
CONFIG += rtti c++17

# 编译器警告
QMAKE_CXXFLAGS += -Wall

include(../Common.pri)

# 链接 Libs/ 子库（DESTDIR 已由 Common.pri 分离 Debug/Release）
LIBS += -L$$DESTDIR -lColorTrans
LIBS += -L$$DESTDIR -lQtColorWidgets
LIBS += -L$$DESTDIR -lQtGradientEditor
LIBS += -L$$DESTDIR -lQSimpleUpdater
LIBS += -L$$DESTDIR -lLayout


HEADERS += \
    App/SingleInstance.h \
    App/AppConfig.h \
    UI/FitCanvasDlg.h \
    UI/ImageArrangementDialog.h \
    UI/MergeTiffProcessor.h \
    UI/TiffExportEngine.h \
    UI/mainwindow.h \
    UI/qatgraphicsview.h \
    UI/GraphicsScene.h \
    UI/PropertyPanel.h \
    UI/NewFileDialog.h \
    UI/ResizeCanvasDialog.h \
    UI/RulerBar.h \
    UI/GradientDialog.h \
    UI/AlignLayoutDialog.h \
    UI/AutoLayoutDialog.h \
    UI/AdvancedLayoutDialog.h \
    UI/SettingsDialog.h \
    UI/PreferencesDialog.h \
    UI/PreferencesPage.h \
    UI/GeneralPage.h \
    UI/CanvasPage.h \
    UI/TaskHistoryPopup.h \
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
    Utils/ScopedTiffHandle.h \
    Utils/SourceReader.h \
    Utils/TiffExportPipeline.h \
    Utils/ProcessGuard.h \
    Utils/ProgressManager.h \
    Utils/SceneToJsonConverter.h \
    Utils/ColorUtils.h \
    Utils/AlignmentUtils.h \
    Utils/LayoutEngine.h \
    NetWork/QHttp.h \
    Action/QAtActionBase.h \
    Action/QAtDrawAction.h \
    CommonDefs.h \
    QCommonDefs.h \
    Utils/NetWorkDefs.h \
    Utils/NetWorkUtils.h \
    Utils/ProjectFile.h \
    Tiff/tifffile.h \
    version.h \
    ATHCPresets.h \

SOURCES += \
    App/SingleInstance.cpp \
    App/AppConfig.cpp \
    App/main.cpp \
    UI/FitCanvasDlg.cpp \
    UI/ImageArrangementDialog.cpp \
    UI/MergeTiffProcessor.cpp \
    UI/TiffExportEngine.cpp \
    UI/mainwindow.cpp \
    UI/qatgraphicsview.cpp \
    UI/GraphicsScene.cpp \
    UI/PropertyPanel.cpp \
    UI/NewFileDialog.cpp \
    UI/ResizeCanvasDialog.cpp \
    UI/RulerBar.cpp \
    UI/GradientDialog.cpp \
    UI/AlignLayoutDialog.cpp \
    UI/AutoLayoutDialog.cpp \
    UI/AdvancedLayoutDialog.cpp \
    UI/SettingsDialog.cpp \
    UI/PreferencesDialog.cpp \
    UI/GeneralPage.cpp \
    UI/CanvasPage.cpp \
    UI/TaskHistoryPopup.cpp \
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
    Utils/SourceReader.cpp \
    Utils/TiffExportPipeline.cpp \
    Utils/ProcessGuard.cpp \
    Utils/ProgressManager.cpp \
    Utils/SceneToJsonConverter.cpp \
    Utils/ColorUtils.cpp \
    Utils/AlignmentUtils.cpp \
    Utils/LayoutEngine.cpp \
    NetWork/QHttp.cpp \
    Action/QAtActionBase.cpp \
    Action/QAtDrawAction.cpp \
    Utils/NetWorkUtils.cpp \
    Utils/ProjectFile.cpp \
    Tiff/tifffile.cpp \

FORMS += \
    UI/FileInfoWidget.ui \
    UI/FitCanvasDlg.ui \
    UI/ImageArrangementDialog.ui \
    UI/mainwindow.ui \

RESOURCES += \
    translations/translations.qrc \
    resources/resources.qrc \
    resources/theme/dark/darkstyle.qrc \
    resources/theme/light/lightstyle.qrc \

TRANSLATIONS += \
    translations/GraphicsDemo_zh_CN.ts \


INCLUDEPATH += \
    $$PWD/App \
    $$PWD/UI \
    $$PWD/Action \
    $$PWD/Items \
    $$PWD/Commands \
    $$PWD/Utils \
    $$PWD/NetWork \
    $$PWD/Tiff \
    $$PWD/../Libs \
    $$PWD/../Libs/Common \
    $$PWD/../Libs/ColorTrans \
    $$PWD/../Libs/Qt-Color-Widgets/include \
    $$PWD/../Libs/Qt-Color-Widgets/src \
    $$PWD/../Libs/QtGradientEditor \
    $$PWD/../Libs/QSimpleUpdater/include \
    $$PWD/../Libs/QSimpleUpdater/src \
    $$PWD/../Libs/Layout \
