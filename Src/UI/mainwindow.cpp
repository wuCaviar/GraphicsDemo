#include "mainwindow.h"
#include "atMath.h"
#include "ui_mainwindow.h"

#include <QMainWindow>
#include <QDockWidget>

#include "AlignWidget.h"
#include "AutoLayoutDialog.h"
#include "LayoutEngine.h"
#include "AlignmentUtils.h"
#include "AppConfig.h"
#include "BezierCurveItem.h"
#include "CanvasItem.h"
#include "Commands.h"
#include "EllipseItem.h"

#include "ImageArrangementDialog.h"
#include "ImageItem.h"
#include "NewFileDialog.h"
#include "ImageUtils.h"
#include "ImageCacheManager.h"
#include "ProjectFile.h"

#include "LineItem.h"

#include "ResizeCanvasDialog.h"
#include "SettingsDialog.h"
#include "PreferencesDialog.h"
#include "RectItem.h"
#include "ResizeHandleItem.h"
#include "TextItem.h"
// Strip write mode by default; comment out to switch to tile-based write.
#define EXPORT_USE_STRIP_WRITE

#ifdef USE_LEGACY_EXPORT
#    include "TiffExportEngine.h"
#endif
#include "TaskHistoryPopup.h"
#include "GraphicsItemGroup.h"
#include "FitCanvasDlg.h"
#include "SceneToJsonConverter.h"
#include "atDefine.h"

// New architecture (P1-P7)
#include "AppContext.h"
#include "PageManager.h"
#include "ThemeService.h"
#include "ClipboardService.h"
#include "ProgressService.h"
#include "NetworkService.h"
#include "ProcessService.h"
#include "ProjectService.h"
#include "UndoService.h"
#include "QAtCanvasPage.h"
#include "FileActions.h"
#include "EditActions.h"
#include "ViewActions.h"
#include "DialogActions.h"
#include "ArrangeActions.h"
#include "QAtDrawAction.h"
#include "ToolBarDirector.h"
#include "StatusBarDirector.h"
#include "SessionFile.h"
#include "RecoveryManager.h"

// ExportEngine API（新导出路径）
#include <ExportEngine/ExportEngine.h>

#include <algorithm>
#include <atomic>
#include <memory>
#include <cmath>

#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QCloseEvent>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QImageWriter>
#include <QImage>
#include <QImageReader>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMessageBox>
#include <QProgressDialog>
#include <QLabel>
#include <QMap>
#include <QMimeData>
#include <QPointer>
#include <QPainter>
#include <QPushButton>
#include <QToolButton>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QSlider>
#include <QIntValidator>
#include <QStyle>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QThreadPool>
#include <QLocale>

MainWindow::TabProjectState &MainWindow::_activeTabState()
{
    auto *page = _currentCanvasPage();
    if (!page) {
        static TabProjectState s;
        return s;
    }
    return m_tabStates[page];
}

const MainWindow::TabProjectState &MainWindow::_activeTabState() const
{
    auto *page = const_cast<MainWindow *>(this)->_currentCanvasPage();
    if (!page) {
        static const TabProjectState s;
        return s;
    }
    return const_cast<MainWindow *>(this)->m_tabStates[page];
}

namespace {
QFrame *createStatusSeparator(QWidget *parent)
{
    auto *line = new QFrame(parent);
    line->setObjectName("StatusBarSeparator");
    line->setFrameShape(QFrame::VLine);
    line->setFrameShadow(QFrame::Plain);
    line->setLineWidth(1);
    line->setFixedHeight(16);
    return line;
}
} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 创建工程文档模型（逐步替代散落的 m_projectPath/m_projectModified/m_tabStates）
    m_document = new ProjectDocument(this);

    // New architecture: create canvas page (owns view + scene + undoStack)
    _initPages();

    // Register services and actions BEFORE menu/toolbar so they can use AppContext::getQAction()
    _initServices();
    _initActions();

    _initWidget();
    _initPropertyPanel();
    _initRulers();
    _initMenuBar();
    _initToolBar();
    // Sync all lazily-created QActions with current state
    AppContext::get().refreshAllActions();
    _initConnections();
    _initStatusBar();
    _initProcess();
    _initNetWork();

    // P6: Wire ToolBarDirector for page-type-aware toolbar visibility
    m_toolBarDirector = new ToolBarDirector(this);
    {
        QToolBar *fileEditBar = findChild<QToolBar *>("FileEditToolBar");
        QToolBar *drawBar = findChild<QToolBar *>("DrawingToolBar");
        QToolBar *alignBar = findChild<QToolBar *>("AlignToolBar");
        if (fileEditBar)
            m_toolBarDirector->addToolBar(fileEditBar); // always visible
        if (drawBar)
            m_toolBarDirector->addToolBar(drawBar, { QStringLiteral("canvas") });
        if (alignBar)
            m_toolBarDirector->addToolBar(alignBar, { QStringLiteral("canvas") });
    }
    connect(&AppContext::get(), &AppContext::pageSwitched, m_toolBarDirector,
            &ToolBarDirector::onPageSwitched);
    // Apply initial visibility for the already-active page
    m_toolBarDirector->applyVisibility(PageManager::get().activePageType());

    // P6: Wire StatusBarDirector for signal rebinding on page switch
    m_statusBarDirector = new StatusBarDirector(this);
    m_statusBarDirector->setPositionLabel(m_posLabel);
    m_statusBarDirector->setZoomControls(m_zoomEdit, m_zoomPctLabel, m_zoomPresetBtn,
                                         m_zoomPresetMenu, m_zoomOutBtn, m_zoomInBtn,
                                         m_zoomSlider);
    m_statusBarDirector->setCanvasLabel(m_canvasLabel);
    m_statusBarDirector->setToolLabel(m_toolLabel);
    connect(&AppContext::get(), &AppContext::pageSwitched, m_statusBarDirector,
            &StatusBarDirector::onPageSwitched);
    // Trigger initial binding for the already-active page
    m_statusBarDirector->onPageSwitched(AppContext::get().activePageId(),
                                        PageManager::get().activePageType());

    // Sync PPI and labels from the initial canvas
    _syncViewState();

    setWindowTitle(tr("AT Drawing Tools"));
    resize(1200, 800);

    // Restore saved session (tabs / tools / projects)
    loadSession();

    // 加载窗口状态（工具栏位置、可见性等）
    loadWindowState();

    // 检查是否有上次崩溃退出快照（正常退出后此文件应由上次启动清除）
    // 若残留 → 说明上次异常退出，弹出恢复对话框
    if (RecoveryManager::instance().hasQuitSnapshot()) {
        auto snap = RecoveryManager::instance().loadQuitSnapshot();
        if (snap.isValid && !snap.canvases.isEmpty()) {
            int totalItems = 0;
            for (const auto &c : snap.canvases)
                totalItems += c.tasks.size();

            QLocale locale;
            QString strTime = locale.toString(snap.timestamp, QLocale::ShortFormat);
            QMessageBox::StandardButton btn = QMessageBox::question(
                this, tr("Recovery"),
                tr("ATGraphics did not exit cleanly last time.\n\n"
                   "Project: %1\n"
                   "Canvases: %2\n"
                   "Items: %3\n"
                   "Time: %4\n\n"
                   "Recover?")
                    .arg(snap.projectPath.isEmpty() ? tr("Untitled") : snap.projectPath)
                    .arg(snap.canvases.size())
                    .arg(totalItems)
                    .arg(strTime),
                QMessageBox::Yes | QMessageBox::No);
            if (btn == QMessageBox::Yes) {
                // 加载退出快照数据 — 匹配现有 Tab 或动态创建新 Tab
                for (int ci = 0; ci < snap.canvases.size(); ++ci) {
                    auto &bundle = snap.canvases[ci];

                    // 如果快照画布多于现有 Tab，动态创建新页
                    QAtCanvasPage *page = nullptr;
                    if (ci < _canvasCount()) {
                        page = qobject_cast<QAtCanvasPage *>(_canvasPageAt(ci));
                    } else {
                        QSizeF sz(bundle.info.width > 0 ? bundle.info.width : 1920,
                                  bundle.info.height > 0 ? bundle.info.height : 1080);
                        qreal ppi = bundle.info.dpi > 0 ? bundle.info.dpi : 150.0;
                        page = new QAtCanvasPage(QStringLiteral("recovery-%1").arg(ci + 1), sz, ppi,
                                                 this);
                        AppContext::get().registerPage(page);
                        _addCanvasDockInternal(page, page->title());
                        m_tabStates[page].modified = false;
                    }
                    if (!page)
                        continue;

                    qreal ppi = bundle.info.dpi > 0 ? bundle.info.dpi : 150.0;
                    page->setCanvasSize(QSizeF(bundle.info.width, bundle.info.height));
                    if (auto *c = page->canvasItem()) {
                        c->setPpi(ppi);
                        if (bundle.info.dpi > 0)
                            c->setCanvasDpi(bundle.info.dpi, bundle.info.dpi);
                    }
                    for (const auto &task : bundle.tasks) {
                        auto di = deserializeItemWorker(task);
                        auto *item = createItemFromDeserialized(di);
                        if (item)
                            page->scene()->addItem(item);
                    }
                    m_tabStates[page].modified = true;
                    _updateCanvasDockTitle(page);
                }
                m_projectModified = true;
            }
        }
        RecoveryManager::instance().discardQuitSnapshot();
    }
}

MainWindow::~MainWindow()
{
    // 断开 view 信号，防止析构期间信号触发访问半销毁状态的对象
    if (m_pView)
        m_pView->disconnect();

    // 清空属性面板引用，避免悬空指针
    if (m_pPropertyPanel)
        m_pPropertyPanel->setItem(nullptr);

    // 清空 undo 栈，避免命令引用已销毁的图元
    if (m_undoStack)
        m_undoStack->clear();

    if (m_pProcessGuard) {
        m_pProcessGuard->stopAll();
    }

    delete ui;
}

void MainWindow::_initWidget()
{
    // View already created and enabled by QAtCanvasPage in _initPages()

#ifdef USE_LEGACY_EXPORT
    // TIFF 导出引擎：管理场景检查、Overlay 渲染、后台线程导出
    m_tiffEngine = new TiffExportEngine(this);

    connect(m_tiffEngine, &TiffExportEngine::progressChanged, this,
            [this](int pct) { m_pProgressMgr->updateTask(m_exportTaskId, pct); });

    connect(m_tiffEngine, &TiffExportEngine::exportFinished, this,
            [this](bool success, const QString &filePath, const QString &errorMessage) {
                if (success) {
                    // 导出成功后，在主线程发起 RIP 添加请求
                    if (m_ripEnabled && m_pNetWorkUtils
                        && m_pProcessGuard->isProcessRunning(AppConfig::instance().ripExePath())) {
                        m_pNetWorkUtils->doAddRip(m_ripXRes, m_ripYRes, filePath);
                    }
                    m_pProgressMgr->finishTask(m_exportTaskId);
                    setEnabled(false); // 导出完成后禁止操作，等待 RIP 结果
                } else {
                    m_pProgressMgr->cancelTask(m_exportTaskId);
                    if (!errorMessage.isEmpty()) {
                        QMessageBox::warning(this, tr("Export"),
                                             tr("Export failed: %1").arg(errorMessage));
                    }
                }
            });
#endif

    // 新导出流程：ExportEngine 进度与完成信号连接
    connect(
        this, &MainWindow::exportProgress, this,
        [this](int pct) { m_pProgressMgr->updateTask(m_exportTaskId, pct); }, Qt::QueuedConnection);

    connect(
        this, &MainWindow::exportComplete, this,
        [this](const QString &filePath) {
            m_pProgressMgr->finishTask(m_exportTaskId);
            if (m_ripEnabled && m_pNetWorkUtils
                && m_pProcessGuard->isProcessRunning(AppConfig::instance().ripExePath())) {
                m_pNetWorkUtils->doAddRip(m_ripXRes, m_ripYRes, filePath);
                setEnabled(false); // 导出完成后禁止操作，等待 RIP 结果
            }
        },
        Qt::QueuedConnection);

    connect(
        this, &MainWindow::exportError, this,
        [this](const QString &msg) {
            m_pProgressMgr->cancelTask(m_exportTaskId);
            QMessageBox::warning(this, tr("Export"), tr("Export failed: %1").arg(msg));
        },
        Qt::QueuedConnection);
}

void MainWindow::_initRulers()
{
    // Rulers are now owned by each QAtCanvasPage (one canvas = one ruler pair).
    // The central widget is a placeholder — canvas dock widgets occupy the dock areas.
    // Nothing else needed here.
}

void MainWindow::_initMenuBar()
{
    QMenuBar *menu = ui->menubar;

    // ---- 文件 ----
    QMenu *fileMenu = menu->addMenu(tr("&File"));

    QAction *newAct =
        fileMenu->addAction(QIcon(":/icons/icons/file-new.svg"), tr("&New Project..."));
    newAct->setShortcut(QKeySequence::New);
    newAct->setToolTip(tr("Create a new project"));
    connect(newAct, &QAction::triggered, this, &MainWindow::onNewProject);

    fileMenu->addSeparator();

    QAction *openProjAct = fileMenu->addAction(style()->standardIcon(QStyle::SP_DialogOpenButton),
                                               tr("&Open Project..."));
    openProjAct->setShortcut(QKeySequence::Open);
    openProjAct->setToolTip(tr("Open a project file"));
    connect(openProjAct, &QAction::triggered, this, &MainWindow::onOpenProject);

    QAction *saveProjAct = fileMenu->addAction(style()->standardIcon(QStyle::SP_DialogSaveButton),
                                               tr("&Save Project..."));
    saveProjAct->setShortcut(QKeySequence::Save);
    saveProjAct->setToolTip(tr("Save the current project"));
    connect(saveProjAct, &QAction::triggered, this, &MainWindow::onSaveProject);

    fileMenu->addSeparator();

    QAction *importAct =
        fileMenu->addAction(QIcon(":/icons/icons/file-import.svg"), tr("&Import Image..."));
    importAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
    importAct->setToolTip(tr("Import an image onto the canvas"));
    connect(importAct, &QAction::triggered, this, &MainWindow::onImportImage);

    QAction *exportAct =
        fileMenu->addAction(QIcon(":/icons/icons/file-export.svg"), tr("&Export Image..."));
    exportAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    exportAct->setToolTip(tr("Export the canvas to an image file"));
    connect(exportAct, &QAction::triggered, this, &MainWindow::onExportImage);

    fileMenu->addSeparator();

    QAction *exitAct = fileMenu->addAction(tr("E&xit"));
    exitAct->setShortcut(QKeySequence::Quit);
    exitAct->setToolTip(tr("Exit the application"));
    connect(exitAct, &QAction::triggered, this, &QWidget::close);

    // ---- 编辑 ----
    QMenu *editMenu = menu->addMenu(tr("&Edit"));

    QAction *undoAct = AppContext::get().getQAction(QStringLiteral("Undo"));
    if (undoAct)
        editMenu->addAction(undoAct);

    QAction *redoAct = AppContext::get().getQAction(QStringLiteral("Redo"));
    if (redoAct)
        editMenu->addAction(redoAct);

    editMenu->addSeparator();

    QAction *cutAct = AppContext::get().getQAction(QStringLiteral("Cut"));
    if (cutAct)
        editMenu->addAction(cutAct);

    QAction *copyAct = AppContext::get().getQAction(QStringLiteral("Copy"));
    if (copyAct)
        editMenu->addAction(copyAct);

    QAction *pasteAct = AppContext::get().getQAction(QStringLiteral("Paste"));
    if (pasteAct)
        editMenu->addAction(pasteAct);

    editMenu->addSeparator();

    QAction *deleteAct = AppContext::get().getQAction(QStringLiteral("Delete"));
    if (deleteAct)
        editMenu->addAction(deleteAct);

    QAction *selectAllAct = AppContext::get().getQAction(QStringLiteral("SelectAll"));
    if (selectAllAct)
        editMenu->addAction(selectAllAct);

    // Refresh action states when Edit menu is about to show
    connect(editMenu, &QMenu::aboutToShow, []() { AppContext::get().refreshAllActions(); });

    // ---- 排列 ----
    QMenu *arrMenu = menu->addMenu(tr("&Arrange"));
    QAction *bringAct = AppContext::get().getQAction(QStringLiteral("BringToFront"));
    if (bringAct)
        arrMenu->addAction(bringAct);
    QAction *sendAct = AppContext::get().getQAction(QStringLiteral("SendToBack"));
    if (sendAct)
        arrMenu->addAction(sendAct);
    arrMenu->addSeparator();
    QAction *groupAct = AppContext::get().getQAction(QStringLiteral("Group"));
    if (groupAct)
        arrMenu->addAction(groupAct);
    QAction *ungroupAct = AppContext::get().getQAction(QStringLiteral("Ungroup"));
    if (ungroupAct)
        arrMenu->addAction(ungroupAct);
    arrMenu->addSeparator();
    {
        QAction *act = AppContext::get().getQAction(QStringLiteral("AlignLayoutDialog"));
        if (act)
            arrMenu->addAction(act);
    }
    arrMenu->addSeparator();
    QAction *fitCanvasAct =
        arrMenu->addAction(tr("Fit Canvas to Selection"), this, &MainWindow::onFitCanvasToItems);
    fitCanvasAct->setToolTip(tr("Resize the canvas to fit the selected items"));
    arrMenu->addSeparator();
    QMenu *rotateMenu = arrMenu->addMenu(tr("Rotate"));
    {
        QAction *act = AppContext::get().getQAction(QStringLiteral("RotateCW"));
        if (act)
            rotateMenu->addAction(act);
    }
    {
        QAction *act = AppContext::get().getQAction(QStringLiteral("RotateCCW"));
        if (act)
            rotateMenu->addAction(act);
    }
    {
        QAction *act = AppContext::get().getQAction(QStringLiteral("Rotate180"));
        if (act)
            rotateMenu->addAction(act);
    }

    // ---- 设置 ----
    QMenu *settingsMenu = menu->addMenu(tr("&Settings"));
    {
        QAction *act = AppContext::get().getQAction(QStringLiteral("Settings"));
        if (act)
            settingsMenu->addAction(act);
    }
    settingsMenu->addSeparator();
    {
        QAction *act = AppContext::get().getQAction(QStringLiteral("Preferences"));
        if (act)
            settingsMenu->addAction(act);
    }

    // ---- 视图 ----
    QMenu *viewMenu = menu->addMenu(tr("&View"));
    viewMenu->addAction(m_pPropertyPanel->toggleViewAction());
    viewMenu->addAction(m_alignLayoutDlg->toggleViewAction());
    viewMenu->addSeparator();

    // ---- 帮助 ----
    QMenu *helpMenu = menu->addMenu(tr("&Help"));
    {
        QAction *act = AppContext::get().getQAction(QStringLiteral("About"));
        if (act)
            helpMenu->addAction(act);
    }

    // 网格显示/隐藏
    m_gridAction = AppContext::get().getQAction(QStringLiteral("ToggleGrid"));
    if (m_gridAction)
        viewMenu->addAction(m_gridAction);

    _initThemeMenu(viewMenu);

    viewMenu->addSeparator();
    // 缩放适配
    QAction *fitAct = AppContext::get().getQAction(QStringLiteral("FitToCanvas"));
    if (fitAct)
        viewMenu->addAction(fitAct);

    QAction *resetZoomAct = AppContext::get().getQAction(QStringLiteral("ResetZoom"));
    if (resetZoomAct)
        viewMenu->addAction(resetZoomAct);

    // Refresh action states when View menu is about to show
    connect(viewMenu, &QMenu::aboutToShow, []() { AppContext::get().refreshAllActions(); });
}

