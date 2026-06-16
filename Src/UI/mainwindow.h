#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "PropertyPanel.h"
#include "ImageUtils.h"
#include "ImageWorker.h"
#include "ProgressManager.h"
#include "qatgraphicsview.h"
#include "AlignmentUtils.h"
#include "NetWorkUtils.h"
#include "ProcessGuard.h"
#include "Core/ProjectDocument.h"
#include "atDefine.h"

#include <QMainWindow>
#include <QDockWidget>
#include <QMap>
#include <QUndoStack>
#include <QProgressBar>
#include <QTimer>
#include <QFuture>
#include <QPointer>

class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QToolBar;
class QToolButton;
class QMenu;
class AlignWidget;
class AutoLayoutDialog;
class LayoutEngine;
class TaskHistoryPopup;
class ToolBarDirector;
class StatusBarDirector;
class QAtCanvasPage;

#ifdef USE_LEGACY_EXPORT
class TiffExportEngine;
#endif

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void setMainWindowVisibility(bool state);

    void getToolInfo();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onNew(); // toolbar: add canvas to current project
    void onNewProject(); // menubar: create a new project (prompts for path)
    void onOpenProject();
    void onSaveProject();
    void onImportImage();
    void onExportImage();
    void onFitCanvasToItems();
    void onResizeCanvas();
    void onAlignLeft();
    void onAlignRight();
    void onAlignTop();
    void onAlignBottom();
    void onAlignHCenter();
    void onAlignVCenter();
    void onDistributeH();
    void onDistributeV();
    void onSelectionChanged();
    void onItemAdded(QGraphicsItem *item);

    // 属性变更槽
    void onPenChanged(QGraphicsItem *item, const QPen &oldPen, const QPen &newPen);
    void onBrushChanged(QGraphicsItem *item, const QBrush &oldBrush, const QBrush &newBrush);
    void onFontChanged(QGraphicsItem *item, const QFont &oldFont, const QFont &newFont);
    void onTextChanged(QGraphicsItem *item, const QString &oldText, const QString &newText);
    void onGeometryChanged(QGraphicsItem *item, const QRectF &oldRect, const QRectF &newRect);
    void onCornerRadiusChanged(QGraphicsItem *item, qreal oldR, qreal newR);
    void onPositionChanged(QGraphicsItem *item, const QPointF &oldPos, const QPointF &newPos);
    void onRotationChanged(QGraphicsItem *item, qreal oldRotation, qreal newRotation);
    void onRequestFinished(const QJsonDocument &json, NetworkRequestType type);
    void onAutoLayout();
    void onUpdateInfo();

signals:
    // 新导出流程信号（跨线程进度报告）
    void exportProgress(int percent);
    void exportComplete(const QString &filePath);
    void exportError(const QString &errorMessage);

private:
    void _initWidget();
    void _initMenuBar();
    void _initToolBar();
    void _initPropertyPanel();
    void _initRulers();
    void _initConnections();
    void _bindViewConnections(); // rebind on tab switch
    void _initStatusBar();
    void _initNetWork();
    void _initProcess();
    // New architecture initialization (P1-P7)
    void _initServices();
    void _initPages();
    void _initActions();

    // 窗口状态持久化
    void loadWindowState();
    void saveWindowState();

    void importSingleImage(const QStringList &paths, QAtGraphicsView *view, QUndoStack *undoStack,
                           QPointer<QAtCanvasPage> page, CanvasItem *canvas);
    void importMultipleImages(const QStringList &paths, QAtGraphicsView *view,
                              QUndoStack *undoStack, QPointer<QAtCanvasPage> page,
                              CanvasItem *canvas);

    // DPI 锁定已移除 — 画布使用固定 150 PPI，允许任意 DPI 图片导入

    QList<QGraphicsItem *> filterSelectableItems() const;

    bool _maybeSaveProject(); // 提示保存整个工程，返回 false 表示用户取消操作
    bool _maybeCloseCanvas(QAtCanvasPage *page); // 提示关闭画布（图元将丢失），返回 false 表示取消
    bool _syncSaveAllCanvases(); // 同步保存所有画布到 m_projectPath
    QList<CanvasSaveBundle> _collectCanvasBundles() const; // 公共：收集所有画布的序列化数据
    void _addCanvasDock(const QString &title, const QString &pageId); // 创建画布 dock 页
    void _updateCanvasDockTitle(QAtCanvasPage *page); // 更新画布 dock 标题

    // ---- Canvas Dock 管理 (替代 QTabWidget API) ----
    QAtCanvasPage *_currentCanvasPage() const;
    QAtCanvasPage *_canvasPageAt(int index) const;
    int _canvasCount() const;
    int _currentCanvasIndex() const;
    int _indexOfCanvasPage(QAtCanvasPage *page) const;
    void _setCurrentCanvasPage(QAtCanvasPage *page);
    void _setCurrentCanvasIndex(int index);
    QDockWidget *_addCanvasDockInternal(QAtCanvasPage *page, const QString &title);
    void _removeCanvasDockInternal(int index);
    void _onCanvasDockActivated(QDockWidget *dock);
    bool eventFilter(QObject *obj, QEvent *event) override;

    /// 项目加载后异步刷新图元缩略图（缓存感知，不阻塞 UI）
    void refreshImageItemsFromCache(const QList<QGraphicsItem *> &items);

    // 对齐/分布辅助方法
    void applyAlign(AlignmentUtils::AlignDirection direction);
    void applyDistribute(
        AlignmentUtils::DistributeDirection direction,
        const AlignmentUtils::DistributeParams &params = AlignmentUtils::DistributeParams());

    Ui::MainWindow *ui;

    // Canvas dock management (replaces QTabWidget — each canvas is a QDockWidget)
    QList<QDockWidget *> m_canvasDocks; // all canvas dock widgets in insertion order
    QDockWidget *m_activeCanvasDock = nullptr; // currently active/focused canvas dock
    QAtGraphicsView *m_pView = nullptr;
    PropertyPanel *m_pPropertyPanel = nullptr;
    QUndoStack *m_undoStack = nullptr;

    // P6: UI builders for page-type-aware behavior
    ToolBarDirector *m_toolBarDirector = nullptr;
    StatusBarDirector *m_statusBarDirector = nullptr;

    NetWorkUtils *m_pNetWorkUtils = nullptr;

