
TEMPLATE = app

QT += core gui widgets network xml concurrent

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
# LIBS += -L$$DESTDIR -lQtitanBase
LIBS += -L$$DESTDIR -lQtitanDocking
# LIBS += -L$$DESTDIR -lQtitanStyle
LIBS += -L$$DESTDIR -lQRuler


HEADERS += \
    App/SingleInstance.h \
    App/AppConfig.h \
    UI/FitCanvasDlg.h \
    UI/ImageArrangementDialog.h \
    UI/mainwindow.h \
    UI/qatgraphicsview.h \
    UI/GraphicsScene.h \
    UI/PropertyPanel.h \
    UI/NewFileDialog.h \
    UI/ResizeCanvasDialog.h \
    UI/GradientDialog.h \
    UI/AlignWidget.h \
    UI/AutoLayoutDialog.h \
    UI/AdvancedLayoutDialog.h \
    UI/SettingsDialog.h \
    UI/PreferencesDialog.h \
    UI/PreferencesPage.h \
    UI/GeneralPage.h \
    UI/CanvasPage.h \
    UI/TaskHistoryPopup.h \
    UI/MenuBarBuilder.h \
    UI/ToolBarDirector.h \
    UI/StatusBarDirector.h \
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
    Utils/ImageCacheManager.h \
    Utils/ImageUtils.h \
    Utils/ImageWorker.h \
    Utils/ProcessGuard.h \
    Utils/ProgressManager.h \
    Utils/SceneToJsonConverter.h \
    Utils/ColorUtils.h \
    Utils/AlignmentUtils.h \
    Utils/LayoutEngine.h \
    Utils/SessionFile.h \
    NetWork/QHttp.h \
    Action/QAtActionBase.h \
    Action/ActionContext.h \
    Action/QAtFileAction.h \
    Action/QAtEditAction.h \
    Action/QAtArrangeAction.h \
    Action/QAtViewAction.h \
    Action/QAtDialogAction.h \
    Action/QAtDrawAction.h \
    Action/FileActions.h \
    Action/EditActions.h \
    Action/ArrangeActions.h \
    Action/ViewActions.h \
    Action/DialogActions.h \
    Core/QAtService.h \
    Core/AppContext.h \
    Page/QAtPage.h \
    Page/QAtCanvasPage.h \
    Page/QAtDataTablePage.h \
    Core/PageManager.h \
    Core/ThemeService.h \
    Core/ClipboardService.h \
    Core/ProjectDocument.h \
    Core/RecoveryManager.h \
    Service/ProgressService.h \
    Service/NetworkService.h \
    Service/ProcessService.h \
    Service/ProjectService.h \
    Service/UndoService.h \
    CommonDefs.h \
    QCommonDefs.h \
    Utils/NetWorkDefs.h \
    Utils/NetWorkUtils.h \
    Utils/ProjectFile.h \
    Tiff/tifffile.h \
    atVersion.h \
    atPresets.h \
    atDebug.h \
    atDefine.h \
    atMath.h \

SOURCES += \
    App/SingleInstance.cpp \
    App/AppConfig.cpp \
    App/main.cpp \
    UI/FitCanvasDlg.cpp \
    UI/ImageArrangementDialog.cpp \
    UI/mainwindow.cpp \
    UI/qatgraphicsview.cpp \
    UI/GraphicsScene.cpp \
    UI/PropertyPanel.cpp \
    UI/NewFileDialog.cpp \
    UI/ResizeCanvasDialog.cpp \
    UI/GradientDialog.cpp \
    UI/AlignWidget.cpp \
    UI/AutoLayoutDialog.cpp \
    UI/AdvancedLayoutDialog.cpp \
    UI/SettingsDialog.cpp \
    UI/PreferencesDialog.cpp \
    UI/GeneralPage.cpp \
    UI/CanvasPage.cpp \
    UI/TaskHistoryPopup.cpp \
    UI/MenuBarBuilder.cpp \
    UI/ToolBarDirector.cpp \
    UI/StatusBarDirector.cpp \
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
    Utils/ImageCacheManager.cpp \
    Utils/ImageUtils.cpp \
    Utils/ImageWorker.cpp \
    Utils/ProcessGuard.cpp \
    Utils/ProgressManager.cpp \
    Utils/SceneToJsonConverter.cpp \
    Utils/ColorUtils.cpp \
    Utils/AlignmentUtils.cpp \
    Utils/LayoutEngine.cpp \
    Utils/SessionFile.cpp \
    NetWork/QHttp.cpp \
    Action/QAtActionBase.cpp \
    Action/ActionContext.cpp \
    Action/QAtEditAction.cpp \
    Action/QAtArrangeAction.cpp \
    Action/QAtViewAction.cpp \
    Action/QAtDrawAction.cpp \
    Action/FileActions.cpp \
    Action/EditActions.cpp \
    Action/ArrangeActions.cpp \
    Action/ViewActions.cpp \
    Action/DialogActions.cpp \
    Core/AppContext.cpp \
    Core/PageManager.cpp \
    Core/ThemeService.cpp \
    Core/ClipboardService.cpp \
    Core/ProjectDocument.cpp \
    Core/RecoveryManager.cpp \
    Page/QAtCanvasPage.cpp \
    Page/QAtDataTablePage.cpp \
    Service/ProgressService.cpp \
    Service/NetworkService.cpp \
    Service/ProcessService.cpp \
    Service/ProjectService.cpp \
    Service/UndoService.cpp \
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
    $$PWD/Page \
    $$PWD/Items \
    $$PWD/Commands \
    $$PWD/Utils \
    $$PWD/NetWork \
    $$PWD/Tiff \
    $$PWD/Core \
    $$PWD/Service \
    $$PWD/../Libs \
    $$PWD/../Libs/Common \
    $$PWD/../Libs/ColorTrans \
    $$PWD/../Libs/Qt-Color-Widgets/include \
    $$PWD/../Libs/Qt-Color-Widgets/src \
    $$PWD/../Libs/QtGradientEditor \
    $$PWD/../Libs/QSimpleUpdater/include \
    $$PWD/../Libs/QSimpleUpdater/src \
    $$PWD/../Libs/Layout \
    $$PWD/../Libs/Qtitan/include \
    $$PWD/../Libs/QRuler/include

# ============================================================
# 旧导出路径（TiffExportEngine + StripPipeline）
# 定义 USE_LEGACY_EXPORT 以启用，默认禁用
#   DEFINES += USE_LEGACY_EXPORT
# ============================================================
contains(DEFINES, USE_LEGACY_EXPORT) {
    HEADERS += \
        UI/TiffExportEngine.h \
        Utils/ScopedTiffHandle.h \
        Utils/SourceReader.h \
        Utils/TiffExportPipeline.h

    SOURCES += \
        UI/TiffExportEngine.cpp \
        Utils/SourceReader.cpp \
        Utils/TiffExportPipeline.cpp
}