void MainWindow::_initThemeMenu(QMenu *viewMenu)
{
    QMenu *themeMenu = viewMenu->addMenu(tr("Theme"));

    auto *themeGroup = new QActionGroup(this);
    themeGroup->setExclusive(true);

    m_lightThemeAction = themeMenu->addAction(tr("Light"));
    m_lightThemeAction->setCheckable(true);
    themeGroup->addAction(m_lightThemeAction);

    m_darkThemeAction = themeMenu->addAction(tr("Dark"));
    m_darkThemeAction->setCheckable(true);
    themeGroup->addAction(m_darkThemeAction);

    connect(m_lightThemeAction, &QAction::triggered, this, [this]() { switchTheme("light"); });
    connect(m_darkThemeAction, &QAction::triggered, this, [this]() { switchTheme("dark"); });

    // 恢复保存的主题设置
    QSettings settings;
    QString savedTheme = settings.value("appearance/theme", "light").toString();
    switchTheme(savedTheme);
}

void MainWindow::switchTheme(const QString &theme)
{
    if (m_currentTheme == theme)
        return;

    m_currentTheme = theme;

    QString qssPath = QString(":/qdarkstyle/%1/%1style.qss").arg(theme);
    QFile f(qssPath);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qApp->setStyleSheet(f.readAll());
        f.close();
    }

    if (m_lightThemeAction)
        m_lightThemeAction->setChecked(theme == "light");
    if (m_darkThemeAction)
        m_darkThemeAction->setChecked(theme == "dark");

    QSettings().setValue("appearance/theme", theme);
}

void MainWindow::_initToolBar()
{
    // 文件 & 编辑工具栏 - 停靠在顶部
    QToolBar *fileEditBar = new QToolBar(tr("File & Edit"), this);
    fileEditBar->setObjectName("FileEditToolBar");
    fileEditBar->setMovable(true);
    fileEditBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    fileEditBar->setIconSize(QSize(20, 20));
    addToolBar(Qt::TopToolBarArea, fileEditBar);

    // New 按钮
    QAction *newAct = new QAction(QIcon(":/icons/icons/file-new.svg"), tr("New"), this);
    newAct->setToolTip(tr("Create a new canvas"));
    connect(newAct, &QAction::triggered, this, &MainWindow::onNew);
    fileEditBar->addAction(newAct);

    // Import 按钮
    QAction *importAct =
        new QAction(QIcon(":/icons/icons/file-import.svg"), tr("Import Image"), this);
    importAct->setToolTip(tr("Import an image onto the canvas"));
    connect(importAct, &QAction::triggered, this, &MainWindow::onImportImage);
    fileEditBar->addAction(importAct);

    // Export 按钮
    QAction *exportAct =
        new QAction(QIcon(":/icons/icons/file-export.svg"), tr("Export Image"), this);
    exportAct->setToolTip(tr("Export the canvas to an image file"));
    connect(exportAct, &QAction::triggered, this, &MainWindow::onExportImage);
    fileEditBar->addAction(exportAct);

    fileEditBar->addSeparator();

    fileEditBar->addAction(AppContext::get().getQAction(QStringLiteral("Undo")));
    fileEditBar->addAction(AppContext::get().getQAction(QStringLiteral("Redo")));

    fileEditBar->addSeparator();

    fileEditBar->addAction(AppContext::get().getQAction(QStringLiteral("Cut")));
    fileEditBar->addAction(AppContext::get().getQAction(QStringLiteral("Copy")));
    fileEditBar->addAction(AppContext::get().getQAction(QStringLiteral("Paste")));

    // 绘图工具栏 - 停靠在左侧
    QToolBar *drawBar = new QToolBar(tr("Drawing Tools"), this);
    drawBar->setObjectName("DrawingToolBar");
    drawBar->setMovable(true);
    drawBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    drawBar->setIconSize(QSize(20, 20));
    addToolBar(Qt::LeftToolBarArea, drawBar);

    auto *actionGroup = new QActionGroup(this);
    actionGroup->setExclusive(true);

    // Draw tools from AppContext (each QAtDrawActionBase is checkable, has icon/text)
    static const QStringList drawToolTokens = {
        QStringLiteral("SelectTool"),      QStringLiteral("RectTool"),
        QStringLiteral("EllipseTool"),     QStringLiteral("LineTool"),
        QStringLiteral("BezierCurveTool"), QStringLiteral("FreehandTool"),
        QStringLiteral("TextTool")
    };
    for (const auto &token : drawToolTokens) {
        QAction *act = AppContext::get().getQAction(token);
        if (act) {
            actionGroup->addAction(act);
            drawBar->addAction(act);
        }
    }

    // 对齐工具栏
    QToolBar *alignToolBar = new QToolBar(tr("Align"), this);
    alignToolBar->setObjectName("AlignToolBar");
    alignToolBar->setMovable(false);
    alignToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    alignToolBar->setIconSize(QSize(20, 20));
    addToolBar(Qt::TopToolBarArea, alignToolBar);

    {
        QAction *act = AppContext::get().getQAction(QStringLiteral("AlignLayoutDialog"));
        if (act)
            alignToolBar->addAction(act);
    }

    alignToolBar->addSeparator();

    // 成组/解组
    QAction *groupAct = AppContext::get().getQAction(QStringLiteral("Group"));
    if (groupAct)
        alignToolBar->addAction(groupAct);
    QAction *ungroupAct = AppContext::get().getQAction(QStringLiteral("Ungroup"));
    if (ungroupAct)
        alignToolBar->addAction(ungroupAct);

    alignToolBar->addSeparator();

    // 旋转
    {
        QAction *act = AppContext::get().getQAction(QStringLiteral("RotateCW"));
        if (act)
            alignToolBar->addAction(act);
    }
    {
        QAction *act = AppContext::get().getQAction(QStringLiteral("RotateCCW"));
        if (act)
            alignToolBar->addAction(act);
    }

    alignToolBar->addSeparator();

    // 画布适配选中图元
    QAction *fitCanvasAct =
        alignToolBar->addAction(QIcon(":/icons/icons/view-fit.svg"), tr("Fit Canvas to Selection"));
    fitCanvasAct->setToolTip(tr("Resize the canvas to fit the selected items"));
    connect(fitCanvasAct, &QAction::triggered, this, &MainWindow::onFitCanvasToItems);

    alignToolBar->addSeparator();

    // 智能排版
    QAction *autoLayoutAct =
        alignToolBar->addAction(QIcon(":/icons/icons/auto-layout.svg"), tr("Auto Layout"));
    autoLayoutAct->setToolTip(tr("Auto arrange image items on the canvas"));
    connect(autoLayoutAct, &QAction::triggered, this, &MainWindow::onAutoLayout);
}

void MainWindow::_initPropertyPanel()
{
    m_pPropertyPanel = new PropertyPanel(this);
    m_pPropertyPanel->setObjectName("PropertyPanel");
    m_pPropertyPanel->setMinimumWidth(300);
    m_pPropertyPanel->setAllowedAreas(Qt::RightDockWidgetArea); // 仅允许停靠在右侧
    addDockWidget(Qt::RightDockWidgetArea, m_pPropertyPanel);

    m_alignLayoutDlg = new AlignWidget(nullptr, nullptr, this);
    m_alignLayoutDlg->setObjectName("AlignLayoutDock");
    m_alignLayoutDlg->setMinimumWidth(300);
    m_alignLayoutDlg->setAllowedAreas(Qt::RightDockWidgetArea); // 仅允许停靠在右侧
    addDockWidget(Qt::RightDockWidgetArea, m_alignLayoutDlg);
    m_alignLayoutDlg->hide();
    AppContext::get().setAlignWidget(m_alignLayoutDlg);
}

void MainWindow::_initConnections()
{
    // Static connections (once per session)
    connect(m_pPropertyPanel, &PropertyPanel::penChanged, this, &MainWindow::onPenChanged);
    connect(m_pPropertyPanel, &PropertyPanel::brushChanged, this, &MainWindow::onBrushChanged);
    connect(m_pPropertyPanel, &PropertyPanel::fontChanged, this, &MainWindow::onFontChanged);
    connect(m_pPropertyPanel, &PropertyPanel::textChanged, this, &MainWindow::onTextChanged);
    connect(m_pPropertyPanel, &PropertyPanel::geometryChanged, this,
            &MainWindow::onGeometryChanged);
    connect(m_pPropertyPanel, &PropertyPanel::cornerRadiusChanged, this,
            &MainWindow::onCornerRadiusChanged);
    connect(m_pPropertyPanel, &PropertyPanel::positionChanged, this,
            &MainWindow::onPositionChanged);
    connect(m_pPropertyPanel, &PropertyPanel::rotationChanged, this,
            &MainWindow::onRotationChanged);

    _bindViewConnections();
}

void MainWindow::_bindViewConnections()
{
    // No active view yet — will be rebound when first canvas is created
    if (!m_pView)
        return;

    // View signals — rebound on tab switch
    connect(m_pView, &QAtGraphicsView::selectionChanged, this, &MainWindow::onSelectionChanged);
    connect(m_pView, &QAtGraphicsView::itemAdded, this, &MainWindow::onItemAdded);
    // Context-menu actions route through AppContext actions
    connect(m_pView, &QAtGraphicsView::bringToFrontRequested, this, []() {
        auto *a = AppContext::get().getQAction(QStringLiteral("BringToFront"));
        if (a)
            a->trigger();
    });
    connect(m_pView, &QAtGraphicsView::sendToBackRequested, this, []() {
        auto *a = AppContext::get().getQAction(QStringLiteral("SendToBack"));
        if (a)
            a->trigger();
    });
    connect(m_pView, &QAtGraphicsView::groupRequested, this, []() {
        auto *a = AppContext::get().getQAction(QStringLiteral("Group"));
        if (a)
            a->trigger();
    });
    connect(m_pView, &QAtGraphicsView::ungroupRequested, this, []() {
        auto *a = AppContext::get().getQAction(QStringLiteral("Ungroup"));
        if (a)
            a->trigger();
    });
    connect(m_pView, &QAtGraphicsView::fitCanvasToItemsRequested, this,
            &MainWindow::onFitCanvasToItems);

    // Undo stack — rebound on tab switch (use AppContext refresh)
    connect(m_undoStack, &QUndoStack::canUndoChanged, this,
            []() { AppContext::get().refreshAllActions(); });
    connect(m_undoStack, &QUndoStack::canRedoChanged, this,
            []() { AppContext::get().refreshAllActions(); });
    connect(m_undoStack, &QUndoStack::indexChanged, this, [this]() {
        m_pView->refreshResizeHandle();
        _updateCanvasLabel();
        _activeTabState().modified = true;
        m_projectModified = true;
        // Show modified indicator (*) in tab title
        auto *page = qobject_cast<QAtCanvasPage *>(_currentCanvasPage());
        if (page)
            _updateCanvasDockTitle(page);
        if (m_pPropertyPanel && m_pPropertyPanel->currentItem())
            m_pPropertyPanel->setItem(m_pPropertyPanel->currentItem());
    });

    // Ruler sync is handled internally by each QAtCanvasPage

    // Status bar: mouse position (zoom is handled by StatusBarDirector)
    connect(m_pView, &QAtGraphicsView::mousePositionChanged, this,
            [this](const QPointF &pos) { _updatePosLabel(pos); });

    // Tool changed → sync status bar + refresh action states
    connect(m_pView, &QAtGraphicsView::toolChanged, this, [this](Tool tool) {
        m_currentTool = tool;
        _updateToolLabel();
        AppContext::get().refreshAllActions();
    });
}