#ifdef USE_LEGACY_EXPORT
    TiffExportEngine *m_tiffEngine = nullptr; // TIFF 导出模块
#endif
    QString m_exportTaskId; // 当前导出进度任务 ID

    // 当前导出的 RIP 配置（由 onExportImage 设置，exportFinished 回调中使用）
    bool m_ripEnabled = false;
    int m_ripXRes = 0;
    int m_ripYRes = 0;

    QString m_projectPath; // 当前工程文件路径（一个工程包含多个画布），空表示未保存
    bool m_projectModified = false; // 工程文件中是否有画布未保存

    // P9: per-tab canvas modified state (projectPath is shared at project level)
    struct TabProjectState
    {
        bool modified = false;
    };
    QMap<class QAtCanvasPage *, TabProjectState> m_tabStates;

    TabProjectState &_activeTabState();
    const TabProjectState &_activeTabState() const;

    ProjectDocument *m_document = nullptr; // 工程文档模型（逐步替代上述散落字段）

    ImageUtils::ImageImportPipeline m_importSinglePipeline; // 单图导入处理管线
    ImageUtils::ImageImportPipeline m_importMultiPipeline; // 批量导入处理管线

    Tool m_currentTool = Tool::Select;

    // 对齐与布局对话框（非模态单例）
    AlignWidget *m_alignLayoutDlg = nullptr;

    // 自动排版引擎
    LayoutEngine *m_layoutEngine = nullptr;

    // 新导出流程：后台 ExportEngine 渲染任务
    QFuture<void> m_exportFuture;
    void exportWithEngine(const QString &json, const QString &outputPath);

    // 网格显示切换
    QAction *m_gridAction = nullptr;

    // 主题切换
    QAction *m_lightThemeAction = nullptr;
    QAction *m_darkThemeAction = nullptr;
    QString m_currentTheme;
    void _initThemeMenu(QMenu *viewMenu);
    void switchTheme(const QString &theme);

    // 通用进度管理器
    ProgressManager *m_pProgressMgr = nullptr;

    // 任务历史按钮与弹窗
    QToolButton *m_taskHistoryBtn = nullptr;
    TaskHistoryPopup *m_taskHistoryPopup = nullptr;
    void _toggleHistoryPopup();

    // 进程管理器
    ProcessGuard *m_pProcessGuard = nullptr;

    // 当前 RIP 进度任务 ID
    QString m_ripTaskId;

    // 状态栏控件
    QLabel *m_posLabel = nullptr;
    QLabel *m_canvasLabel = nullptr;
    QLabel *m_toolLabel = nullptr;
    QSlider *m_zoomSlider = nullptr;
    QLineEdit *m_zoomEdit = nullptr;
    QLabel *m_zoomPctLabel = nullptr;
    QToolButton *m_zoomPresetBtn = nullptr;
    QMenu *m_zoomPresetMenu = nullptr;
    QToolButton *m_zoomOutBtn = nullptr;
    QToolButton *m_zoomInBtn = nullptr;
    QToolButton *m_resizeCanvasBtn = nullptr;

    // 状态栏辅助
    QPointF m_lastScenePos; // 最近一次鼠标场景坐标
    void _updatePosLabel(const QPointF &scenePos); // 根据单位模式更新坐标标签
    void saveSession();
    void loadSession();
    void _syncViewState(); // sync ruler / propertyPanel / labels from active view
    void _updateCanvasLabel(); // 根据单位模式更新画布尺寸标签
    void _updateToolLabel(); // 根据当前工具更新工具标签
};

#endif // MAINWINDOW_H