void MainWindow::_initStatusBar()
{
    auto *bar = statusBar();

    // 鼠标坐标
    m_posLabel = new QLabel(tr("X: 0.0 mm  Y: 0.0 mm"));
    m_posLabel->setMinimumWidth(220);

    m_pProgressMgr = new ProgressManager(this);

    // ---- Zoom controls: − / combo / + / log-slider ----
    m_zoomOutBtn = new QToolButton;
    m_zoomOutBtn->setText(QStringLiteral("−")); // minus sign
    m_zoomOutBtn->setFixedSize(26, 24);
    m_zoomOutBtn->setAutoRaise(true);
    m_zoomOutBtn->setToolTip(tr("Zoom out"));
    connect(m_zoomOutBtn, &QToolButton::clicked, this, [this]() {
        if (m_pView) m_pView->setZoomLevel(m_pView->zoomLevel() * 0.8);
    });

    // Zoom percentage input
    m_zoomEdit = new QLineEdit(QStringLiteral("100"));
    m_zoomEdit->setFixedWidth(44);
    m_zoomEdit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_zoomEdit->setValidator(new QIntValidator(1, 3200, this));
    m_zoomEdit->setToolTip(tr("Enter zoom percentage, press Enter to apply"));
    connect(m_zoomEdit, &QLineEdit::returnPressed,
            m_statusBarDirector, &StatusBarDirector::applyZoomFromEdit);

    // % label
    m_zoomPctLabel = new QLabel(QStringLiteral("%"));

    // Dropdown preset button
    m_zoomPresetMenu = new QMenu(this);
    auto addPreset = [this](const QString &text, int pct) {
        auto *act = m_zoomPresetMenu->addAction(text);
        connect(act, &QAction::triggered, this, [this, pct]() {
            m_statusBarDirector->applyZoomPreset(pct);
        });
    };
    addPreset(QStringLiteral("100%"), 100);
    addPreset(QStringLiteral("200%"), 200);
    addPreset(QStringLiteral("50%"), 50);
    addPreset(QStringLiteral("25%"), 25);
    addPreset(QStringLiteral("400%"), 400);
    addPreset(QStringLiteral("800%"), 800);

    m_zoomPresetBtn = new QToolButton;
    m_zoomPresetBtn->setText(QStringLiteral("▾"));
    m_zoomPresetBtn->setFixedSize(18, 24);
    m_zoomPresetBtn->setAutoRaise(true);
    m_zoomPresetBtn->setToolTip(tr("Zoom presets"));
    m_zoomPresetBtn->setPopupMode(QToolButton::InstantPopup);
    m_zoomPresetBtn->setMenu(m_zoomPresetMenu);
    m_zoomPresetBtn->setStyleSheet("QToolButton::menu-indicator { image: none; }");

    m_zoomInBtn = new QToolButton;
    m_zoomInBtn->setText(QStringLiteral("+"));
    m_zoomInBtn->setFixedSize(26, 24);
    m_zoomInBtn->setAutoRaise(true);
    m_zoomInBtn->setToolTip(tr("Zoom in"));
    connect(m_zoomInBtn, &QToolButton::clicked, this, [this]() {
        if (m_pView) m_pView->setZoomLevel(m_pView->zoomLevel() * 1.25);
    });

    // Log-mapped slider
    m_zoomSlider = new QSlider(Qt::Horizontal);
    m_zoomSlider->setRange(0, 100);
    m_zoomSlider->setValue(50); // ~56.6% default; corrected on first sync
    m_zoomSlider->setFixedWidth(80);
    m_zoomSlider->setToolTip(tr("Adjust zoom level"));
    connect(m_zoomSlider, &QSlider::valueChanged, this, [this](int v) {
        if (m_pView)
            m_pView->setZoomLevel(0.01 * std::pow(3200.0, v / 100.0));
    });

    // 画布尺寸修改按钮
    m_resizeCanvasBtn = new QToolButton;
    m_resizeCanvasBtn->setIcon(QIcon(":/icons/icons/canvas-resize.svg"));
    m_resizeCanvasBtn->setIconSize(QSize(16, 16));
    m_resizeCanvasBtn->setAutoRaise(true);
    m_resizeCanvasBtn->setToolTip(tr("Resize canvas"));
    m_resizeCanvasBtn->setVisible(true);
    connect(m_resizeCanvasBtn, &QToolButton::clicked, this, &MainWindow::onResizeCanvas);

    // 画布尺寸
    m_canvasLabel = new QLabel;
    m_canvasLabel->setMinimumWidth(230);
    if (m_pView)
        _updateCanvasLabel();

    // 当前工具
    m_toolLabel = new QLabel;
    m_toolLabel->setMinimumWidth(120);
    _updateToolLabel();

    bar->addWidget(m_posLabel);
    bar->addPermanentWidget(createStatusSeparator(bar));

    // 任务历史按钮
    m_taskHistoryBtn = new QToolButton;
    m_taskHistoryBtn->setIcon(QIcon(":/icons/icons/task-history.svg"));
    m_taskHistoryBtn->setIconSize(QSize(14, 14));
    m_taskHistoryBtn->setAutoRaise(true);
    m_taskHistoryBtn->setToolTip(tr("Task history"));
    m_taskHistoryBtn->setEnabled(false);
    connect(m_taskHistoryBtn, &QToolButton::clicked, this, &MainWindow::_toggleHistoryPopup);

    // 任务历史弹窗
    m_taskHistoryPopup = new TaskHistoryPopup(this);
    connect(m_taskHistoryPopup, &TaskHistoryPopup::taskClicked, this,
            [this](const QString &taskId) { m_pProgressMgr->setFocusTask(taskId); });
    connect(m_taskHistoryPopup, &TaskHistoryPopup::popupHidden, this,
            [this]() { m_taskHistoryBtn->setChecked(false); });

    // 弹窗数据刷新
    connect(m_pProgressMgr, &ProgressManager::taskHistoryChanged, this, [this]() {
        m_taskHistoryBtn->setEnabled(m_pProgressMgr->hasActiveTasks()
                                     || m_pProgressMgr->hasFinishedTasks());
        if (m_taskHistoryPopup && m_taskHistoryPopup->isVisible())
            m_taskHistoryPopup->refresh(m_pProgressMgr->activeTaskList(),
                                        m_pProgressMgr->finishedTaskList());
    });

    // 按钮 + 标签紧凑布局
    auto *taskContainer = new QWidget;
    auto *taskLayout = new QHBoxLayout(taskContainer);
    taskLayout->setContentsMargins(0, 0, 0, 0);
    taskLayout->setSpacing(2);
    taskLayout->addWidget(m_taskHistoryBtn);
    taskLayout->addWidget(m_pProgressMgr->label());
    taskContainer->setAttribute(Qt::WA_TranslucentBackground);

    bar->addPermanentWidget(taskContainer);
    bar->addPermanentWidget(m_pProgressMgr->bar());
    bar->addPermanentWidget(createStatusSeparator(bar));
    bar->addPermanentWidget(m_zoomOutBtn);
    bar->addPermanentWidget(m_zoomEdit);
    bar->addPermanentWidget(m_zoomPctLabel);
    bar->addPermanentWidget(m_zoomPresetBtn);
    bar->addPermanentWidget(m_zoomInBtn);
    bar->addPermanentWidget(createStatusSeparator(bar));
    bar->addPermanentWidget(m_zoomSlider);
    bar->addPermanentWidget(createStatusSeparator(bar));
    bar->addPermanentWidget(m_resizeCanvasBtn);
    bar->addPermanentWidget(m_canvasLabel);
    bar->addPermanentWidget(m_toolLabel);
}

void MainWindow::_initNetWork()
{
    m_pNetWorkUtils = new NetWorkUtils(this);
    connect(m_pNetWorkUtils, &NetWorkUtils::requestFinished, this, &MainWindow::onRequestFinished);
}

void MainWindow::_initProcess()
{
    m_pProcessGuard = new ProcessGuard(this);
    if (m_pProcessGuard) {
        m_pProcessGuard->addProcess(AppConfig::instance().ripExePath());
    }
}

// ==================== New architecture initialization (P1-P7) ====================

void MainWindow::_initServices()
{
    auto &ctx = AppContext::get();

    // Register all services
    ctx.registerService(new ThemeService(this));
    ctx.registerService(new ClipboardService(this));
    ctx.registerService(new ProgressService(this));
    ctx.registerService(new NetworkService(this));
    ctx.registerService(new ProcessService(this));
    ctx.registerService(new ProjectService(this));
    ctx.registerService(new UndoService(this));

    // Wire callbacks
    ctx.setMaybeSaveProject([this]() { return _maybeSaveProject(); });

    // Route remaining Action tokens that still need MainWindow-level resources
    ctx.setActionCallback([this](const QString &token) {
        static const QMap<QString, void (MainWindow::*)()> map = {
            { QStringLiteral("New"), &MainWindow::onNew },
            { QStringLiteral("OpenProject"), &MainWindow::onOpenProject },
            { QStringLiteral("SaveProject"), &MainWindow::onSaveProject },
            { QStringLiteral("ImportImage"), &MainWindow::onImportImage },
            { QStringLiteral("ExportImage"), &MainWindow::onExportImage },
            { QStringLiteral("AutoLayout"), &MainWindow::onAutoLayout },
        };
        auto it = map.find(token);
        if (it != map.end()) {
            (this->*(it.value()))();
            return true;
        }
        return false;
    });

    // Bind undo service to existing undo stack
    ctx.undo()->bindUndoStack(m_undoStack);
}

void MainWindow::_initPages()
{
    // Canvas pages are now Qtitan DockDocumentPanels — each gets its own
    // QAtCanvasPage widget embedded via setWidget().
    // No canvases created on startup — user creates via File -> New.
    // The DockPanelManager automatically fills the central area with document panels.
}

void MainWindow::_initActions()
{
    auto &ctx = AppContext::get();

    // Register draw tool actions (these have REGISTER_ACTION macros)
    // File actions
    ctx.registerAction(QAtActionBasePtr(new NewAction));
    ctx.registerAction(QAtActionBasePtr(new OpenProjectAction));
    ctx.registerAction(QAtActionBasePtr(new SaveProjectAction));
    ctx.registerAction(QAtActionBasePtr(new ImportImageAction));
    ctx.registerAction(QAtActionBasePtr(new ExportImageAction));
    ctx.registerAction(QAtActionBasePtr(new ExitAction));

    // Edit actions
    ctx.registerAction(QAtActionBasePtr(new UndoAction));
    ctx.registerAction(QAtActionBasePtr(new RedoAction));
    ctx.registerAction(QAtActionBasePtr(new CutAction));
    ctx.registerAction(QAtActionBasePtr(new CopyAction));
    ctx.registerAction(QAtActionBasePtr(new PasteAction));
    ctx.registerAction(QAtActionBasePtr(new DeleteAction));
    ctx.registerAction(QAtActionBasePtr(new SelectAllAction));

    // Draw actions
    ctx.registerAction(QAtActionBasePtr(new SelectToolAction));
    ctx.registerAction(QAtActionBasePtr(new HandToolAction));
    ctx.registerAction(QAtActionBasePtr(new RectToolAction));
    ctx.registerAction(QAtActionBasePtr(new EllipseToolAction));
    ctx.registerAction(QAtActionBasePtr(new LineToolAction));
    ctx.registerAction(QAtActionBasePtr(new BezierCurveToolAction));
    ctx.registerAction(QAtActionBasePtr(new FreehandToolAction));
    ctx.registerAction(QAtActionBasePtr(new TextToolAction));

    // Arrange actions
    ctx.registerAction(QAtActionBasePtr(new BringToFrontAction));
    ctx.registerAction(QAtActionBasePtr(new SendToBackAction));
    ctx.registerAction(QAtActionBasePtr(new GroupAction));
    ctx.registerAction(QAtActionBasePtr(new UngroupAction));
    ctx.registerAction(QAtActionBasePtr(new FitCanvasToItemsAction));
    ctx.registerAction(QAtActionBasePtr(new RotateCWAction));
    ctx.registerAction(QAtActionBasePtr(new RotateCCWAction));
    ctx.registerAction(QAtActionBasePtr(new Rotate180Action));
    ctx.registerAction(QAtActionBasePtr(new AutoLayoutAction));

    // View actions
    ctx.registerAction(QAtActionBasePtr(new ToggleGridAction));
    ctx.registerAction(QAtActionBasePtr(new FitToCanvasAction));
    ctx.registerAction(QAtActionBasePtr(new ResetZoomAction));
    ctx.registerAction(QAtActionBasePtr(new ThemeAction));

    // Dialog actions
    ctx.registerAction(QAtActionBasePtr(new SettingsAction));
    ctx.registerAction(QAtActionBasePtr(new PreferencesAction));
    ctx.registerAction(QAtActionBasePtr(new AboutAction));
    ctx.registerAction(QAtActionBasePtr(new AlignWidgetAction));

    // Initialize undo/redo action state
    ctx.refreshAllActions();
}

void MainWindow::_updatePosLabel(const QPointF &scenePos)
{
    m_lastScenePos = scenePos;
    qreal ppi = m_pView->canvasItem() ? m_pView->canvasItem()->ppi() : 150.0;
    AtMath::Units::DPIContext ctx(ppi);
    qreal xmm = ctx.pxToMm(scenePos.x());
    qreal ymm = ctx.pxToMm(scenePos.y());
    m_posLabel->setText(tr("X: %1 mm  Y: %2 mm").arg(xmm, 0, 'f', 1).arg(ymm, 0, 'f', 1));
}

void MainWindow::_updateCanvasLabel()
{
    if (!m_canvasLabel)
        return;

    auto *canvas = m_pView->canvasItem();
    if (!canvas) {
        m_canvasLabel->clear();
        return;
    }

    QSizeF sz = canvas->canvasSize();
    qreal ppi = canvas->ppi();
    AtMath::Units::DPIContext ctx(ppi);

    m_canvasLabel->setText(tr("Canvas: %1 \u00d7 %2 mm")
                               .arg(ctx.pxToMm(sz.width()), 0, 'f', 1)
                               .arg(ctx.pxToMm(sz.height()), 0, 'f', 1));
}

// ============================================================
// Qtitan Dock Canvas management (replaces old QDockWidget API)
// ============================================================

QAtCanvasPage *MainWindow::_currentCanvasPage() const
{
    if (!m_activeDocumentPanel)
        return nullptr;
    return qobject_cast<QAtCanvasPage *>(m_activeDocumentPanel->widget());
}

int MainWindow::_canvasCount() const
{
    return dockPanelManager()->documentPanelList().size();
}

void MainWindow::_addCanvasPage(const QString &title, const QString &pageId)
{
    static constexpr qreal kDefaultPpi = 150.0;
    qreal mmToPx = AtMath::Units::DPIContext(kDefaultPpi).mmToPx(1.0);

    NewFileDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    QSizeF sizeMM = dlg.selectedSizeMM();
    QSizeF canvasSize(std::ceil(sizeMM.width() * mmToPx), std::ceil(sizeMM.height() * mmToPx));

    auto *canvasPage = new QAtCanvasPage(pageId, canvasSize, kDefaultPpi, this);
    AppContext::get().registerPage(canvasPage);

    auto *docPanel = dockPanelManager()->addDocumentPanel(title);
    docPanel->setWidget(canvasPage);
    docPanel->setProperty("pageId", pageId);

    // Set as active
    m_activeDocumentPanel = docPanel;
    m_tabStates[canvasPage].modified = false;

    AppContext::get().setActivePage(pageId);
    QAtGraphicsView *newView = canvasPage->view();
    QUndoStack *newStack = canvasPage->undoStack();
    m_pView = newView;
    m_undoStack = newStack;
    if (m_undoStack)
        AppContext::get().undo()->bindUndoStack(m_undoStack);
    _bindViewConnections();

    m_pPropertyPanel->setItem(nullptr);
    m_resizeCanvasBtn->setVisible(true);
    m_pPropertyPanel->setDisplayPpi(kDefaultPpi);

    m_pProgressMgr->resetAll();

    // Sync labels
    _updateCanvasLabel();
    _updatePosLabel(m_lastScenePos);
    AppContext::get().refreshAllActions();
    m_statusBarDirector->onPageSwitched(pageId, QStringLiteral("canvas"));

    // Enable side panels now that we have a canvas
    if (m_propsDockPanel)
        m_propsDockPanel->showPanel();
    if (m_alignDockPanel)
        m_alignDockPanel->closePanel();

    m_projectModified = true;
}

void MainWindow::_removeCanvasPage(Qtitan::DockDocumentPanel *docPanel)
{
    if (!docPanel)
        return;
    QAtCanvasPage *page = qobject_cast<QAtCanvasPage *>(docPanel->widget());
    if (page) {
        page->disconnect(this);
        if (m_statusBarDirector)
            page->disconnect(m_statusBarDirector);
        AppContext::get().unregisterPage(page->pageId());
        m_tabStates.remove(page);
    }

    auto docList = dockPanelManager()->documentPanelList();
    if (m_activeDocumentPanel == docPanel) {
        m_activeDocumentPanel = nullptr;
    }

    // Last canvas removed — reset all state
    if (docList.size() <= 1) {
        m_pView = nullptr;
        m_undoStack = nullptr;
        m_projectPath.clear();
        m_projectModified = false;
        if (m_propsDockPanel)
            m_propsDockPanel->closePanel();
        if (m_alignDockPanel)
            m_alignDockPanel->closePanel();
    }

    dockPanelManager()->removeDockPanel(docPanel);
}

void MainWindow::_onDocumentPanelActivated(Qtitan::DockDocumentPanel *panel)
{
    if (!panel || m_activeDocumentPanel == panel)
        return;

    QAtCanvasPage *page = qobject_cast<QAtCanvasPage *>(panel->widget());
    if (!page)
        return;

    _updateDocumentPanelTitle(page);

    Qtitan::DockDocumentPanel *oldPanel = m_activeDocumentPanel;
    m_activeDocumentPanel = panel;

    AppContext::get().setActivePage(page->pageId());
    QAtGraphicsView *newView = page->view();
    QUndoStack *newStack = page->undoStack();

    if (m_pView != newView) {
        if (m_pView) {
            m_pView->disconnect(this);
            if (m_undoStack)
                m_undoStack->disconnect(this);
        }
        m_pView = newView;
        m_undoStack = newStack;
        if (m_undoStack)
            AppContext::get().undo()->bindUndoStack(m_undoStack);
        _bindViewConnections();

        if (m_alignLayoutDlg && newView) {
            m_alignLayoutDlg->setScene(newView->scene());
            m_alignLayoutDlg->setUndoStack(newStack);
        }

        if (m_pView && m_pView->currentTool() != m_currentTool)
            m_pView->setTool(m_currentTool);

        _updateCanvasLabel();
        _updatePosLabel(m_lastScenePos);

        AppContext::get().refreshAllActions();

        if (m_pView && m_pView->canvasItem()) {
            qreal ppi = m_pView->canvasItem()->ppi();
            if (m_pPropertyPanel)
                m_pPropertyPanel->setDisplayPpi(ppi);
        }
    }

    if (oldPanel) {
        auto *oldPage = qobject_cast<QAtCanvasPage *>(oldPanel->widget());
        if (oldPage)
            _updateDocumentPanelTitle(oldPage);
    }

    if (m_pView && m_pPropertyPanel) {
        auto items = ::filterSelectableItems(m_pView->scene()->selectedItems());
        m_pPropertyPanel->setItem(items.size() == 1 ? items.first() : nullptr);
    }
}

void MainWindow::_updateDocumentPanelTitle(QAtCanvasPage *page)
{
    if (!page)
        return;
    QString title;
    if (!m_projectPath.isEmpty()) {
        if (_canvasCount() == 1)
            title = QFileInfo(m_projectPath).completeBaseName();
        else {
            auto docList = dockPanelManager()->documentPanelList();
            for (int i = 0; i < docList.size(); ++i) {
                if (qobject_cast<QAtCanvasPage *>(docList[i]->widget()) == page) {
                    title = tr("%1 — Canvas %2")
                                .arg(QFileInfo(m_projectPath).completeBaseName())
                                .arg(i + 1);
                    break;
                }
            }
        }
    } else {
        title = page->title();
    }
    auto &state = m_tabStates[page];
    if (state.modified && !title.endsWith(QStringLiteral(" *")))
        title += QStringLiteral(" *");
    auto docList = dockPanelManager()->documentPanelList();
    for (auto *dp : docList) {
        if (qobject_cast<QAtCanvasPage *>(dp->widget()) == page) {
            dp->setCaption(title);
            break;
        }
    }
}

void MainWindow::_updateToolLabel()
{
    if (!m_toolLabel)
        return;

    QString toolName;
    switch (m_currentTool) {
    case Tool::Select:
        toolName = tr("Select");
        break;
    case Tool::Hand:
        toolName = tr("Hand");
        break;
    case Tool::Rect:
        toolName = tr("Rectangle");
        break;
    case Tool::Ellipse:
        toolName = tr("Ellipse");
        break;
    case Tool::Line:
        toolName = tr("Line");
        break;
    case Tool::BezierCurve:
        toolName = tr("B\u00e9zier Curve");
        break;
    case Tool::Freehand:
        toolName = tr("Freehand");
        break;
    case Tool::Text:
        toolName = tr("Text");
        break;
    }

    if (toolName.isEmpty())
        toolName = tr("Unknown");

    m_toolLabel->setText(tr("Tool: %1").arg(toolName));
}

void MainWindow::_syncViewState()
{
    if (!m_pView)
        return;
    if (auto *c = m_pView->canvasItem()) {
        qreal ppi = c->ppi();
        auto *page = qobject_cast<QAtCanvasPage *>(_currentCanvasPage());
        if (page)
            page->setRulerPpi(ppi);
        if (m_pPropertyPanel)
            m_pPropertyPanel->setDisplayPpi(ppi);
    }
    _updateCanvasLabel();
    _updatePosLabel(m_lastScenePos);
}

// ---- 公共：收集所有画布的序列化数据（含 CMYK） ----
// 委托给 ProjectDocument::collectBundles()，消除重复序列化逻辑
QList<CanvasSaveBundle> MainWindow::_collectCanvasBundles() const
{
    if (_canvasCount() == 0)
        return { };

    // 确保 ProjectDocument 已注册所有当前画布（文档滞后于 TabWidget）
    if (m_document) {
        m_document->setFilePath(m_projectPath);
        for (int i = 0; i < _canvasCount(); ++i) {
            auto *page = qobject_cast<QAtCanvasPage *>(_canvasPageAt(i));
            if (page) {
                if (!m_document->canvases().contains(page))
                    m_document->registerCanvas(page);
                m_document->markCanvasModified(page, m_tabStates.value(page).modified);
            }
        }
        return m_document->collectBundles();
    }

    // 回退：m_document 尚未初始化（不应发生，但安全兜底）
    QList<CanvasSaveBundle> bundles;
    for (int i = 0; i < _canvasCount(); ++i) {
        auto *page = qobject_cast<QAtCanvasPage *>(_canvasPageAt(i));
        if (!page || !page->canvasItem())
            continue;

        CanvasSaveBundle bundle;
        bundle.info.width = page->canvasItem()->canvasSize().width();
        bundle.info.height = page->canvasItem()->canvasSize().height();
        bundle.info.dpi =
            page->canvasItem()->isDpiLocked() ? page->canvasItem()->canvasDpiX() : 0.0;

        auto items = ::filterSelectableItems(page->scene()->items());
        for (auto *item : items) {
            SerializeInput input;
            auto *igi = dynamic_cast<IGraphicsItem *>(item);
            input.itemType = igi ? static_cast<int>(igi->itemType()) : 0;
            input.zValue = item->zValue();
            input.posX = item->pos().x();
            input.posY = item->pos().y();
            input.rotation = item->rotation();
            if (igi) {
                QByteArray binary;
                QDataStream out(&binary, QIODevice::WriteOnly);
                out << static_cast<int>(igi->itemType());
                igi->serialize(out);
                input.binary = binary;

                if (igi->hasPenCmyk()) {
                    input.cmyk.hasPen = true;
                    igi->penCmyk(input.cmyk.penC, input.cmyk.penM, input.cmyk.penY,
                                 input.cmyk.penK);
                }
                if (igi->hasBrushCmyk()) {
                    input.cmyk.hasBrush = true;
                    igi->brushCmyk(input.cmyk.brushC, input.cmyk.brushM, input.cmyk.brushY,
                                   input.cmyk.brushK);
                }
                input.cmyk.gradient = igi->gradientStopCmykMap();
            }
            bundle.items.append(serializeItemWorker(input));
        }
        bundles.append(bundle);
    }
    return bundles;
}

void MainWindow::saveSession()
{
    if (_canvasCount() == 0)
        return;

    // Sync legacy fields to ProjectDocument
    if (m_document) {
        m_document->setFilePath(m_projectPath);
        for (int i = 0; i < _canvasCount(); ++i) {
            auto *page = qobject_cast<QAtCanvasPage *>(_canvasPageAt(i));
            if (page) {
                if (!m_document->canvases().contains(page))
                    m_document->registerCanvas(page);
                m_document->markCanvasModified(page, m_tabStates.value(page).modified);
            }
        }
    }

    SessionInfo info;
    info.version = 2;
    info.projectPath = m_projectPath;
    info.activeTabIndex = qMax(0, _currentCanvasIndex());

    info.currentTool = m_currentTool;
    info.ripEnabled = m_ripEnabled;
    info.ripXRes = m_ripXRes;
    info.ripYRes = m_ripYRes;
    info.alignHSpacing = AlignWidget::hSpacing();
    info.alignVSpacing = AlignWidget::vSpacing();

    for (int i = 0; i < _canvasCount(); ++i) {
        auto *page = qobject_cast<QAtCanvasPage *>(_canvasPageAt(i));
        if (!page)
            continue;

        SessionTabInfo tab;
        const auto &state = m_tabStates[page];
        tab.modified = state.modified;

        if (auto *canvas = page->canvasItem()) {
            tab.canvasWidthPx = canvas->canvasSize().width();
            tab.canvasHeightPx = canvas->canvasSize().height();
            tab.ppi = canvas->ppi();
        }
        if (auto *view = page->view()) {
            tab.zoomLevel = view->zoomLevel();
            tab.gridVisible = view->isGridVisible();
        }
        info.tabs.append(tab);
    }

    QString error;
    if (!SessionFile::save(info, &error)) {
        qWarning() << "[Session] Failed to save session:" << error;
    }
}

void MainWindow::loadSession()
{
    bool ok = false;
    QString error;
    SessionInfo info = SessionFile::load(&ok, &error);

    if (!ok) {
        qWarning() << "[Session] Failed to load session:" << error;
        SessionFile::remove();
        return;
    }

    if (info.tabs.isEmpty())
        return;

    // --- Restore project-level path ---
    m_projectPath = info.projectPath;
    m_projectModified = false;

    // --- Check if project path is autosave ---
    bool isAutoSave = m_projectPath.contains(QStringLiteral("/autosave/"));

    // --- Rebuild tabs ---
    for (int i = 0; i < info.tabs.size(); ++i) {
        const SessionTabInfo &tab = info.tabs[i];
        QString pageId = QStringLiteral("canvas-%1").arg(i + 1);

        QSizeF canvasSize(tab.canvasWidthPx, tab.canvasHeightPx);
        if (canvasSize.width() <= 0 || canvasSize.height() <= 0) {
            constexpr qreal kDefaultPpi = 150.0;
            const qreal mmToPx = AtMath::Units::DPIContext(kDefaultPpi).mmToPx(1.0);
            canvasSize = QSizeF(210.0 * mmToPx, 297.0 * mmToPx);
        }

        auto *canvasPage = new QAtCanvasPage(pageId, canvasSize, tab.ppi, this);
        AppContext::get().registerPage(canvasPage);
        m_tabStates[canvasPage].modified = tab.modified;

        // Apply view-level state
        if (auto *view = canvasPage->view()) {
            view->setGridVisible(tab.gridVisible);
            QTimer::singleShot(0, view,
                               [view, zoom = tab.zoomLevel]() { view->setZoomLevel(zoom); });
        }

        _addCanvasDockInternal(canvasPage, canvasPage->title());
    }

    // --- Set active tab ---
    int activeIdx = AtMath::clamp(info.activeTabIndex, 0, _canvasCount() - 1);
    _setCurrentCanvasIndex(activeIdx);

    // --- Restore global tool ---
    m_currentTool = info.currentTool;
    if (m_pView)
        m_pView->setTool(m_currentTool);
    _updateToolLabel();
    AppContext::get().refreshAllActions();

    // --- Restore RIP / alignment ---
    m_ripEnabled = info.ripEnabled;
    m_ripXRes = info.ripXRes;
    m_ripYRes = info.ripYRes;
    AlignWidget::setHSpacing(info.alignHSpacing);
    AlignWidget::setVSpacing(info.alignVSpacing);

    // --- Async load project file (single file for all canvases) ---
    if (m_projectPath.isEmpty()) {
        // Update tab titles
        for (int i = 0; i < _canvasCount(); ++i) {
            auto *page = qobject_cast<QAtCanvasPage *>(_canvasPageAt(i));
            if (page)
                _updateCanvasDockTitle(page);
        }
        SessionFile::remove();
        return;
    }

    if (!QFile::exists(m_projectPath)) {
        qWarning() << "[Session] Project file not found:" << m_projectPath;
        m_projectPath.clear();
        m_projectModified = true;
        SessionFile::remove();
        return;
    }

    // Parse project file → gets all canvases
    ProjectFile pf;
    ProjectFile::ProjectInfo projInfo;
    QList<CanvasDeserialBundle> canvasBundles;

    if (!pf.parseMulti(m_projectPath, projInfo, canvasBundles)) {
        qWarning() << "[Session] Failed to parse project:" << m_projectPath << pf.lastError();
        m_projectPath = isAutoSave ? QString() : m_projectPath;
        m_projectModified = isAutoSave;
        SessionFile::remove();
        return;
    }

    // Match loaded canvases to existing tabs (reuse tabs, update sizes)
    for (int ci = 0; ci < canvasBundles.size() && ci < _canvasCount(); ++ci) {
        auto &bundle = canvasBundles[ci];
        auto *page = qobject_cast<QAtCanvasPage *>(_canvasPageAt(ci));
        if (!page)
            continue;

        qreal ppi = bundle.info.dpi > 0 ? bundle.info.dpi : 150.0;
        page->setCanvasSize(QSizeF(bundle.info.width, bundle.info.height));
        if (auto *c = page->canvasItem()) {
            c->setPpi(ppi);
            if (bundle.info.dpi > 0)
                c->setCanvasDpi(bundle.info.dpi, bundle.info.dpi);
        }

        if (bundle.tasks.isEmpty()) {
            m_tabStates[page].modified = isAutoSave;
            _updateCanvasDockTitle(page);
            continue;
        }

        const QString taskId =
            m_pProgressMgr->startTask(tr("Restore canvas %1").arg(ci + 1), bundle.tasks.size());
        page->view()->setEnabled(false);

        auto *watcher = new QFutureWatcher<DeserializedItem>(this);
        QPointer<QAtCanvasPage> pagePtr(page);

        connect(watcher, &QFutureWatcher<DeserializedItem>::progressValueChanged, this,
                [this, taskId](int v) { m_pProgressMgr->updateTask(taskId, v); });

        connect(watcher, &QFutureWatcher<DeserializedItem>::finished, this,
                [this, watcher, taskId, ppi, pagePtr, isAutoSave]() {
                    m_pProgressMgr->finishTask(taskId);
                    if (!pagePtr) {
                        watcher->deleteLater();
                        return;
                    }

                    QList<QGraphicsItem *> loadedItems;
                    auto f = watcher->future();
                    for (int j = 0; j < f.resultCount(); ++j) {
                        auto *item = createItemFromDeserialized(f.resultAt(j));
                        if (item)
                            loadedItems.append(item);
                    }
                    for (auto *item : loadedItems)
                        pagePtr->scene()->addItem(item);

                    pagePtr->view()->setEnabled(true);
                    // Clear autosave path
                    m_tabStates[pagePtr].modified = isAutoSave;
                    _updateCanvasDockTitle(pagePtr);

                    if (_currentCanvasPage() == pagePtr) {
                        pagePtr->setRulerPpi(ppi);
                        if (m_pPropertyPanel)
                            m_pPropertyPanel->setDisplayPpi(ppi);
                        pagePtr->updateRulers();
                        _updateCanvasLabel();
                        _updatePosLabel(m_lastScenePos);
                    }
                    refreshImageItemsFromCache(loadedItems);
                    watcher->deleteLater();
                });

        watcher->setFuture(QtConcurrent::mapped(bundle.tasks, deserializeItemWorker));
    }

    // Clear autosave path — user should pick a real save location on next save
    if (isAutoSave)
        m_projectPath.clear();
    if (!m_projectPath.isEmpty())
        setWindowTitle(
            tr("AT Drawing Tools - %1").arg(QFileInfo(m_projectPath).completeBaseName()));
    SessionFile::remove();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    // 更新刻度尺 (rulers are per-page)
    auto *page = qobject_cast<QAtCanvasPage *>(_currentCanvasPage());
    if (page)
        page->updateRulers();
}

void MainWindow::setMainWindowVisibility(bool state)
{
    if (state) {
        show();
        raise();
        activateWindow();
    } else {
        hide();
    }
}

void MainWindow::getToolInfo()
{
    // 获取Rip工具版本
    if (m_pNetWorkUtils && m_pProcessGuard->isProcessRunning(AppConfig::instance().ripExePath())) {
        // 同步请求获取 Rip 版本（阻塞主线程，通常很快）
        m_pNetWorkUtils->doRipVersion();
    }

    // 获取其他工具版本，可用性
}
// ============================================================
// 文件操作
// ============================================================
bool MainWindow::_maybeSaveProject()
{
    if (_canvasCount() == 0)
        return true;

    // 检查是否有任何画布被修改
    bool anyModified = false;
    for (int i = 0; i < _canvasCount() && !anyModified; ++i) {
        auto *page = qobject_cast<QAtCanvasPage *>(_canvasPageAt(i));
        if (page && m_tabStates[page].modified)
            anyModified = true;
    }
    if (!anyModified)
        return true;

    QString title = m_projectPath.isEmpty() ? tr("Untitled project")
                                            : QFileInfo(m_projectPath).completeBaseName();

    QMessageBox::StandardButton btn =
        QMessageBox::question(this, tr("Unsaved Changes"),
                              tr("The project \"%1\" has unsaved changes.\n"
                                 "Do you want to save them?")
                                  .arg(title),
                              QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (btn == QMessageBox::Cancel)
        return false;
    if (btn == QMessageBox::No) {
        // Discard all changes
        for (int i = 0; i < _canvasCount(); ++i) {
            auto *page = qobject_cast<QAtCanvasPage *>(_canvasPageAt(i));
            if (page) {
                m_tabStates[page].modified = false;
                _updateCanvasDockTitle(page);
            }
        }
        m_projectModified = false;
        return true;
    }
    // Yes — save
    if (m_projectPath.isEmpty()) {
        // No saved path — prompt for location
        QString savePath = QFileDialog::getSaveFileName(
            this, tr("Save Project"), QString(), tr("AT Project Files (*.atp);;All Files (*)"));
        if (savePath.isEmpty())
            return false;
        if (!savePath.endsWith(QLatin1String(".atp"), Qt::CaseInsensitive))
            savePath.append(QLatin1String(".atp"));
        m_projectPath = savePath;
    }
    // Synchronous save of all canvases
    return _syncSaveAllCanvases();
}

// ============================================================
// 关闭画布标签页警告
// ============================================================
bool MainWindow::_maybeCloseCanvas(QAtCanvasPage *page)
{
    if (!page || !page->scene())
        return true;

    // Check if canvas has any items (primitives or images)
    auto items = ::filterSelectableItems(page->scene()->items());
    if (items.isEmpty())
        return true;

    QMessageBox::StandardButton btn =
        QMessageBox::warning(this, tr("Close Canvas"),
                             tr("Closing this canvas will permanently lose all drawn\n"
                                "primitives and image items on this canvas.\n\n"
                                "They will not be recoverable.\n\n"
                                "Continue?"),
                             QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    return btn == QMessageBox::Yes;
}

bool MainWindow::_syncSaveAllCanvases()
{
    if (_canvasCount() == 0 || m_projectPath.isEmpty())
        return false;

    ProjectFile::ProjectInfo projInfo;
    QFileInfo fi(m_projectPath);
    projInfo.name = fi.completeBaseName();
    projInfo.version = QStringLiteral("2.0.0");
    projInfo.author = QStringLiteral("ATHC");

    auto bundles = _collectCanvasBundles();

    // 原子保存: 先写临时文件，成功后再轮转备份 + 替换
    {
        const QString tmpPath = m_projectPath + QStringLiteral(".tmp");
        ProjectFile pf;
        if (!pf.saveMulti(tmpPath, projInfo, bundles)) {
            QMessageBox::warning(this, tr("Save Project"),
                                 tr("Failed to save:\n%1").arg(pf.lastError()));
            QFile::remove(tmpPath);
            return false;
        }
        // 只有写入临时文件成功后才轮转备份
        ProjectDocument::rotateBackups(m_projectPath, 3);
        QFile::remove(m_projectPath);
        if (!QFile::rename(tmpPath, m_projectPath)) {
            QMessageBox::warning(this, tr("Save Project"), tr("Failed to finalize save"));
            QFile::remove(tmpPath);
            return false;
        }
    }

    m_projectModified = false;
    for (int i = 0; i < _canvasCount(); ++i) {
        auto *page = qobject_cast<QAtCanvasPage *>(_canvasPageAt(i));
        if (page) {
            m_tabStates[page].modified = false;
            _updateCanvasDockTitle(page);
        }
    }
    setWindowTitle(tr("AT Drawing Tools - %1").arg(projInfo.name));
    return true;
}

void MainWindow::onNew()
{
    // Toolbar: 在当前工程中新建画布（不提示保存）
    _addCanvasDock(tr("Canvas %1").arg(_canvasCount() + 1),
                   QStringLiteral("canvas-%1").arg(_canvasCount() + 1));
}

void MainWindow::onNewProject()
{
    // Menubar: 新建工程文件 — 提示用户输入工程文件名称和位置
    if (!_maybeSaveProject())
        return;

    QString path = QFileDialog::getSaveFileName(this, tr("New Project"), QString(),
                                                tr("AT Project Files (*.atp);;All Files (*)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(QLatin1String(".atp"), Qt::CaseInsensitive))
        path.append(QLatin1String(".atp"));

    // 关闭所有现有画布标签
    if (_canvasCount() > 0) {
        while (_canvasCount() > 0) {
            auto *page = qobject_cast<QAtCanvasPage *>(_canvasPageAt(0));
            if (page) {
                page->disconnect(this);
                if (m_statusBarDirector)
                    page->disconnect(m_statusBarDirector);
                AppContext::get().unregisterPage(page->pageId());
                m_tabStates.remove(page);
            }
            _removeCanvasDockInternal(0);
        }
        m_pView = nullptr;
        m_undoStack = nullptr;
    }

    m_projectPath = path;
    m_projectModified = false;

    QFileInfo fi(path);
    setWindowTitle(tr("AT Drawing Tools - %1").arg(fi.completeBaseName()));

    // 创建第一个画布
    _addCanvasDock(fi.completeBaseName(), QStringLiteral("canvas-1"));
}

// 内部方法：创建画布标签页（共享逻辑）
void MainWindow::_addCanvasDock(const QString &title, const QString &pageId)
{
    static constexpr qreal kDefaultPpi = 150.0;
    qreal mmToPx = AtMath::Units::DPIContext(kDefaultPpi).mmToPx(1.0);

    NewFileDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    QSizeF sizeMM = dlg.selectedSizeMM();
    QSizeF canvasSize(std::ceil(sizeMM.width() * mmToPx), std::ceil(sizeMM.height() * mmToPx));

    auto *canvasPage = new QAtCanvasPage(pageId, canvasSize, kDefaultPpi, this);
    AppContext::get().registerPage(canvasPage);
    _addCanvasDockInternal(canvasPage, title);
    _setCurrentCanvasPage(canvasPage);

    _activeTabState().modified = false;
    _updateCanvasDockTitle(canvasPage);

    m_pPropertyPanel->setItem(nullptr);
    m_resizeCanvasBtn->setVisible(true);
    m_pPropertyPanel->setDisplayPpi(kDefaultPpi);

    m_pProgressMgr->resetAll();

    canvasPage->updateRulers();
    _updateCanvasLabel();
    _updatePosLabel(m_lastScenePos);
}

void MainWindow::onOpenProject()
{
    if (!_maybeSaveProject())
        return;

    QString path = QFileDialog::getOpenFileName(this, tr("Open Project"), QString(),
                                                tr("AT Project Files (*.atp);;All Files (*)"));
    if (path.isEmpty())
        return;

    // ---- 阶段 1：主线程解析 XML（快速）— 支持多画布 ----
    ProjectFile pf;
    ProjectFile::ProjectInfo projInfo;
    QList<CanvasDeserialBundle> canvasBundles;

    if (!pf.parseMulti(path, projInfo, canvasBundles)) {
        QMessageBox::warning(this, tr("Open Project"),
                             tr("Failed to open project:\n%1").arg(pf.lastError()));
        return;
    }

    if (canvasBundles.isEmpty()) {
        QMessageBox::warning(this, tr("Open Project"), tr("Project contains no canvases."));
        return;
    }

    // 关闭所有现有画布标签
    if (_canvasCount() > 0) {
        while (_canvasCount() > 0) {
            auto *oldPage = qobject_cast<QAtCanvasPage *>(_canvasPageAt(0));
            if (oldPage) {
                oldPage->disconnect(this);
                if (m_statusBarDirector)
                    oldPage->disconnect(m_statusBarDirector);
                AppContext::get().unregisterPage(oldPage->pageId());
                m_tabStates.remove(oldPage);
            }
            _removeCanvasDockInternal(0);
        }
        m_pView = nullptr;
        m_undoStack = nullptr;
    }

    m_projectPath = path;
    m_projectModified = false;

    // ---- 为每个画布创建标签页 ----
    for (int ci = 0; ci < canvasBundles.size(); ++ci) {
        auto &bundle = canvasBundles[ci];
        qreal ppi = bundle.info.dpi > 0 ? bundle.info.dpi : 150.0;

        QString pageId = QStringLiteral("proj-%1-canvas-%2").arg(ci).arg(ci);
        auto *canvasPage = new QAtCanvasPage(pageId, this);
        canvasPage->setCanvasSize(QSizeF(bundle.info.width, bundle.info.height));
        if (auto *c = canvasPage->canvasItem()) {
            c->setPpi(ppi);
            if (bundle.info.dpi > 0)
                c->setCanvasDpi(bundle.info.dpi, bundle.info.dpi);
        }
        AppContext::get().registerPage(canvasPage);
        m_tabStates[canvasPage].modified = false;

        QString tabTitle = canvasBundles.size() == 1
                               ? projInfo.name
                               : tr("%1 — Canvas %2").arg(projInfo.name).arg(ci + 1);
        _addCanvasDockInternal(canvasPage, tabTitle);

        if (ci == 0)
            _setCurrentCanvasPage(canvasPage);

        if (bundle.tasks.isEmpty()) {
            // Empty canvas — already initialized
            canvasPage->view()->setEnabled(true);
            canvasPage->setRulerPpi(ppi);
            canvasPage->updateRulers();
            if (bundle.info.zoom > 0)
                canvasPage->view()->setZoomLevel(bundle.info.zoom);
            _updateCanvasDockTitle(canvasPage);
            continue;
        }

        // ---- 并发 Base64 解码 ----
        const QString taskId =
            m_pProgressMgr->startTask(tr("Load %1").arg(tabTitle), bundle.tasks.size());
        canvasPage->view()->setEnabled(false);

        qreal zoom = bundle.info.zoom > 0 ? bundle.info.zoom : 1.0;

        auto *watcher = new QFutureWatcher<DeserializedItem>(this);
        QPointer<QAtCanvasPage> pagePtr(canvasPage);

        connect(watcher, &QFutureWatcher<DeserializedItem>::progressValueChanged, this,
                [this, taskId](int v) { m_pProgressMgr->updateTask(taskId, v); });

        connect(watcher, &QFutureWatcher<DeserializedItem>::finished, this,
                [this, watcher, taskId, ppi, zoom, pagePtr]() {
                    m_pProgressMgr->finishTask(taskId);
                    if (!pagePtr) {
                        watcher->deleteLater();
                        return;
                    }

                    QList<QGraphicsItem *> loadedItems;
                    auto f = watcher->future();
                    for (int j = 0; j < f.resultCount(); ++j) {
                        auto *item = createItemFromDeserialized(f.resultAt(j));
                        if (item)
                            loadedItems.append(item);
                    }
                    for (auto *item : loadedItems)
                        pagePtr->scene()->addItem(item);

                    pagePtr->view()->setEnabled(true);
                    pagePtr->view()->setZoomLevel(zoom);
                    m_tabStates[pagePtr].modified = false;
                    _updateCanvasDockTitle(pagePtr);

                    if (_currentCanvasPage() == pagePtr) {
                        pagePtr->setRulerPpi(ppi);
                        if (m_pPropertyPanel)
                            m_pPropertyPanel->setDisplayPpi(ppi);
                        pagePtr->updateRulers();
                        _updateCanvasLabel();
                        _updatePosLabel(m_lastScenePos);
                    }
                    refreshImageItemsFromCache(loadedItems);
                    watcher->deleteLater();
                });

        watcher->setFuture(QtConcurrent::mapped(bundle.tasks, deserializeItemWorker));
    }

    m_pPropertyPanel->setDisplayPpi(
        canvasBundles.first().info.dpi > 0 ? canvasBundles.first().info.dpi : 150.0);
    m_pProgressMgr->resetAll();
    setWindowTitle(tr("AT Drawing Tools - %1").arg(projInfo.name));
}

void MainWindow::onSaveProject()
{
    if (_canvasCount() == 0)
        return;

    QString path = m_projectPath;
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, tr("Save Project"), QString(),
                                            tr("AT Project Files (*.atp);;All Files (*)"));
        if (path.isEmpty())
            return;
        if (!path.endsWith(QLatin1String(".atp"), Qt::CaseInsensitive))
            path.append(QLatin1String(".atp"));
    }

    ProjectFile::ProjectInfo projInfo;
    QFileInfo fi(path);
    projInfo.name = fi.completeBaseName();
    projInfo.version = QStringLiteral("2.0.0");
    projInfo.author = QStringLiteral("ATHC");

    // ---- 采集所有画布的快照（主线程，线程安全） ----
    struct CanvasSnapshot
    {
        QPointer<QAtCanvasPage> page;
        CanvasInfo info;
        QList<SerializeInput> inputs;
    };
    QList<CanvasSnapshot> snapshots;

    int totalItems = 0;
    for (int i = 0; i < _canvasCount(); ++i) {
        auto *page = qobject_cast<QAtCanvasPage *>(_canvasPageAt(i));
        if (!page)
            continue;
        auto *canvas = page->canvasItem();
        if (!canvas)
            continue;

        CanvasSnapshot snap;
        snap.page = page;
        snap.info.width = canvas->canvasSize().width();
        snap.info.height = canvas->canvasSize().height();
        snap.info.dpi = canvas->isDpiLocked() ? canvas->canvasDpiX() : 0.0;
        snap.info.zoom = page->view() ? page->view()->zoomLevel() : 1.0;

        auto items = ::filterSelectableItems(page->scene()->items());
        snap.inputs.reserve(items.size());
        for (auto *item : items) {
            SerializeInput input;
            auto *igi = dynamic_cast<IGraphicsItem *>(item);
            input.itemType = igi ? static_cast<int>(igi->itemType()) : 0;
            input.zValue = item->zValue();
            input.posX = item->pos().x();
            input.posY = item->pos().y();
            input.rotation = item->rotation();
            if (igi) {
                QByteArray binary;
                QDataStream out(&binary, QIODevice::WriteOnly);
                out << static_cast<int>(igi->itemType());
                igi->serialize(out);
                input.binary = binary;
                if (igi->hasPenCmyk()) {
                    input.cmyk.hasPen = true;
                    igi->penCmyk(input.cmyk.penC, input.cmyk.penM, input.cmyk.penY,
                                 input.cmyk.penK);
                }
                if (igi->hasBrushCmyk()) {
                    input.cmyk.hasBrush = true;
                    igi->brushCmyk(input.cmyk.brushC, input.cmyk.brushM, input.cmyk.brushY,
                                   input.cmyk.brushK);
                }
                input.cmyk.gradient = igi->gradientStopCmykMap();
            }
            snap.inputs.append(input);
        }
        totalItems += snap.inputs.size();
        snapshots.append(snap);
    }

    if (totalItems == 0 && m_projectPath.isEmpty()) {
        QMessageBox::information(this, tr("Save Project"),
                                 tr("All canvases are empty. Nothing to save."));
        return;
    }

    // ---- 并发序列化所有画布的图元 ----
    const QString taskId = m_pProgressMgr->startTask(tr("Save Project"), totalItems);

    // Disable all views during save
    for (auto &snap : snapshots) {
        if (snap.page)
            snap.page->view()->setEnabled(false);
    }

    auto *watcher = new QFutureWatcher<SerializedItem>(this);
    int snapCount = snapshots.size();

    connect(watcher, &QFutureWatcher<SerializedItem>::progressValueChanged, this,
            [this, taskId](int v) { m_pProgressMgr->updateTask(taskId, v); });

    connect(watcher, &QFutureWatcher<SerializedItem>::finished, this,
            [this, watcher, taskId, path, projInfo, snapshots, snapCount]() {
                m_pProgressMgr->finishTask(taskId);

                // Collect results per canvas
                auto future = watcher->future();
                QList<CanvasSaveBundle> bundles;
                int offset = 0;
                for (int i = 0; i < snapCount; ++i) {
                    CanvasSaveBundle bundle;
                    bundle.info = snapshots[i].info;
                    for (int j = 0; j < snapshots[i].inputs.size(); ++j) {
                        bundle.items.append(future.resultAt(offset + j));
                    }
                    bundles.append(bundle);
                    offset += snapshots[i].inputs.size();
                }

                ProjectFile pf;
                if (!pf.saveMulti(path, projInfo, bundles)) {
                    QMessageBox::warning(this, tr("Save Project"),
                                         tr("Failed to save:\n%1").arg(pf.lastError()));
                    for (auto &snap : snapshots) {
                        if (snap.page)
                            snap.page->view()->setEnabled(true);
                    }
                    watcher->deleteLater();
                    return;
                }

                m_projectPath = path;
                m_projectModified = false;
                for (auto &snap : snapshots) {
                    if (snap.page) {
                        m_tabStates[snap.page].modified = false;
                        _updateCanvasDockTitle(snap.page);
                        snap.page->view()->setEnabled(true);
                    }
                }
                setWindowTitle(tr("AT Drawing Tools - %1").arg(projInfo.name));
                watcher->deleteLater();
            });

    // Flatten all inputs
    QList<SerializeInput> allInputs;
    for (auto &snap : snapshots)
        allInputs.append(snap.inputs);

    auto f = QtConcurrent::mapped(allInputs, serializeItemWorker);
    watcher->setFuture(f);
}

void MainWindow::onImportImage()
{
    // Capture active canvas page before any modal dialog — modal dialogs
    // run an event loop, and focus-return can trigger _onCanvasDockActivated
    // which reassigns m_pView / m_undoStack to a different canvas.
    if (!m_pView || !m_pView->canvasItem())
        return;

    QAtGraphicsView *view = m_pView;
    QUndoStack *undoStack = m_undoStack;
    QPointer<QAtCanvasPage> page(qobject_cast<QAtCanvasPage *>(_currentCanvasPage()));
    if (!page)
        return;
    CanvasItem *canvas = page->canvasItem();
    if (!canvas)
        return;

    const QStringList paths =
        QFileDialog::getOpenFileNames(this, tr("Import Images"), QString(),
                                      tr("Images (*.tif *.tiff *.png *.jpg *.jpeg *.bmp);;"
                                         "TIFF (*.tif *.tiff);;"
                                         "PNG (*.png);;"
                                         "JPEG (*.jpg *.jpeg);;"
                                         "BMP (*.bmp);;"
                                         "All Files (*)"));
    if (paths.isEmpty())
        return;

    if (paths.size() == 1)
        importSingleImage(paths, view, undoStack, page, canvas);
    else
        importMultipleImages(paths, view, undoStack, page, canvas);
}

void MainWindow::importSingleImage(const QStringList &paths, QAtGraphicsView *view,
                                   QUndoStack *undoStack, QPointer<QAtCanvasPage> page,
                                   CanvasItem *canvas)
{
    FitCanvasDlg dlg;
    dlg.setParam(canvas->canvasSize());
    if (dlg.exec() != QDialog::Accepted)
        return;
    FitCanvasType fitType = dlg.fitType();
    double fitVal = dlg.fitValue();

    const QString taskId = m_pProgressMgr->startTask(tr("Import"), paths.size());

    auto *watcher = new QFutureWatcher<ImageUtils::ImportWorkerResult>(this);
    auto *importedItems = new QList<ImageItem *>();
    auto runningY = std::make_shared<qreal>(0);

    connect(
        watcher, &QFutureWatcher<ImageUtils::ImportWorkerResult>::progressValueChanged, this,
        [this, taskId](int progressValue) { m_pProgressMgr->updateTask(taskId, progressValue); });

    connect(watcher, &QFutureWatcher<ImageUtils::ImportWorkerResult>::resultReadyAt, this,
            [this, watcher, importedItems, runningY, view, undoStack, page](int index) {
                if (!page)
                    return;
                auto result = watcher->resultAt(index);
                if (result.isValid()) {
                    // 根据图片 DPI 与画布 PPI 计算场景像素尺寸，保持物理尺寸一致
                    qreal canvasPpi = view->canvasItem()->ppi();
                    QSize sceneSize = result.size;
                    if (result.dpiX > 0 && result.dpiY > 0) {
                        sceneSize = QSize(qRound(result.size.width() * canvasPpi / result.dpiX),
                                          qRound(result.size.height() * canvasPpi / result.dpiY));
                    }

                    auto *item = new ImageItem(result.pixmap, sceneSize);
                    item->setItemPen(QPen(Qt::NoPen));
                    item->setFilePath(result.path);
                    item->setOriginalSize(result.size);

                    // 设置图片 DPI 到 ImageItem
                    if (result.dpiX > 0 && result.dpiY > 0)
                        item->setDpi(result.dpiX, result.dpiY);

                    item->setPos(0, *runningY);
                    *runningY += sceneSize.height() + 10;

                    undoStack->push(new AddItemCommand(page->scene(), item));
                    importedItems->append(item);
                } else {
                    qWarning() << "Import failed:" << result.path << result.errorMessage;
                }
            });

    connect(watcher, &QFutureWatcher<ImageUtils::ImportWorkerResult>::finished, this,
            [this, watcher, taskId, importedItems, fitType, fitVal, view, undoStack, page]() {
                if (!page) {
                    watcher->deleteLater();
                    delete importedItems;
                    return;
                }
                m_pProgressMgr->finishTask(taskId);
                watcher->deleteLater();

                // 导入完成后刷新状态栏（仅当该页仍是活动页时）
                if (_currentCanvasPage() == page) {
                    _updateCanvasLabel();
                    _updatePosLabel(m_lastScenePos);
                }

                if (fitType != fctNone && !importedItems->isEmpty()) {
                    CanvasItem *canvas = view->canvasItem();
                    if (canvas) {
                        QRectF unitedRect;
                        for (auto *item : *importedItems) {
                            QRectF r = item->mapToScene(item->boundingRect()).boundingRect();
                            unitedRect = unitedRect.isValid() ? unitedRect.united(r) : r;
                        }

                        // 留白边距（mm → px），与 onFitCanvasToItems 一致
                        const AppConfig &cfg = AppConfig::instance();
                        qreal ppm = canvas->pixelsPerMm();
                        qreal marginLeft = cfg.canvasMarginLeft() * ppm;
                        qreal marginRight = cfg.canvasMarginRight() * ppm;
                        qreal marginTop = cfg.canvasMarginTop() * ppm;
                        qreal marginBottom = cfg.canvasMarginBottom() * ppm;

                        qreal offsetX = -unitedRect.left() + marginLeft;
                        qreal offsetY = -unitedRect.top() + marginTop;

                        QSizeF oldSize = canvas->canvasSize();
                        QSizeF newSize;
                        switch (fitType) {
                        case fctAdapt:
                            newSize = QSizeF(unitedRect.width() + marginLeft + marginRight,
                                             unitedRect.height() + marginTop + marginBottom);
                            break;
                        case fctWidth:
                            newSize = QSizeF(fitVal, oldSize.height());
                            break;
                        case fctHeight:
                            newSize = QSizeF(oldSize.width(), fitVal);
                            break;
                        default:
                            break;
                        }

                        if (newSize.isValid() && newSize.width() > 0 && newSize.height() > 0
                            && newSize != oldSize) {
                            undoStack->beginMacro(tr("Fit Canvas on Import"));

                            if (offsetX != 0 || offsetY != 0) {
                                QPointF delta(offsetX, offsetY);
                                QList<QPointF> oldPositions, newPositions;
                                for (auto *item : *importedItems) {
                                    oldPositions << item->pos();
                                    newPositions << item->pos() + delta;
                                    item->setPos(item->pos() + delta);
                                }
                                undoStack->push(new MoveItemsCommand(
                                    QList<QGraphicsItem *>(importedItems->begin(),
                                                           importedItems->end()),
                                    oldPositions, newPositions, page->scene()));
                            }

                            undoStack->push(
                                new CanvasResizeCommand(canvas, oldSize, newSize, page->scene()));
                            undoStack->endMacro();
                            if (_currentCanvasPage() == page) {
                                _updateCanvasLabel();
                                view->fitToCanvas();
                            }
                        }
                    }
                }

                delete importedItems;
                atDebug() << "Finished import";
            });

    auto future = QtConcurrent::mapped(paths, ImageUtils::runImportWorker);
    watcher->setFuture(future);
    atDebug() << "Started import task for" << paths.size() << "images";
}

void MainWindow::importMultipleImages(const QStringList &paths, QAtGraphicsView *view,
                                      QUndoStack *undoStack, QPointer<QAtCanvasPage> page,
                                      CanvasItem *canvas)
{
    ImageArrangementDialog dlg;
    dlg.setFilePaths(paths);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const ImageArrangement arr = dlg.arrangement();
    const QStringList ordered = dlg.orderedPaths();

    FitCanvasDlg fitDlg;
    fitDlg.setParam(canvas->canvasSize());
    if (fitDlg.exec() != QDialog::Accepted)
        return;
    FitCanvasType fitType = fitDlg.fitType();
    double fitVal = fitDlg.fitValue();

    const QString taskId = m_pProgressMgr->startTask(tr("Import"), ordered.size());

    auto *watcher = new QFutureWatcher<ImageUtils::ImportWorkerResult>(this);
    auto *importedItems = new QList<ImageItem *>();
    auto runningCoord = std::make_shared<qreal>(0);

    connect(
        watcher, &QFutureWatcher<ImageUtils::ImportWorkerResult>::progressValueChanged, this,
        [this, taskId](int progressValue) { m_pProgressMgr->updateTask(taskId, progressValue); });

    connect(watcher, &QFutureWatcher<ImageUtils::ImportWorkerResult>::resultReadyAt, this,
            [this, watcher, importedItems, runningCoord, arr, view, undoStack, page](int index) {
                if (!page)
                    return;
                auto result = watcher->resultAt(index);
                if (result.isValid()) {
                    qreal canvasPpi = view->canvasItem()->ppi();
                    QSize sceneSize = result.size;
                    if (result.dpiX > 0 && result.dpiY > 0) {
                        sceneSize = QSize(qRound(result.size.width() * canvasPpi / result.dpiX),
                                          qRound(result.size.height() * canvasPpi / result.dpiY));
                    }

                    auto *item = new ImageItem(result.pixmap, sceneSize);
                    item->setItemPen(QPen(Qt::NoPen));
                    item->setFilePath(result.path);
                    item->setOriginalSize(result.size);

                    if (result.dpiX > 0 && result.dpiY > 0)
                        item->setDpi(result.dpiX, result.dpiY);

                    if (arr == ArrangeHorizontal) {
                        item->setPos(*runningCoord, 0);
                        *runningCoord += sceneSize.width();
                    } else {
                        item->setPos(0, *runningCoord);
                        *runningCoord += sceneSize.height();
                    }

                    undoStack->push(new AddItemCommand(page->scene(), item));
                    importedItems->append(item);
                } else {
                    qWarning() << "Import failed:" << result.path << result.errorMessage;
                }
            });

    connect(watcher, &QFutureWatcher<ImageUtils::ImportWorkerResult>::finished, this,
            [this, watcher, taskId, importedItems, fitType, fitVal, view, undoStack, page]() {
                if (!page) {
                    watcher->deleteLater();
                    delete importedItems;
                    return;
                }
                m_pProgressMgr->finishTask(taskId);
                watcher->deleteLater();

                if (_currentCanvasPage() == page) {
                    _updateCanvasLabel();
                    _updatePosLabel(m_lastScenePos);
                }

                if (fitType != fctNone && !importedItems->isEmpty()) {
                    CanvasItem *canvas = view->canvasItem();
                    if (canvas) {
                        QRectF unitedRect;
                        for (auto *item : *importedItems) {
                            QRectF r = item->mapToScene(item->boundingRect()).boundingRect();
                            unitedRect = unitedRect.isValid() ? unitedRect.united(r) : r;
                        }

                        const AppConfig &cfg = AppConfig::instance();
                        qreal ppm = canvas->pixelsPerMm();
                        qreal marginLeft = cfg.canvasMarginLeft() * ppm;
                        qreal marginRight = cfg.canvasMarginRight() * ppm;
                        qreal marginTop = cfg.canvasMarginTop() * ppm;
                        qreal marginBottom = cfg.canvasMarginBottom() * ppm;

                        qreal offsetX = -unitedRect.left() + marginLeft;
                        qreal offsetY = -unitedRect.top() + marginTop;

                        QSizeF oldSize = canvas->canvasSize();
                        QSizeF newSize;
                        switch (fitType) {
                        case fctAdapt:
                            newSize = QSizeF(unitedRect.width() + marginLeft + marginRight,
                                             unitedRect.height() + marginTop + marginBottom);
                            break;
                        case fctWidth:
                            newSize = QSizeF(fitVal, oldSize.height());
                            break;
                        case fctHeight:
                            newSize = QSizeF(oldSize.width(), fitVal);
                            break;
                        default:
                            break;
                        }

                        if (newSize.isValid() && newSize.width() > 0 && newSize.height() > 0
                            && newSize != oldSize) {
                            undoStack->beginMacro(tr("Fit Canvas on Import"));

                            if (offsetX != 0 || offsetY != 0) {
                                QPointF delta(offsetX, offsetY);
                                QList<QPointF> oldPositions, newPositions;
                                for (auto *item : *importedItems) {
                                    oldPositions << item->pos();
                                    newPositions << item->pos() + delta;
                                    item->setPos(item->pos() + delta);
                                }
                                undoStack->push(new MoveItemsCommand(
                                    QList<QGraphicsItem *>(importedItems->begin(),
                                                           importedItems->end()),
                                    oldPositions, newPositions, page->scene()));
                            }

                            undoStack->push(
                                new CanvasResizeCommand(canvas, oldSize, newSize, page->scene()));
                            undoStack->endMacro();
                            if (_currentCanvasPage() == page) {
                                _updateCanvasLabel();
                                view->fitToCanvas();
                            }
                        }
                    }
                }

                delete importedItems;
                atDebug() << "Finished import";
            });

    auto future = QtConcurrent::mapped(ordered, ImageUtils::runImportWorker);
    watcher->setFuture(future);
    atDebug() << "Started import task for" << paths.size() << "images";
}

// 递归检查图元列表中是否包含 ImageItem（会遍历组内的子图元）
static bool containsImageItemRecursive(const QList<QGraphicsItem *> &items)
{
    for (auto *item : items) {
        if (dynamic_cast<ImageItem *>(item))
            return true;
        auto *group = dynamic_cast<GraphicsItemGroup *>(item);
        if (group && containsImageItemRecursive(group->childItems()))
            return true;
    }
    return false;
}

void MainWindow::onExportImage()
{
#ifdef USE_LEGACY_EXPORT
    // === 旧导出路径: TiffExportEngine ===
    if (m_tiffEngine->isRunning()) {
        QMessageBox::information(this, tr("Export"), tr("An export is already in progress."));
        return;
    }

    // 3. 检查是否需要用户设置 DPI（无图片图元 且 画布 DPI 未锁定）
    int dpiOverride = 0;
    {
        auto *canvas = m_pView->canvasItem();
        const auto allItems = ::filterSelectableItems(m_pView->scene()->items());
        bool hasImages = containsImageItemRecursive(allItems);
        if (!hasImages && (!canvas || !canvas->isDpiLocked())) {
            QStringList dpiItems;
            for (int dpi : kDpiValues)
                dpiItems << QString::number(dpi);
            bool ok = false;
            const QString chosen =
                QInputDialog::getItem(this, tr("Set Export DPI"),
                                      tr("No image items on the canvas.\n"
                                         "Please set the export DPI:"),
                                      dpiItems, /* current = */ 2, /* editable = */ false, &ok);
            if (!ok)
                return; // 用户取消导出
            dpiOverride = chosen.toInt();
        }
    }

    // 应用用户自定义 DPI 到画布（保持物理尺寸，但不锁定 DPI）
    if (dpiOverride > 0) {
        auto *canvas = m_pView->canvasItem();
        if (canvas) {
            // 先算出当前物理 mm（在修改 DPI 前）
            QSizeF oldSize = canvas->canvasSize();
            qreal oldPpi = canvas->ppi();
            AtMath::Units::DPIContext ctxOld(oldPpi);
            qreal mmW = ctxOld.pxToMm(oldSize.width());
            qreal mmH = ctxOld.pxToMm(oldSize.height());

            // 更新画布 DPI（不锁定）
            canvas->setCanvasDpi(dpiOverride, dpiOverride);
            canvas->setPpi(static_cast<qreal>(dpiOverride));

            // 从物理 mm 反算新像素尺寸（ceil 保证整数边界，消除取整不一致）
            qreal mmToPx = AtMath::Units::DPIContext(static_cast<qreal>(dpiOverride)).mmToPx(1.0);
            QSizeF newSize(std::ceil(mmW * mmToPx), std::ceil(mmH * mmToPx));
            m_pView->setCanvasSize(newSize);

            // 同步缩放非图片图元的像素尺寸和位置（factor = newPpi/oldPpi）
            qreal factor = static_cast<qreal>(dpiOverride) / oldPpi;
            if (!AtMath::isEqual(factor, 1.0)) {
                const auto selectable = ::filterSelectableItems(m_pView->scene()->items());
                for (auto *item : selectable) {
                    if (dynamic_cast<ImageItem *>(item) || item->parentItem())
                        continue;
                    item->setPos(item->pos() * factor);
                    item->setTransform(QTransform::fromScale(factor, factor) * item->transform());
                }
            }

            if (auto *page = qobject_cast<QAtCanvasPage *>(_currentCanvasPage()))
                page->setRulerPpi(static_cast<qreal>(dpiOverride));
            m_pPropertyPanel->setDisplayPpi(static_cast<qreal>(dpiOverride));
            if (auto *page = qobject_cast<QAtCanvasPage *>(_currentCanvasPage()))
                page->updateRulers();
            _updateCanvasLabel();
        }
    }

    // 1. 选择保存路径
    QString path =
        QFileDialog::getSaveFileName(this, tr("Export Image"), QString(), tr("prn Files (*.prn)"));
    if (path.isEmpty())
        return;

    QFileInfo fi(path);

    // 2. RIP 设置对话框
    bool bRip = false;
    int ripXRes = 0, ripYRes = 0;
    SettingsDialog dlg(this);
    dlg.setOutputPath(fi.absolutePath());
    if (dlg.exec() == QDialog::Accepted) {
        bRip = true;
        ripXRes = dlg.resolutionX();
        ripYRes = dlg.resolutionY();
    }

    const QString tiffPath = fi.absolutePath() + "/" + fi.completeBaseName() + ".tif";

    // 4. 委托给导出引擎 — 自动完成场景检查、Overlay 渲染、后台线程导出
    //    结果通过 TiffExportEngine::exportFinished / progressChanged 信号回传
    m_exportTaskId = m_pProgressMgr->startTask(tr("Export"));
    m_ripEnabled = bRip;
    m_ripXRes = ripXRes;
    m_ripYRes = ripYRes;

    if (!m_tiffEngine->startExport(m_pView->scene(), m_pView, tiffPath, dpiOverride)) {
        m_pProgressMgr->cancelTask(m_exportTaskId);
        QMessageBox::warning(this, tr("Export"), m_tiffEngine->lastError());
    }
#else
    // === 新导出路径: SceneToJsonConverter → ExportEngine ===

    // 1. 必须选择导出 DPI（不修改画布，仅在 JSON 转换时按比例缩放坐标）
    QStringList dpiItems;
    for (int dpi : kDpiValues)
        dpiItems << QString::number(dpi);
    bool ok = false;
    const QString chosen =
        QInputDialog::getItem(this, tr("Set Export DPI"), tr("Please select the export DPI:"),
                              dpiItems, /* current = */ 2, /* editable = */ false, &ok);
    if (!ok)
        return; // 用户取消导出
    int dpiOverride = chosen.toInt();

    // 2. 选择保存路径（.prn，派生 .tif）
    QString path =
        QFileDialog::getSaveFileName(this, tr("Export Image"), QString(), tr("prn Files (*.prn)"));
    if (path.isEmpty())
        return;

    QFileInfo fi(path);

    // 3. RIP 设置对话框
    bool bRip = false;
    int ripXRes = 0, ripYRes = 0;
    SettingsDialog dlg(this);
    dlg.setOutputPath(fi.absolutePath());
    if (dlg.exec() == QDialog::Accepted) {
        bRip = true;
        ripXRes = dlg.resolutionX();
        ripYRes = dlg.resolutionY();
    }

    const QString tiffPath = fi.absolutePath() + "/" + fi.completeBaseName() + ".tif";

    // 4. 场景 → JSON
    const auto &cfg = AppConfig::instance();
    auto convertResult =
        SceneToJsonConverter::convert(m_pView->scene(), { } /* rgbProfile */, { } /* cmykProfile */,
                                      { } /* grayProfile */, dpiOverride);
    if (!convertResult.success) {
        QMessageBox::warning(this, tr("Export"),
                             tr("Failed to convert scene: %1").arg(convertResult.errorMessage));
        return;
    }

    // 5. 启动进度 → 后台 ExportEngine 渲染
    m_exportTaskId = m_pProgressMgr->startTask(tr("Export"));
    m_ripEnabled = bRip;
    m_ripXRes = ripXRes;
    m_ripYRes = ripYRes;

    QString json = convertResult.json;

    QFile jsonFile(fi.absolutePath() + "/" + fi.completeBaseName() + ".json");
    if (jsonFile.open(QIODevice::WriteOnly)) {
        jsonFile.write(json.toUtf8());
        jsonFile.close();
    } else {
        qWarning() << "Failed to write intermediate JSON file:" << jsonFile.errorString();
    }

    m_exportFuture =
        QtConcurrent::run([this, json, tiffPath]() { exportWithEngine(json, tiffPath); });
#endif
}

// ============================================================
// ExportEngine 后台渲染（运行在 QtConcurrent 线程中）
// ============================================================
void MainWindow::exportWithEngine(const QString &json, const QString &outputPath)
{
    // helper: EE error strings are UTF-8 (nlohmann::json uses UTF-8 internally);
    // QString::fromStdString() uses the system local code page (GBK on Chinese Windows)
    // and would garble any non-ASCII content in the error message.
    auto toQString = [](const std::string &s) {
        return QString::fromUtf8(s.data(), static_cast<int>(s.size()));
    };

    // 1. 解析 JSON → Canvas
    ATHC::EE::Canvas canvas;
    ATHC::EE::JsonSceneParser parser;
    try {
        if (!parser.parseFromJson(json.toStdString(), canvas)) {
            emit exportError(toQString(parser.errorString()));
            return;
        }
    } catch (const std::exception &e) {
        emit exportError(tr("JSON parse exception: %1").arg(QString::fromUtf8(e.what())));
        return;
    } catch (...) {
        emit exportError(tr("Unknown exception during JSON parsing"));
        return;
    }

    // 2. 加载 ICC 配置文件
    ATHC::EE::ColorConverter converter;
    const auto &cfg = AppConfig::instance();
    std::string rgbPath = cfg.srgbIccPath().toStdString();
    std::string cmykPath = cfg.cmykIccPath().toStdString();
    std::string grayPath = cfg.grayIccPath().toStdString();

    if (!converter.loadProfile(rgbPath, cmykPath, grayPath)) {
        emit exportError(toQString(converter.errorString()));
        return;
    }

    // 3. 获取 CMYK ICC 字节（嵌入 TIFF）
    std::vector<uint8_t> iccBytes;
    converter.getProfileBytes(iccBytes);

    // 4. 确定分块尺寸（大画布用 1024，小画布用 512）
    int tileSize = (canvas.width > 10000 || canvas.height > 10000) ? 1024 : 512;

#ifdef EXPORT_USE_STRIP_WRITE
    // ── Strip write path ──────────────────────────────────────────────
    // Assemble rendered tiles into full-frame buffers, then call write()
    // which uses strip-based TIFFWriteEncodedStrip internally.

    const size_t frameBytes = static_cast<size_t>(canvas.width) * canvas.height * 4;
    ATHC::EE::ImageBuffer fullRGBA(canvas.width, canvas.height);
    std::vector<uint8_t> fullCmyk(frameBytes, 0);

    int tilesX = (canvas.width + tileSize - 1) / tileSize;
    int tilesY = (canvas.height + tileSize - 1) / tileSize;
    int totalTiles = tilesX * tilesY;

    std::atomic<int> doneTiles{ 0 };

    ATHC::EE::SceneRenderer renderer;
    renderer.setConverter(&converter);
    renderer.renderTiled(
        canvas, tileSize,
        [&](int tx, int ty, const std::vector<uint8_t> &cmykTile,
            const ATHC::EE::ImageBuffer &rgbaTile) {
            int tw = rgbaTile.width();
            int th = rgbaTile.height();
            int x0 = tx, y0 = ty;

            // Copy RGBA tile into full-frame buffer
            for (int r = 0; r < th; ++r)
                std::memcpy(fullRGBA.scanLine(y0 + r) + x0 * 4, rgbaTile.scanLine(r),
                            static_cast<size_t>(tw) * 4);

            // Copy CMYK tile into full-frame buffer
            if (!cmykTile.empty()) {
                for (int r = 0; r < th; ++r)
                    std::memcpy(fullCmyk.data()
                                    + (static_cast<size_t>(y0 + r) * canvas.width + x0) * 4,
                                cmykTile.data() + static_cast<size_t>(r) * tw * 4,
                                static_cast<size_t>(tw) * 4);
            }

            int done = ++doneTiles;
            int reportInterval = std::max(totalTiles / 10, 1);
            if (done % reportInterval == 0 || done == totalTiles)
                emit exportProgress(done * 100 / totalTiles);
        });

    // Strip-based write: processes the full frame in ~8 MB strips internally
    ATHC::EE::TiffWriter writer;
    writer.setConverter(&converter);
    if (!writer.write(outputPath.toStdString(), fullCmyk, fullRGBA, canvas.dpi)) {
        emit exportError(toQString(writer.errorString()));
        return;
    }

#else
    // ── Tile write path ────────────────────────────────────────────────

    // 5. 打开输出 TIFF（流式分块写入）
    ATHC::EE::TiffWriter writer;
    writer.setConverter(&converter);
    if (!writer.beginWrite(outputPath.toStdString(), canvas.width, canvas.height, canvas.dpi,
                           tileSize, iccBytes)) {
        emit exportError(toQString(writer.errorString()));
        return;
    }

    // 6. 分块渲染 + 写入
    int tilesX = (canvas.width + tileSize - 1) / tileSize;
    int tilesY = (canvas.height + tileSize - 1) / tileSize;
    int totalTiles = tilesX * tilesY;

    std::atomic<int> doneTiles{ 0 };
    std::atomic<bool> writeOk{ true };

    ATHC::EE::SceneRenderer renderer;
    renderer.setConverter(&converter);
    renderer.renderTiled(
        canvas, tileSize,
        [&](int tileX, int tileY, const std::vector<uint8_t> &cmykTile,
            const ATHC::EE::ImageBuffer &rgbaTile) {
            if (!writeOk.load())
                return;

            int tileW = std::min(tileSize, canvas.width - tileX);
            int tileH = std::min(tileSize, canvas.height - tileY);

            if (!writer.writeTile(tileX, tileY, tileW, tileH, cmykTile, rgbaTile)) {
                writeOk.store(false);
                return;
            }

            int done = ++doneTiles;
            // 每完成 ~10% 的 tile 报告一次进度
            int reportInterval = std::max(totalTiles / 10, 1);
            if (done % reportInterval == 0 || done == totalTiles) {
                int pct = done * 100 / totalTiles;
                emit exportProgress(pct);
            }
        });

    if (!writeOk.load()) {
        emit exportError(toQString(writer.errorString()));
        return;
    }

    // 7. 关闭 TIFF
    if (!writer.endWrite()) {
        emit exportError(toQString(writer.errorString()));
        return;
    }
#endif

    emit exportComplete(outputPath);
}

// ============================================================
// 对齐与分布面板
// ============================================================
// ============================================================
// 对齐（菜单快捷入口）
// ============================================================
// 对齐/分布辅助方法
// ============================================================
void MainWindow::applyAlign(AlignmentUtils::AlignDirection direction)
{
    auto items = filterSelectableItems();
    if (items.size() < 2)
        return;

    auto result = AlignmentUtils::computeAlign(items, direction);
    if (!result.valid)
        return;

    m_undoStack->push(new AlignItemsCommand(
        items, result.oldPositions, result.newPositions,
        tr("Align %1").arg(AlignmentUtils::alignDirectionName(direction)), m_pView->scene()));
}

void MainWindow::applyDistribute(AlignmentUtils::DistributeDirection direction,
                                 const AlignmentUtils::DistributeParams &params)
{
    auto items = filterSelectableItems();
    if (items.size() < 2)
        return;

    auto result = AlignmentUtils::computeDistribute(items, direction, params);
    if (!result.valid)
        return;

    m_undoStack->push(new AlignItemsCommand(
        items, result.oldPositions, result.newPositions,
        tr("Distribute %1").arg(AlignmentUtils::distributeDirectionName(direction)),
        m_pView->scene()));
}

// ============================================================
void MainWindow::onAlignLeft()
{
    applyAlign(AlignmentUtils::AlignLeft);
}

void MainWindow::onAlignRight()
{
    applyAlign(AlignmentUtils::AlignRight);
}

void MainWindow::onAlignTop()
{
    applyAlign(AlignmentUtils::AlignTop);
}

void MainWindow::onAlignBottom()
{
    applyAlign(AlignmentUtils::AlignBottom);
}

void MainWindow::onAlignHCenter()
{
    applyAlign(AlignmentUtils::AlignHCenter);
}

void MainWindow::onAlignVCenter()
{
    applyAlign(AlignmentUtils::AlignVCenter);
}

void MainWindow::onDistributeH()
{
    applyDistribute(AlignmentUtils::DistributeH);
}

void MainWindow::onDistributeV()
{
    applyDistribute(AlignmentUtils::DistributeV);
}

// ============================================================
// 选择变更
// ============================================================
void MainWindow::onSelectionChanged()
{
    auto items = m_pView->scene()->selectedItems();
    auto selectable = ::filterSelectableItems(items);

    if (selectable.size() == 1) {
        m_pPropertyPanel->setItem(selectable.first());
    } else {
        m_pPropertyPanel->setItem(nullptr);
    }
}

void MainWindow::onItemAdded(QGraphicsItem *item)
{
    // 新图元添加后自动选中
    m_pView->scene()->clearSelection();
    item->setSelected(true);
}

// ============================================================
// 属性变更（通过属性面板触发，创建撤销命令）
// ============================================================
void MainWindow::onPenChanged(QGraphicsItem *item, const QPen &oldPen, const QPen &newPen)
{
    m_undoStack->push(new PropertyChangeCommand(item, PropertyChangeCommand::Pen,
                                                QVariant::fromValue(oldPen),
                                                QVariant::fromValue(newPen), m_pView->scene()));
}

void MainWindow::onBrushChanged(QGraphicsItem *item, const QBrush &oldBrush, const QBrush &newBrush)
{
    m_undoStack->push(new PropertyChangeCommand(item, PropertyChangeCommand::Brush,
                                                QVariant::fromValue(oldBrush),
                                                QVariant::fromValue(newBrush), m_pView->scene()));
}

void MainWindow::onFontChanged(QGraphicsItem *item, const QFont &oldFont, const QFont &newFont)
{
    m_undoStack->push(new PropertyChangeCommand(item, PropertyChangeCommand::Font,
                                                QVariant::fromValue(oldFont),
                                                QVariant::fromValue(newFont), m_pView->scene()));
}

void MainWindow::onTextChanged(QGraphicsItem *item, const QString &oldText, const QString &newText)
{
    m_undoStack->push(new PropertyChangeCommand(
        item, PropertyChangeCommand::Text, QVariant(oldText), QVariant(newText), m_pView->scene()));
}

void MainWindow::onGeometryChanged(QGraphicsItem *item, const QRectF &oldRect,
                                   const QRectF &newRect)
{
    m_undoStack->push(new PropertyChangeCommand(item, PropertyChangeCommand::Geometry,
                                                QVariant(oldRect), QVariant(newRect),
                                                m_pView->scene()));
    // 尺寸变更后需要更新 ResizeHandleItem 以正确显示选中框
    m_pView->scheduleResizeHandleUpdate();
}

void MainWindow::onCornerRadiusChanged(QGraphicsItem *item, qreal oldR, qreal newR)
{
    m_undoStack->push(new PropertyChangeCommand(item, PropertyChangeCommand::CornerRadius,
                                                QVariant(oldR), QVariant(newR), m_pView->scene()));
}

void MainWindow::onPositionChanged(QGraphicsItem *item, const QPointF &oldPos,
                                   const QPointF &newPos)
{
    m_undoStack->push(new PositionChangeCommand(item, oldPos, newPos, m_pView->scene()));
    // 位置变更后需要更新 ResizeHandleItem 以正确显示选中框
    m_pView->scheduleResizeHandleUpdate();
}

void MainWindow::onRotationChanged(QGraphicsItem *item, qreal oldRotation, qreal newRotation)
{
    m_undoStack->push(new RotationChangeCommand(item, oldRotation, newRotation, m_pView->scene()));
    // 旋转后需要更新 ResizeHandleItem 以正确显示选中框
    m_pView->scheduleResizeHandleUpdate();
}

void MainWindow::onRequestFinished(const QJsonDocument &json, NetworkRequestType type)
{
    // 解析返回数据
    if (json.isEmpty())
        return;

    switch (type) {
    case NetworkRequestType::RequestHelpAbout:
        break;
    case NetworkRequestType::RequestAddRip: {
        m_ripTaskId = m_pProgressMgr->startTask(tr("RIP"), RIP_PROGRESS_MAX);
        m_pNetWorkUtils->doWhileRipStatus();

    } break;
    case NetworkRequestType::RequestRipStatus: {
        QJsonValue value = json.object().value("rip_picture_progress");
        int progress = value.toInt();
        m_pProgressMgr->updateTask(m_ripTaskId, progress);
        if (progress >= RIP_PROGRESS_MAX) {
            m_pNetWorkUtils->doStopWhile();
            m_pProgressMgr->finishTask(m_ripTaskId);
            setEnabled(true);
        }
    } break;
    case NetworkRequestType::RequestRipVersion: {
        QJsonValue value = json.object().value("ripVersion");
        RipVersion = value.toString();
    } break;

    default:
        break;
    }
}

void MainWindow::onUpdateInfo()
{
    // 更新信息
}

// ============================================================
// 画布适配选中图元（无选中时适配全部图元）
// ============================================================
void MainWindow::onFitCanvasToItems()
{
    CanvasItem *canvas = m_pView->canvasItem();
    if (!canvas)
        return;

    // 优先使用选中的图元，无选中时使用场景中全部可操作图元
    auto selected = filterSelectableItems();
    QList<QGraphicsItem *> items =
        selected.isEmpty() ? ::filterSelectableItems(m_pView->scene()->items()) : selected;
    if (items.isEmpty())
        return;

    // 计算所有目标图元的场景包围矩形并集
    QRectF unitedRect;
    for (auto *item : items) {
        auto *igi = dynamic_cast<IGraphicsItem *>(item);
        QRectF localRect =
            (igi && igi->supportsGeometryRect()) ? igi->geometryRect() : item->boundingRect();
        QRectF itemSceneRect = item->mapToScene(localRect).boundingRect();
        unitedRect = unitedRect.isValid() ? unitedRect.united(itemSceneRect) : itemSceneRect;
    }

    if (!unitedRect.isValid() || unitedRect.width() < 1 || unitedRect.height() < 1)
        return;

    // 留白（从首选项读取，单位 mm → 像素）
    const AppConfig &cfg = AppConfig::instance();
    qreal ppm = canvas->pixelsPerMm();
    qreal marginLeft = cfg.canvasMarginLeft() * ppm;
    qreal marginRight = cfg.canvasMarginRight() * ppm;
    qreal marginTop = cfg.canvasMarginTop() * ppm;
    qreal marginBottom = cfg.canvasMarginBottom() * ppm;

    // 将图元整体平移到 (marginLeft, marginTop) 起始
    qreal offsetX = -unitedRect.left() + marginLeft;
    qreal offsetY = -unitedRect.top() + marginTop;

    // 画布尺寸 = 图元包围盒 + 四周留白
    QSizeF newSize(unitedRect.width() + marginLeft + marginRight,
                   unitedRect.height() + marginTop + marginBottom);
    QSizeF oldSize = canvas->canvasSize();

    if (newSize.width() <= 0 || newSize.height() <= 0)
        return;

    m_undoStack->beginMacro(tr("Fit Canvas to Selection"));

    // 平移图元到原点
    if (offsetX != 0 || offsetY != 0) {
        QPointF delta(offsetX, offsetY);
        QList<QPointF> oldPositions, newPositions;
        for (auto *item : items) {
            oldPositions << item->pos();
            newPositions << item->pos() + delta;
            item->setPos(item->pos() + delta);
        }
        m_undoStack->push(
            new MoveItemsCommand(items, oldPositions, newPositions, m_pView->scene()));
    }

    // 画布尺寸变更
    if (oldSize != newSize)
        m_undoStack->push(new CanvasResizeCommand(canvas, oldSize, newSize, m_pView->scene()));

    m_undoStack->endMacro();
    _updateCanvasLabel();
    m_pView->fitToCanvas();
}

// ============================================================
// 弹出对话框修改画布尺寸
// ============================================================
void MainWindow::onResizeCanvas()
{
    CanvasItem *canvas = m_pView->canvasItem();
    if (!canvas)
        return;

    ResizeCanvasDialog dlg(this);
    dlg.setCurrentSizeMM(canvas->canvasSize(), canvas->ppi());
    if (dlg.exec() != QDialog::Accepted)
        return;

    QSizeF newSize = dlg.newPixelSize(canvas->ppi());
    QSizeF oldSize = canvas->canvasSize();

    if (newSize == oldSize)
        return;

    m_undoStack->push(new CanvasResizeCommand(canvas, oldSize, newSize, m_pView->scene()));

    if (auto *page = qobject_cast<QAtCanvasPage *>(_currentCanvasPage()))
        page->updateRulers();
    _updateCanvasLabel();
    _updatePosLabel(m_lastScenePos);
    m_pView->fitToCanvas();
}

// ============================================================
// 选择过滤
// ============================================================
QList<QGraphicsItem *> MainWindow::filterSelectableItems() const
{
    return ::filterSelectableItems(m_pView->scene()->selectedItems());
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // 快捷键切换工具 (single key, no modifiers)
    // Tool → token mapping for trigger via AppContext
    static const QMap<int, QString> keyToToken = {
        { Qt::Key_V, QStringLiteral("SelectTool") },
        { Qt::Key_R, QStringLiteral("RectTool") },
        { Qt::Key_E, QStringLiteral("EllipseTool") },
        { Qt::Key_L, QStringLiteral("LineTool") },
        { Qt::Key_C, QStringLiteral("BezierCurveTool") },
        { Qt::Key_F, QStringLiteral("FreehandTool") },
        { Qt::Key_T, QStringLiteral("TextTool") },
    };
    if (!event->modifiers()) {
        auto it = keyToToken.find(event->key());
        if (it != keyToToken.end()) {
            QAction *act = AppContext::get().getQAction(it.value());
            if (act) {
                act->trigger();
                return;
            }
        }
    }
    // Delete key — trigger via AppContext action
    if (event->key() == Qt::Key_Delete) {
        QAction *act = AppContext::get().getQAction(QStringLiteral("Delete"));
        if (act) {
            act->trigger();
            return;
        }
    }
    QMainWindow::keyPressEvent(event);
}

// ============================================================
// 窗口状态持久化（工具栏位置、可见性等）
// ============================================================
void MainWindow::loadWindowState()
{
    QSettings settings;

    // 恢复窗口几何信息
    if (settings.contains("window/geometry")) {
        restoreGeometry(settings.value("window/geometry").toByteArray());
    }

    // 恢复窗口状态（工具栏、dockwidget等）
    if (settings.contains("window/state")) {
        restoreState(settings.value("window/state").toByteArray());
    }

    // 恢复工具栏可见性
    QToolBar *fileEditBar = findChild<QToolBar *>("FileEditToolBar");
    QToolBar *drawBar = findChild<QToolBar *>("DrawingToolBar");
    QToolBar *alignToolBar = findChild<QToolBar *>("AlignToolBar");

    if (fileEditBar && settings.contains("toolbar/FileEditToolBar_visible")) {
        fileEditBar->setVisible(settings.value("toolbar/FileEditToolBar_visible").toBool());
    }
    if (drawBar && settings.contains("toolbar/DrawingToolBar_visible")) {
        drawBar->setVisible(settings.value("toolbar/DrawingToolBar_visible").toBool());
    }
    if (alignToolBar && settings.contains("toolbar/AlignToolBar_visible")) {
        alignToolBar->setVisible(settings.value("toolbar/AlignToolBar_visible").toBool());
    }

    // 恢复其他设置
    // If session was loaded, per-tab grid state is already set; only apply
    // QSettings grid if no session tabs were restored.
    if (_canvasCount() == 0 && settings.contains("view/gridVisible")) {
        bool gridVisible = settings.value("view/gridVisible").toBool();
        if (m_pView) {
            m_pView->setGridVisible(gridVisible);
        }
        AppContext::get().refreshAllActions();
    }
}

void MainWindow::saveWindowState()
{
    QSettings settings;

    // 保存窗口几何信息
    settings.setValue("window/geometry", saveGeometry());

    // 保存窗口状态（工具栏、dockwidget等）
    settings.setValue("window/state", saveState());

    // 保存工具栏可见性
    QToolBar *fileEditBar = findChild<QToolBar *>("FileEditToolBar");
    QToolBar *drawBar = findChild<QToolBar *>("DrawingToolBar");
    QToolBar *alignToolBar = findChild<QToolBar *>("AlignToolBar");

    if (fileEditBar) {
        settings.setValue("toolbar/FileEditToolBar_visible", fileEditBar->isVisible());
    }
    if (drawBar) {
        settings.setValue("toolbar/DrawingToolBar_visible", drawBar->isVisible());
    }
    if (alignToolBar) {
        settings.setValue("toolbar/AlignToolBar_visible", alignToolBar->isVisible());
    }

    // QSettings 析构时自动 flush，无需显式 sync()
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // 1. 收集序列化数据（只做一次，quit snapshot 和 session 共用）
    auto bundles = _collectCanvasBundles();

    // 2. 构建完整会话信息
    SessionInfo si;
    si.version = 2;
    si.projectPath = m_projectPath;
    si.activeTabIndex = qMax(0, _currentCanvasIndex());
    si.currentTool = m_currentTool;
    si.ripEnabled = m_ripEnabled;
    si.ripXRes = m_ripXRes;
    si.ripYRes = m_ripYRes;
    si.alignHSpacing = AlignWidget::hSpacing();
    si.alignVSpacing = AlignWidget::vSpacing();

    // 3. 保存退出快照 (Level 3: 崩溃恢复兜底)
    RecoveryManager::instance().saveQuitSnapshot(bundles, m_projectPath, si);

    // 4. 保存完整会话状态（标签页/工具/项目）
    saveSession();

    // 5. 保存窗口状态
    saveWindowState();

    // 断开 view 信号，防止析构期间信号触发访问半销毁状态的对象
    if (m_pView)
        disconnect(m_pView, nullptr, this, nullptr);

    // 清空属性面板引用，避免悬空指针
    if (m_pPropertyPanel)
        m_pPropertyPanel->setItem(nullptr);

    // 清空 undo 栈，避免命令引用已销毁的图元
    if (m_undoStack)
        m_undoStack->clear();

    if (m_pProgressMgr)
        m_pProgressMgr->resetAll();

    if (m_pProcessGuard)
        m_pProcessGuard->stopAll();

    QMainWindow::closeEvent(event);
}

void MainWindow::_toggleHistoryPopup()
{
    if (!m_taskHistoryPopup)
        return;
    if (m_taskHistoryPopup->isVisible()) {
        m_taskHistoryPopup->hide();
        return;
    }
    m_taskHistoryPopup->showAbove(m_taskHistoryBtn, m_pProgressMgr->activeTaskList(),
                                  m_pProgressMgr->finishedTaskList());
    m_taskHistoryBtn->setChecked(true);
}

// ============================================================
// 智能排版
// ============================================================
void MainWindow::onAutoLayout()
{
    // 1. 收集画布上所有 ImageItem 的文件路径
    QStringList imagePaths;
    QList<ImageItem *> imageItems;
    const auto allItems = ::filterSelectableItems(m_pView->scene()->items());
    for (auto *item : allItems) {
        auto *imgItem = dynamic_cast<ImageItem *>(item);
        if (imgItem && !imgItem->filePath().isEmpty() && QFileInfo::exists(imgItem->filePath())) {
            imagePaths << imgItem->filePath();
            imageItems << imgItem;
        }
    }

    if (imagePaths.isEmpty()) {
        QMessageBox::information(
            this, tr("Auto Layout"),
            tr("No image items with valid source files found on the canvas.\n\n"
               "Only images imported from files (not clipboard) are supported."));
        return;
    }

    // 2. 弹出排版设置对话框
    AutoLayoutDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    // 3. 创建 LayoutEngine（若需要）
    if (!m_layoutEngine) {
        m_layoutEngine = new LayoutEngine(this);
    }

    // 4. 显示进度对话框（模态，禁止操作主界面）
    QProgressDialog progressDlg(this);
    progressDlg.setWindowTitle(tr("Auto Layout"));
    progressDlg.setLabelText(tr("Auto-layout in progress, please wait..."));
    progressDlg.setCancelButtonText(tr("Cancel"));
    progressDlg.setWindowModality(Qt::WindowModal);
    progressDlg.setMinimumDuration(0);
    progressDlg.setRange(0, 100);
    progressDlg.setValue(0);
    progressDlg.setAutoReset(false);
    progressDlg.setAutoClose(false);

    // 5. 连接信号
    bool layoutSuccess = false;
    QStringList outputPaths;
    QString errorMsg;

    connect(m_layoutEngine, &LayoutEngine::progressChanged, &progressDlg,
            &QProgressDialog::setValue);
    connect(&progressDlg, &QProgressDialog::canceled, m_layoutEngine, &LayoutEngine::cancel);

    QEventLoop loop;
    connect(m_layoutEngine, &LayoutEngine::finished, &loop,
            [&](bool success, const QStringList &paths, const QString &error) {
                layoutSuccess = success;
                outputPaths = paths;
                errorMsg = error;
                // Disconnect progress updates so late-arriving signals don't re-show
                // the dialog after it is closed.
                QObject::disconnect(m_layoutEngine, &LayoutEngine::progressChanged, &progressDlg,
                                    &QProgressDialog::setValue);
                loop.quit();
            });

    // 6. 启动排版
    m_layoutEngine->start(imagePaths, dlg.paperWidth(), dlg.elementInterval(), dlg.loopCount(),
                          dlg.operateCount());

    loop.exec();
    progressDlg.close();

    // 7. 处理结果
    if (!layoutSuccess || outputPaths.isEmpty()) {
        QString msg = errorMsg.isEmpty() ? tr("Layout operation produced no output.\n\n"
                                              "The Layout library may have encountered an error.")
                                         : errorMsg;
        QMessageBox::warning(this, tr("Auto Layout"), msg);
        return;
    }

    // 8. 替换图元：删除旧图元，添加新图元（类似导入多图流程，但不弹对话框，默认竖向排列后画布自适应）
    QList<QGraphicsItem *> oldItems;
    for (auto *imgItem : imageItems)
        oldItems << static_cast<QGraphicsItem *>(imgItem);

    m_undoStack->beginMacro(tr("Auto Layout"));

    if (!oldItems.isEmpty())
        m_undoStack->push(new RemoveItemsCommand(m_pView->scene(), oldItems));

    // 导入排版后的图片，纵向排列
    CanvasItem *canvas = m_pView->canvasItem();
    constexpr qreal kVerticalGap = 10.0;
    qreal yOffset = 0;
    QList<ImageItem *> newItems;
    for (const auto &path : outputPaths) {
        // 获取原始尺寸（轻量级，不解码）
        QSize originalSize;
        int dpiX = 0, dpiY = 0;

        if (ImageUtils::isTiffFile(path)) {
            QPair<int, int> dpi = ImageUtils::readTiffDpi(path);
            dpiX = dpi.first;
            dpiY = dpi.second;
            originalSize = ImageUtils::readTiffSize(path);
        } else {
            QImageReader reader(path);
            originalSize = reader.size();
            if (!originalSize.isValid()) {
                reader.setAllocationLimit(0);
                QImage img = reader.read();
                if (img.isNull())
                    continue;
                originalSize = img.size();
            }
        }

        if (!originalSize.isValid())
            continue;

        // 根据图片 DPI 与画布 PPI 计算场景像素尺寸，保持物理尺寸一致
        QSize sceneSize = originalSize;
        if (dpiX > 0 && dpiY > 0) {
            qreal canvasPpi = canvas->ppi();
            sceneSize = QSize(qRound(originalSize.width() * canvasPpi / dpiX),
                              qRound(originalSize.height() * canvasPpi / dpiY));
        }

        // 生成缩略图（缓存感知，自适应目标长边 800px）
        QImage thumb = ImageUtils::generateDisplayThumbnail(path);
        if (thumb.isNull())
            continue;

        QPixmap pix = QPixmap::fromImage(thumb);
        auto *item = new ImageItem(pix, sceneSize);
        item->setItemPen(QPen(Qt::NoPen));
        item->setFilePath(path);
        item->setOriginalSize(originalSize);
        item->setPos(0, yOffset);

        if (dpiX > 0 && dpiY > 0)
            item->setDpi(dpiX, dpiY);

        m_undoStack->push(new AddItemCommand(m_pView->scene(), item));
        newItems.append(item);
        yOffset += sceneSize.height() + kVerticalGap;
    }

    // 画布自适应：根据新图元自动调整画布大小（带留白边距，与 onFitCanvasToItems 一致）
    if (!newItems.isEmpty() && canvas) {
        QRectF unitedRect;
        for (auto *item : newItems) {
            QRectF r = item->mapToScene(item->boundingRect()).boundingRect();
            unitedRect = unitedRect.isValid() ? unitedRect.united(r) : r;
        }

        // 留白边距（mm → px），与 onFitCanvasToItems 一致
        const AppConfig &cfg = AppConfig::instance();
        qreal ppm = canvas->pixelsPerMm();
        qreal marginLeft = cfg.canvasMarginLeft() * ppm;
        qreal marginRight = cfg.canvasMarginRight() * ppm;
        qreal marginTop = cfg.canvasMarginTop() * ppm;
        qreal marginBottom = cfg.canvasMarginBottom() * ppm;

        qreal offsetX = -unitedRect.left() + marginLeft;
        qreal offsetY = -unitedRect.top() + marginTop;

        QSizeF oldSize = canvas->canvasSize();
        QSizeF newSize(unitedRect.width() + marginLeft + marginRight,
                       unitedRect.height() + marginTop + marginBottom);

        if (newSize.isValid() && newSize.width() > 0 && newSize.height() > 0
            && newSize != oldSize) {
            if (offsetX != 0 || offsetY != 0) {
                QPointF delta(offsetX, offsetY);
                QList<QPointF> oldPositions, newPositions;
                for (auto *item : newItems) {
                    oldPositions << item->pos();
                    newPositions << item->pos() + delta;
                    item->setPos(item->pos() + delta);
                }
                m_undoStack->push(
                    new MoveItemsCommand(QList<QGraphicsItem *>(newItems.begin(), newItems.end()),
                                         oldPositions, newPositions, m_pView->scene()));
            }
            m_undoStack->push(new CanvasResizeCommand(canvas, oldSize, newSize, m_pView->scene()));
        }
    }

    m_undoStack->endMacro();

    // 画布自适应后刷新视图和状态栏（与导入多图 finished 处理一致）
    if (!newItems.isEmpty()) {
        _updateCanvasLabel();
        _updatePosLabel(m_lastScenePos);
        m_pView->fitToCanvas();
    }

    // Clean up only the input copy directory — keep the output directory
    QDir(QCoreApplication::applicationDirPath() + "/tmp_layout/in").removeRecursively();

    // 9. 显示结果
    if (!errorMsg.isEmpty()) {
        QMessageBox::information(this, tr("Auto Layout"),
                                 tr("Layout completed with warnings:\n%1").arg(errorMsg));
    }
}

// ========== 项目加载后异步刷新图片缩略图（缓存感知） ==========

void MainWindow::refreshImageItemsFromCache(const QList<QGraphicsItem *> &items)
{
    for (auto *it : items) {
        auto *imgItem = dynamic_cast<ImageItem *>(it);
        if (!imgItem || imgItem->filePath().isEmpty())
            continue;
        if (!QFile::exists(imgItem->filePath()))
            continue;

        // 异步任务仅为从缓存加载 PNG（≤1ms 磁盘读取），项目刚加载完
        // 图元不会被立即删除，无需弱引用保护
        ImageItem *target = imgItem;
        QString path = imgItem->filePath();

        auto *watcher = new QFutureWatcher<QImage>(this);
        connect(watcher, &QFutureWatcher<QImage>::finished, this, [watcher, target]() {
            QImage cached = watcher->resultAt(0);
            if (cached.isNull()
                || !target->scene()) // belt-and-suspenders: scene 才持有 item 生命周期
                return;
            target->setPixmap(QPixmap::fromImage(cached));
            target->update();
            watcher->deleteLater();
        });

        watcher->setFuture(QtConcurrent::run(
            [](const QString &p) -> QImage {
                return ImageCacheManager::instance().loadIfCached(p);
            },
            path));
    }
}