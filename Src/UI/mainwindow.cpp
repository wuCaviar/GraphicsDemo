#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "AlignLayoutDialog.h"
#include "AutoLayoutDialog.h"
#include "LayoutEngine.h"
#include "AlignmentUtils.h"
#include "AppConfig.h"
#include "ATHCPresets.h"
#include "BezierCurveItem.h"
#include "CanvasItem.h"
#include "Commands.h"
#include "EllipseItem.h"

#include "ImageArrangementDialog.h"
#include "ImageItem.h"
#include "ImageUtils.h"
#include "ProjectFile.h"

#include "LineItem.h"
#include "NewFileDialog.h"
#include "ResizeCanvasDialog.h"
#include "SettingsDialog.h"
#include "PreferencesDialog.h"
#include "RectItem.h"
#include "ResizeHandleItem.h"
#include "RulerBar.h"
#include "TextItem.h"
#include "TiffExportEngine.h"
#include "TaskHistoryPopup.h"
#include "GraphicsItemGroup.h"
#include "FitCanvasDlg.h"
#include "SceneToJsonConverter.h"

#include <algorithm>
#include <memory>
#include <cmath>

#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QCloseEvent>
#include <QPointer>
#include <QDataStream>
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
#include <QPainter>
#include <QPushButton>
#include <QToolButton>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QSlider>
#include <QStyle>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QThreadPool>

static const char *kMimeFormat = "application/x-graphicsdemo-items";
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
    m_undoStack = new QUndoStack(this);

    _initWidget();
    _initPropertyPanel();
    _initRulers();
    _initMenuBar();
    _initToolBar();
    _initConnections();
    _initStatusBar();
    _initProcess();
    _initNetWork();

    setWindowTitle(tr("AT Drawing Tools"));
    resize(1200, 800);

    // 加载窗口状态（工具栏位置、可见性等）
    loadWindowState();
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
    m_pView = new QAtGraphicsView(this);
    m_pView->setUndoStack(m_undoStack);
    m_pView->setEnabled(false); // 没有画板前禁止操作
    setCentralWidget(m_pView);

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
}

void MainWindow::_initRulers()
{
    // 创建刻度尺
    m_hRuler = new RulerBar(RulerBar::Horizontal, this);
    m_vRuler = new RulerBar(RulerBar::Vertical, this);

    m_hRuler->setGraphicsView(m_pView);
    m_vRuler->setGraphicsView(m_pView);

    // 使用布局将刻度尺和视图组合
    // 创建一个角落占位
    auto *cornerWidget = new QWidget(this);
    cornerWidget->setFixedSize(30, 30);

    // 创建中央容器
    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QGridLayout(centralWidget);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    mainLayout->addWidget(cornerWidget, 0, 0);
    mainLayout->addWidget(m_hRuler, 0, 1);
    mainLayout->addWidget(m_vRuler, 1, 0);
    mainLayout->addWidget(m_pView, 1, 1);

    setCentralWidget(centralWidget);
}

void MainWindow::_initMenuBar()
{
    QMenuBar *menu = ui->menubar;

    // ---- 文件 ----
    QMenu *fileMenu = menu->addMenu(tr("&File"));

    QAction *newAct = fileMenu->addAction(QIcon(":/icons/icons/file-new.svg"), tr("&New..."));
    newAct->setShortcut(QKeySequence::New);
    newAct->setToolTip(tr("Create a new canvas"));
    connect(newAct, &QAction::triggered, this, &MainWindow::onNew);

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

    m_undoAction = editMenu->addAction(QIcon(":/icons/icons/edit-undo.svg"), tr("&Undo"));
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setToolTip(tr("Undo the last action"));
    connect(m_undoAction, &QAction::triggered, this, &MainWindow::onUndo);

    m_redoAction = editMenu->addAction(QIcon(":/icons/icons/edit-redo.svg"), tr("&Redo"));
    m_redoAction->setShortcut(QKeySequence::Redo);
    m_redoAction->setToolTip(tr("Redo the last undone action"));
    connect(m_redoAction, &QAction::triggered, this, &MainWindow::onRedo);

    editMenu->addSeparator();

    QAction *cutAct = editMenu->addAction(QIcon(":/icons/icons/edit-cut.svg"), tr("Cu&t"));
    cutAct->setShortcut(QKeySequence::Cut);
    cutAct->setToolTip(tr("Cut the selected items to clipboard"));
    connect(cutAct, &QAction::triggered, this, &MainWindow::onCut);

    QAction *copyAct = editMenu->addAction(QIcon(":/icons/icons/edit-copy.svg"), tr("&Copy"));
    copyAct->setShortcut(QKeySequence::Copy);
    copyAct->setToolTip(tr("Copy the selected items to clipboard"));
    connect(copyAct, &QAction::triggered, this, &MainWindow::onCopy);

    QAction *pasteAct = editMenu->addAction(QIcon(":/icons/icons/edit-paste.svg"), tr("&Paste"));
    pasteAct->setShortcut(QKeySequence::Paste);
    pasteAct->setToolTip(tr("Paste items from clipboard"));
    connect(pasteAct, &QAction::triggered, this, &MainWindow::onPaste);

    editMenu->addSeparator();

    QAction *deleteAct = editMenu->addAction(QIcon(":/icons/icons/edit-delete.svg"), tr("&Delete"));
    deleteAct->setShortcut(QKeySequence::Delete);
    deleteAct->setToolTip(tr("Delete the selected items"));
    connect(deleteAct, &QAction::triggered, this, &MainWindow::onDelete);

    QAction *selectAllAct =
        editMenu->addAction(QIcon(":/icons/icons/edit-select-all.svg"), tr("Select &All"));
    selectAllAct->setShortcut(QKeySequence::SelectAll);
    selectAllAct->setToolTip(tr("Select all items on the canvas"));
    connect(selectAllAct, &QAction::triggered, this, &MainWindow::onSelectAll);

    // ---- 排列 ----
    QMenu *arrMenu = menu->addMenu(tr("&Arrange"));
    arrMenu
        ->addAction(QIcon(":/icons/icons/bring-front.svg"), tr("Bring Forward"), this,
                    &MainWindow::onBringToFront)
        ->setToolTip(tr("Bring selected items forward one step"));
    arrMenu
        ->addAction(QIcon(":/icons/icons/send-back.svg"), tr("Send Backward"), this,
                    &MainWindow::onSendToBack)
        ->setToolTip(tr("Send selected items backward one step"));
    arrMenu->addSeparator();
    QAction *groupAct = arrMenu->addAction(QIcon(":/icons/icons/group.svg"), tr("&Group"));
    groupAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    groupAct->setToolTip(tr("Group selected items together"));
    connect(groupAct, &QAction::triggered, this, &MainWindow::onGroup);
    QAction *ungroupAct = arrMenu->addAction(QIcon(":/icons/icons/ungroup.svg"), tr("&Ungroup"));
    ungroupAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_G));
    ungroupAct->setToolTip(tr("Ungroup selected items"));
    connect(ungroupAct, &QAction::triggered, this, &MainWindow::onUngroup);
    arrMenu->addSeparator();
    arrMenu->addAction(tr("Align && Layout..."), this, &MainWindow::onAlignLayoutDialog)
        ->setToolTip(tr("Open the Align & Layout dialog"));
    arrMenu->addSeparator();
    QAction *fitCanvasAct =
        arrMenu->addAction(tr("Fit Canvas to Selection"), this, &MainWindow::onFitCanvasToItems);
    fitCanvasAct->setToolTip(tr("Resize the canvas to fit the selected items"));
    arrMenu->addSeparator();
    QMenu *rotateMenu = arrMenu->addMenu(tr("Rotate"));
    rotateMenu
        ->addAction(QIcon(":/icons/icons/rotate-cw.svg"), tr("90\u00b0 Clockwise"), this,
                    [this]() { rotateSelectedItems(90.0); })
        ->setToolTip(tr("Rotate selected items 90 degrees clockwise"));
    rotateMenu
        ->addAction(QIcon(":/icons/icons/rotate-ccw.svg"), tr("90\u00b0 Counter-clockwise"), this,
                    [this]() { rotateSelectedItems(-90.0); })
        ->setToolTip(tr("Rotate selected items 90 degrees counter-clockwise"));
    rotateMenu->addAction(tr("180\u00b0"), this, [this]() { rotateSelectedItems(180.0); })
        ->setToolTip(tr("Rotate selected items 180 degrees"));

    // ---- 设置 ----
    QMenu *settingsMenu = menu->addMenu(tr("&Settings"));
    settingsMenu->addAction(tr("&RIP Settings..."), this, &MainWindow::onSettings)
        ->setToolTip(tr("Configure RIP settings"));
    settingsMenu->addSeparator();
    settingsMenu->addAction(tr("&Preferences..."), this, &MainWindow::onPreferences)
        ->setToolTip(tr("Open application preferences"));

    // ---- 视图 ----
    QMenu *viewMenu = menu->addMenu(tr("&View"));
    viewMenu->addAction(m_pPropertyPanel->toggleViewAction());
    viewMenu->addAction(m_alignLayoutDlg->toggleViewAction());
    viewMenu->addSeparator();

    // ---- 帮助 ----
    QMenu *helpMenu = menu->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&About..."), this, &MainWindow::onAbout)
        ->setToolTip(tr("About this application"));

    // 网格显示/隐藏
    m_gridAction = viewMenu->addAction(tr("Show Grid"));
    m_gridAction->setCheckable(true);
    m_gridAction->setChecked(true);
    m_gridAction->setToolTip(tr("Show or hide the grid"));
    connect(m_gridAction, &QAction::toggled, this,
            [this](bool checked) { m_pView->setGridVisible(checked); });

    _initThemeMenu(viewMenu);

    viewMenu->addSeparator();
    // 缩放适配
    QAction *fitAct = new QAction(QIcon(":/icons/icons/view-fit.svg"), tr("Fit to Canvas"), this);
    fitAct->setToolTip(tr("Fit the view to the canvas"));
    connect(fitAct, &QAction::triggered, this, [this]() { m_pView->fitToCanvas(); });
    viewMenu->addAction(fitAct);

    QAction *resetZoomAct =
        new QAction(QIcon(":/icons/icons/view-zoom-reset.svg"), tr("Reset Zoom (0)"), this);
    resetZoomAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    resetZoomAct->setToolTip(tr("Reset zoom to 100%"));
    connect(resetZoomAct, &QAction::triggered, this, [this]() { m_pView->setZoomLevel(1.0); });
    viewMenu->addAction(resetZoomAct);

    _updateUndoRedoActions();
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

    fileEditBar->addAction(m_undoAction);
    fileEditBar->addAction(m_redoAction);

    fileEditBar->addSeparator();

    // Cut 按钮
    QAction *cutAct = new QAction(QIcon(":/icons/icons/edit-cut.svg"), tr("Cut"), this);
    cutAct->setShortcut(QKeySequence::Cut);
    cutAct->setToolTip(tr("Cut the selected items to clipboard"));
    connect(cutAct, &QAction::triggered, this, &MainWindow::onCut);
    fileEditBar->addAction(cutAct);

    // Copy 按钮
    QAction *copyAct = new QAction(QIcon(":/icons/icons/edit-copy.svg"), tr("Copy"), this);
    copyAct->setShortcut(QKeySequence::Copy);
    copyAct->setToolTip(tr("Copy the selected items to clipboard"));
    connect(copyAct, &QAction::triggered, this, &MainWindow::onCopy);
    fileEditBar->addAction(copyAct);

    // Paste 按钮
    QAction *pasteAct = new QAction(QIcon(":/icons/icons/edit-paste.svg"), tr("Paste"), this);
    pasteAct->setShortcut(QKeySequence::Paste);
    pasteAct->setToolTip(tr("Paste items from clipboard"));
    connect(pasteAct, &QAction::triggered, this, &MainWindow::onPaste);
    fileEditBar->addAction(pasteAct);

    // 绘图工具栏 - 停靠在左侧
    QToolBar *drawBar = new QToolBar(tr("Drawing Tools"), this);
    drawBar->setObjectName("DrawingToolBar");
    drawBar->setMovable(true);
    drawBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    drawBar->setIconSize(QSize(20, 20));
    addToolBar(Qt::LeftToolBarArea, drawBar);

    auto *actionGroup = new QActionGroup(this);
    actionGroup->setExclusive(true);

    auto addToolAction = [&](const QString &iconPath, const QString &text, Tool tool,
                             const QString &shortcut = { }) {
        QAction *act = drawBar->addAction(QIcon(iconPath), text);
        act->setCheckable(true);
        act->setToolTip(text);
        actionGroup->addAction(act);
        if (!shortcut.isEmpty())
            act->setShortcut(QKeySequence(shortcut));
        connect(act, &QAction::triggered, this, [this, tool]() { onToolTriggered(tool); });
        m_toolActions[tool] = act;
        return act;
    };

    auto *selectAct =
        addToolAction(":/icons/icons/tool-select.svg", tr("Select (V)"), Tool::Select, "V");
    selectAct->setChecked(true);
    addToolAction(":/icons/icons/tool-rect.svg", tr("Rectangle (R)"), Tool::Rect, "R");
    addToolAction(":/icons/icons/tool-ellipse.svg", tr("Ellipse (E)"), Tool::Ellipse, "E");
    addToolAction(":/icons/icons/tool-line.svg", tr("Line (L)"), Tool::Line, "L");
    addToolAction(":/icons/icons/tool-curve.svg", tr("Curve (C)"), Tool::BezierCurve, "C");
    addToolAction(":/icons/icons/tool-freehand.svg", tr("Freehand (F)"), Tool::Freehand, "F");
    addToolAction(":/icons/icons/tool-text.svg", tr("Text (T)"), Tool::Text, "T");

    // 对齐工具栏
    QToolBar *alignToolBar = new QToolBar(tr("Align"), this);
    alignToolBar->setObjectName("AlignToolBar");
    alignToolBar->setMovable(false);
    alignToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    alignToolBar->setIconSize(QSize(20, 20));
    addToolBar(Qt::TopToolBarArea, alignToolBar);

    QAction *alignLayoutAct =
        alignToolBar->addAction(QIcon(":/icons/icons/align-layout.svg"), tr("Align && Layout..."));
    alignLayoutAct->setToolTip(tr("Open Align & Layout dialog"));
    connect(alignLayoutAct, &QAction::triggered, this, &MainWindow::onAlignLayoutDialog);

    alignToolBar->addSeparator();

    // 成组/解组
    QAction *groupAct = alignToolBar->addAction(QIcon(":/icons/icons/group.svg"), tr("Group"));
    groupAct->setToolTip(tr("Group selected items (Ctrl+G)"));
    groupAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    connect(groupAct, &QAction::triggered, this, &MainWindow::onGroup);

    QAction *ungroupAct =
        alignToolBar->addAction(QIcon(":/icons/icons/ungroup.svg"), tr("Ungroup"));
    ungroupAct->setToolTip(tr("Ungroup selected items (Ctrl+Shift+G)"));
    ungroupAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_G));
    connect(ungroupAct, &QAction::triggered, this, &MainWindow::onUngroup);

    alignToolBar->addSeparator();

    // 顺时针旋转 90°
    QAction *rotateCWAct =
        alignToolBar->addAction(QIcon(":/icons/icons/rotate-cw.svg"), tr("Rotate 90\u00b0 CW"));
    rotateCWAct->setToolTip(tr("Rotate 90\u00b0 clockwise"));
    connect(rotateCWAct, &QAction::triggered, this, [this]() { rotateSelectedItems(90.0); });

    // 逆时针旋转 90°
    QAction *rotateCCWAct =
        alignToolBar->addAction(QIcon(":/icons/icons/rotate-ccw.svg"), tr("Rotate 90\u00b0 CCW"));
    rotateCCWAct->setToolTip(tr("Rotate 90\u00b0 counter-clockwise"));
    connect(rotateCCWAct, &QAction::triggered, this, [this]() { rotateSelectedItems(-90.0); });

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
    addDockWidget(Qt::RightDockWidgetArea, m_pPropertyPanel);

    m_alignLayoutDlg = new AlignLayoutDialog(m_pView->scene(), m_undoStack, this);
    m_alignLayoutDlg->setObjectName("AlignLayoutDock");
    m_alignLayoutDlg->setMinimumWidth(300);
    splitDockWidget(m_pPropertyPanel, m_alignLayoutDlg, Qt::Vertical);
    m_alignLayoutDlg->hide();
}

void MainWindow::_initConnections()
{
    connect(m_pView, &QAtGraphicsView::selectionChanged, this, &MainWindow::onSelectionChanged);
    connect(m_pView, &QAtGraphicsView::itemAdded, this, &MainWindow::onItemAdded);

    // 右键菜单 → 复用菜单栏的 Bring Forward / Send Backward
    connect(m_pView, &QAtGraphicsView::bringToFrontRequested, this, &MainWindow::onBringToFront);
    connect(m_pView, &QAtGraphicsView::sendToBackRequested, this, &MainWindow::onSendToBack);
    connect(m_pView, &QAtGraphicsView::groupRequested, this, &MainWindow::onGroup);
    connect(m_pView, &QAtGraphicsView::ungroupRequested, this, &MainWindow::onUngroup);
    connect(m_pView, &QAtGraphicsView::fitCanvasToItemsRequested, this,
            &MainWindow::onFitCanvasToItems);

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

    connect(m_undoStack, &QUndoStack::canUndoChanged, this, [this]() { _updateUndoRedoActions(); });
    connect(m_undoStack, &QUndoStack::canRedoChanged, this, [this]() { _updateUndoRedoActions(); });

    // undo/redo 后更新 ResizeHandleItem 位置（而非重建，避免选中框闪烁）
    // 同时标记工程为已修改
    connect(m_undoStack, &QUndoStack::indexChanged, this, [this]() {
        m_pView->refreshResizeHandle();
        _updateCanvasLabel();
        m_projectModified = true;
        // 刷新属性面板（如 Z 值等属性可能已变更）
        if (m_pPropertyPanel && m_pPropertyPanel->currentItem())
            m_pPropertyPanel->setItem(m_pPropertyPanel->currentItem());
    });

    // 视图滚动/缩放时更新刻度尺
    connect(m_pView->horizontalScrollBar(), &QScrollBar::valueChanged, m_hRuler,
            &RulerBar::updateRuler);
    connect(m_pView->verticalScrollBar(), &QScrollBar::valueChanged, m_vRuler,
            &RulerBar::updateRuler);

    // 状态栏：鼠标位置 & 缩放变化 & 刻度尺鼠标位置同步
    connect(m_pView, &QAtGraphicsView::mousePositionChanged, this, [this](const QPointF &pos) {
        _updatePosLabel(pos);
        // 同步鼠标位置到刻度尺
        m_hRuler->setMousePosition(pos);
        m_vRuler->setMousePosition(pos);
    });
    connect(m_pView, &QAtGraphicsView::zoomChanged, this, [this](qreal level) {
        int pct = qRound(level * 100);
        m_zoomLabel->setText(tr("%1%").arg(pct));
        // 更新滑块（避免信号循环）
        m_zoomSlider->blockSignals(true);
        m_zoomSlider->setValue(pct);
        m_zoomSlider->blockSignals(false);

        // 同步刻度尺缩放
        m_hRuler->updateRuler();
        m_vRuler->updateRuler();
    });

    // TextItem 编辑完成时更新属性面板
    connect(m_pView->scene(), &QGraphicsScene::focusItemChanged, this,
            [this](QGraphicsItem *newFocus, QGraphicsItem *oldFocus, Qt::FocusReason) {
                Q_UNUSED(oldFocus);
                Q_UNUSED(newFocus);
                // 当焦点离开 TextItem 时更新属性面板
                if (oldFocus && qgraphicsitem_cast<TextItem *>(oldFocus)) {
                    auto *ti = qgraphicsitem_cast<TextItem *>(oldFocus);
                    if (ti && ti->isSelected())
                        m_pPropertyPanel->setItem(ti);
                }
            });

    // 工具变更时更新状态栏（含空格临时切换手型工具）
    connect(m_pView, &QAtGraphicsView::toolChanged, this, [this](Tool tool) {
        m_currentTool = tool;
        _updateToolLabel();
        // 同步工具栏按钮状态
        for (auto it = m_toolActions.begin(); it != m_toolActions.end(); ++it)
            it.value()->setChecked(it.key() == tool);
    });
}

void MainWindow::_initStatusBar()
{
    auto *bar = statusBar();

    // 鼠标坐标
    m_posLabel = new QLabel(tr("X: 0.0 mm  Y: 0.0 mm"));
    m_posLabel->setMinimumWidth(220);

    m_pProgressMgr = new ProgressManager(this);

    // 缩放标签
    m_zoomLabel = new QLabel(tr("100%"));
    m_zoomLabel->setMinimumWidth(60);
    m_zoomLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // 缩放滑块
    m_zoomSlider = new QSlider(Qt::Horizontal);
    m_zoomSlider->setRange(1, 5000);
    m_zoomSlider->setValue(100);
    m_zoomSlider->setFixedWidth(120);
    m_zoomSlider->setTickPosition(QSlider::TicksBelow);
    m_zoomSlider->setTickInterval(50);
    m_zoomSlider->setToolTip(tr("Adjust zoom level"));
    connect(m_zoomSlider, &QSlider::valueChanged, this, [this](int value) {
        qreal targetZoom = value / 100.0;
        m_pView->setZoomLevel(targetZoom);
    });

    // 画布尺寸修改按钮
    m_resizeCanvasBtn = new QToolButton;
    m_resizeCanvasBtn->setIcon(QIcon(":/icons/icons/canvas-resize.svg"));
    m_resizeCanvasBtn->setIconSize(QSize(16, 16));
    m_resizeCanvasBtn->setAutoRaise(true);
    m_resizeCanvasBtn->setToolTip(tr("Resize canvas"));
    m_resizeCanvasBtn->setVisible(false);
    connect(m_resizeCanvasBtn, &QToolButton::clicked, this, &MainWindow::onResizeCanvas);

    // 画布尺寸
    m_canvasLabel = new QLabel;
    m_canvasLabel->setMinimumWidth(230);
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
    bar->addPermanentWidget(m_zoomLabel);
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

void MainWindow::_updateUndoRedoActions()
{
    if (m_undoAction)
        m_undoAction->setEnabled(m_undoStack->canUndo());
    if (m_redoAction)
        m_redoAction->setEnabled(m_undoStack->canRedo());
}

void MainWindow::_updatePosLabel(const QPointF &scenePos)
{
    m_lastScenePos = scenePos;
    qreal ppi = m_pView->canvasItem() ? m_pView->canvasItem()->ppi() : 300.0;
    qreal kPxToMm = 25.4 / ppi;
    qreal xmm = scenePos.x() * kPxToMm;
    qreal ymm = scenePos.y() * kPxToMm;
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
    qreal kPxToMm = 25.4 / ppi;

    if (canvas->isDpiLocked()) {
        m_canvasLabel->setText(tr("Canvas: %1 \u00d7 %2 mm \u00b7 %3 DPI")
                                   .arg(sz.width() * kPxToMm, 0, 'f', 1)
                                   .arg(sz.height() * kPxToMm, 0, 'f', 1)
                                   .arg(canvas->canvasDpiX()));
    } else {
        m_canvasLabel->setText(tr("Canvas: %1 \u00d7 %2 mm \u00b7 DPI: --")
                                   .arg(sz.width() * kPxToMm, 0, 'f', 1)
                                   .arg(sz.height() * kPxToMm, 0, 'f', 1));
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
    case Tool::Image:
        toolName = tr("Image");
        break;
    }

    if (toolName.isEmpty())
        toolName = tr("Unknown");

    m_toolLabel->setText(tr("Tool: %1").arg(toolName));
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    // 更新刻度尺
    if (m_hRuler)
        m_hRuler->updateRuler();
    if (m_vRuler)
        m_vRuler->updateRuler();
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
    if (!m_projectModified || !m_pView->canvasItem())
        return true;

    QMessageBox::StandardButton btn =
        QMessageBox::question(this, tr("Unsaved Changes"),
                              tr("The current project has unsaved changes.\n"
                                 "Do you want to save them?"),
                              QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (btn == QMessageBox::Cancel)
        return false;
    if (btn == QMessageBox::Yes) {
        // 如果已有保存路径，直接保存；否则弹出另存为对话框
        if (!m_currentProjectPath.isEmpty()) {
            CanvasItem *canvas = m_pView->canvasItem();
            if (!canvas)
                return false;
            ProjectFile::ProjectInfo info;
            info.name = QFileInfo(m_currentProjectPath).completeBaseName();
            info.version = QStringLiteral("1.0.0");
            info.author = QStringLiteral("Caviar");
            ProjectFile::CanvasInfo canvasInfo;
            canvasInfo.width = canvas->canvasSize().width();
            canvasInfo.height = canvas->canvasSize().height();
            canvasInfo.dpi = canvas->ppi();
            auto items = ::filterSelectableItems(m_pView->scene()->items());
            ProjectFile pf;
            if (!pf.save(m_currentProjectPath, info, canvasInfo, items)) {
                QMessageBox::warning(this, tr("Save Project"),
                                     tr("Failed to save:\n%1").arg(pf.lastError()));
                return false;
            }
            m_projectModified = false;
        } else {
            onSaveProject();
            if (m_projectModified)
                return false; // 用户取消了保存
        }
    }
    return true;
}

void MainWindow::onNew()
{
    if (!_maybeSaveProject())
        return;

    NewFileDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    setEnabled(false); // 阻止用户在重置过程中操作界面

    // 获取 mm 尺寸，使用默认 PPI 300 转换为场景像素
    QSizeF sizeMM = dlg.selectedSizeMM();
    static constexpr qreal kDefaultPpi = 300.0;
    qreal mmToPx = kDefaultPpi / 25.4;
    QSizeF canvasSize(std::ceil(sizeMM.width() * mmToPx), std::ceil(sizeMM.height() * mmToPx));

    // 先清空 undo 栈，避免命令引用即将被删除的图元
    m_undoStack->clear();

    // 重置工程状态
    m_currentProjectPath.clear();
    m_projectModified = false;

    // 清空属性面板引用
    m_pPropertyPanel->setItem(nullptr);

    // 安全清空场景并重建画布
    m_pView->resetCanvas(canvasSize);
    m_pView->setEnabled(true); // 画板就绪，允许操作
    m_resizeCanvasBtn->setVisible(true);

    // 新画布 DPI 未确定，使用默认显示 PPI
    if (auto *canvas = m_pView->canvasItem()) {
        canvas->setPpi(kDefaultPpi);
        canvas->setCanvasDpi(0, 0); // DPI 未确定
    }

    // 同步刻度尺 PPI
    m_hRuler->setPpi(kDefaultPpi);
    m_vRuler->setPpi(kDefaultPpi);
    m_pPropertyPanel->setDisplayPpi(kDefaultPpi);

    m_pProgressMgr->resetAll();

    // PPI 变化后刷新刻度尺和状态栏
    m_hRuler->updateRuler();
    m_vRuler->updateRuler();
    _updateCanvasLabel();
    _updatePosLabel(m_lastScenePos);

    QApplication::restoreOverrideCursor();
    setEnabled(true);
}

void MainWindow::onOpenProject()
{
    if (!_maybeSaveProject())
        return;

    QString path = QFileDialog::getOpenFileName(this, tr("Open Project"), QString(),
                                                tr("AT Project Files (*.atp);;All Files (*)"));
    if (path.isEmpty())
        return;

    // ---- 阶段 1：主线程解析 XML（快速） ----
    ProjectFile pf;
    ProjectFile::ProjectInfo info;
    ProjectFile::CanvasInfo canvasInfo;
    QList<DeserialTask> tasks;

    if (!pf.parseForDeserialize(path, info, canvasInfo, tasks)) {
        QMessageBox::warning(this, tr("Open Project"),
                             tr("Failed to open project:\n%1").arg(pf.lastError()));
        return;
    }

    if (tasks.isEmpty()) {
        // 空工程 — 直接重建画布
        m_undoStack->clear();
        m_pPropertyPanel->setItem(nullptr);
        m_pView->resetCanvas(QSizeF(canvasInfo.width, canvasInfo.height));
        qreal ppi = canvasInfo.dpi > 0 ? canvasInfo.dpi : 300.0;
        if (auto *canvas = m_pView->canvasItem()) {
            canvas->setPpi(ppi);
            if (canvasInfo.dpi > 0) {
                canvas->setCanvasDpi(canvasInfo.dpi, canvasInfo.dpi);
                canvas->lockDpi();
            }
        }
        m_pView->setEnabled(true);
        m_resizeCanvasBtn->setVisible(true);
        m_hRuler->setPpi(ppi);
        m_vRuler->setPpi(ppi);
        m_pPropertyPanel->setDisplayPpi(ppi);
        m_hRuler->updateRuler();
        m_vRuler->updateRuler();
        _updateCanvasLabel();
        _updatePosLabel(m_lastScenePos);
        m_currentProjectPath = path;
        m_projectModified = false;
        m_pProgressMgr->resetAll();
        setWindowTitle(tr("AT Drawing Tools - %1").arg(info.name));
        return;
    }

    // ---- 阶段 2：并发 Base64 解码（纯 CPU，不创建 QGraphicsItem） ----
    const QString taskId = m_pProgressMgr->startTask(tr("Open Project"), tasks.size());

    // 禁用视图，防止用户在加载期间操作画布
    m_pView->setEnabled(false);

    auto *watcher = new QFutureWatcher<DeserializedItem>(this);

    connect(watcher, &QFutureWatcher<DeserializedItem>::progressValueChanged, this,
            [this, taskId](int value) { m_pProgressMgr->updateTask(taskId, value); });

    connect(watcher, &QFutureWatcher<DeserializedItem>::finished, this,
            [this, watcher, taskId, info, canvasInfo, path]() {
                m_pProgressMgr->finishTask(taskId);

                // 在主线程创建 QGraphicsItem（安全的做法）
                QList<QGraphicsItem *> loadedItems;
                auto future = watcher->future();
                for (int i = 0; i < future.resultCount(); ++i) {
                    QGraphicsItem *item = createItemFromDeserialized(future.resultAt(i));
                    if (item)
                        loadedItems.append(item);
                }

                // 清空当前画布并重建
                m_undoStack->clear();
                m_pPropertyPanel->setItem(nullptr);
                m_pView->resetCanvas(QSizeF(canvasInfo.width, canvasInfo.height));

                qreal ppi = canvasInfo.dpi > 0 ? canvasInfo.dpi : 300.0;
                if (auto *canvas = m_pView->canvasItem()) {
                    canvas->setPpi(ppi);
                    if (canvasInfo.dpi > 0) {
                        canvas->setCanvasDpi(canvasInfo.dpi, canvasInfo.dpi);
                        canvas->lockDpi();
                    }
                }

                for (auto *item : loadedItems)
                    m_pView->scene()->addItem(item);

                m_pView->setEnabled(true);
                m_resizeCanvasBtn->setVisible(true);

                m_hRuler->setPpi(ppi);
                m_vRuler->setPpi(ppi);
                m_pPropertyPanel->setDisplayPpi(ppi);
                m_hRuler->updateRuler();
                m_vRuler->updateRuler();
                _updateCanvasLabel();
                _updatePosLabel(m_lastScenePos);

                m_currentProjectPath = path;
                m_projectModified = false;
                setWindowTitle(tr("AT Drawing Tools - %1").arg(info.name));

                watcher->deleteLater();
            });

    auto future = QtConcurrent::mapped(tasks, deserializeItemWorker);
    watcher->setFuture(future);
}

void MainWindow::onSaveProject()
{
    CanvasItem *canvas = m_pView->canvasItem();
    if (!canvas) {
        QMessageBox::warning(this, tr("Save Project"),
                             tr("No canvas to save. Create a new canvas first."));
        return;
    }

    QString path = QFileDialog::getSaveFileName(this, tr("Save Project"), QString(),
                                                tr("AT Project Files (*.atp);;All Files (*)"));
    if (path.isEmpty())
        return;

    // 确保后缀为 .atp
    if (!path.endsWith(QLatin1String(".atp"), Qt::CaseInsensitive))
        path.append(QLatin1String(".atp"));

    ProjectFile::ProjectInfo info;
    QFileInfo fi(path);
    info.name = fi.completeBaseName();
    info.version = QStringLiteral("1.0.0");
    info.author = QStringLiteral("ATHC");

    ProjectFile::CanvasInfo canvasInfo;
    canvasInfo.width = canvas->canvasSize().width();
    canvasInfo.height = canvas->canvasSize().height();
    // 存储画布实际 DPI：锁定状态用 canvasDpiX，未锁定用 0
    canvasInfo.dpi = canvas->isDpiLocked() ? canvas->canvasDpiX() : 0.0;

    auto items = ::filterSelectableItems(m_pView->scene()->items());

    // ---- 在主线程采集快照（线程安全），然后并发 Base64 编码 ----
    QList<SerializeInput> inputs;
    inputs.reserve(items.size());
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

            // 采集 CMYK 数据（写入 XML 属性，用于 TIFF 导出精确颜色）
            if (igi->hasPenCmyk()) {
                input.cmyk.hasPen = true;
                igi->penCmyk(input.cmyk.penC, input.cmyk.penM, input.cmyk.penY, input.cmyk.penK);
            }
            if (igi->hasBrushCmyk()) {
                input.cmyk.hasBrush = true;
                igi->brushCmyk(input.cmyk.brushC, input.cmyk.brushM, input.cmyk.brushY,
                               input.cmyk.brushK);
            }
            input.cmyk.gradient = igi->gradientStopCmykMap();
        }
        inputs.append(input);
    }

    // ---- 并发序列化（禁用视图防止用户在序列化期间修改图元） ----
    const QString taskId = m_pProgressMgr->startTask(tr("Save Project"), inputs.size());

    m_pView->setEnabled(false);

    auto *watcher = new QFutureWatcher<SerializedItem>(this);

    connect(watcher, &QFutureWatcher<SerializedItem>::progressValueChanged, this,
            [this, taskId](int value) { m_pProgressMgr->updateTask(taskId, value); });

    connect(watcher, &QFutureWatcher<SerializedItem>::finished, this,
            [this, watcher, taskId, path, info, canvasInfo]() {
                m_pProgressMgr->finishTask(taskId);

                // 收集序列化结果
                QList<SerializedItem> results;
                auto future = watcher->future();
                for (int i = 0; i < future.resultCount(); ++i)
                    results.append(future.resultAt(i));

                // 主线程组装 XML 并写入文件
                ProjectFile pf;
                if (!pf.saveFromSerialized(path, info, canvasInfo, results)) {
                    QMessageBox::warning(this, tr("Save Project"),
                                         tr("Failed to save project:\n%1").arg(pf.lastError()));
                    m_pView->setEnabled(true);
                    watcher->deleteLater();
                    return;
                }

                m_currentProjectPath = path;
                m_projectModified = false;
                setWindowTitle(tr("AT Drawing Tools - %1").arg(info.name));

                m_pView->setEnabled(true);
                watcher->deleteLater();
            });

    auto future = QtConcurrent::mapped(inputs, serializeItemWorker);
    watcher->setFuture(future);
}

// ============================================================
// DPI 管理
// ============================================================

bool MainWindow::_tryLockCanvasDpi(int dpiX, int dpiY)
{
    auto *canvas = m_pView->canvasItem();
    if (!canvas)
        return true; // 无画布，允许导入

    if (!canvas->isDpiLocked()) {
        // DPI 未锁定：由当前图片确定并锁定
        // 先算出当前物理 mm（在修改 DPI 前）
        QSizeF oldSize = canvas->canvasSize();
        qreal oldPpi = canvas->ppi();
        qreal mmW = oldSize.width() / oldPpi * 25.4;
        qreal mmH = oldSize.height() / oldPpi * 25.4;

        canvas->setCanvasDpi(dpiX, dpiY);
        canvas->setPpi(static_cast<qreal>(dpiX));
        canvas->lockDpi();

        // 从物理 mm 反算新像素尺寸，保持物理毫米尺寸不变（ceil 保证整数边界）
        qreal mmToPx = static_cast<qreal>(dpiX) / 25.4;
        QSizeF newSize(std::ceil(mmW * mmToPx), std::ceil(mmH * mmToPx));
        m_pView->setCanvasSize(newSize);

        // 同步缩放非图片图元的像素尺寸和位置（factor = newPpi/oldPpi）
        qreal factor = static_cast<qreal>(dpiX) / oldPpi;
        if (!qFuzzyCompare(factor, 1.0)) {
            const auto selectable = ::filterSelectableItems(m_pView->scene()->items());
            for (auto *item : selectable) {
                if (dynamic_cast<ImageItem *>(item) || item->parentItem())
                    continue;
                item->setPos(item->pos() * factor);
                item->setTransform(QTransform::fromScale(factor, factor) * item->transform());
            }
        }

        m_hRuler->setPpi(static_cast<qreal>(dpiX));
        m_vRuler->setPpi(static_cast<qreal>(dpiX));
        m_pPropertyPanel->setDisplayPpi(static_cast<qreal>(dpiX));
        m_hRuler->updateRuler();
        m_vRuler->updateRuler();
        _updateCanvasLabel();
        _updatePosLabel(m_lastScenePos);
        return true;
    }

    // DPI 已锁定：检查是否匹配
    if (canvas->canvasDpiX() == dpiX && canvas->canvasDpiY() == dpiY)
        return true;

    QMessageBox::warning(this, tr("DPI Mismatch"),
                         tr("The image DPI (%1×%2) does not match the canvas DPI (%3×%4).\n"
                            "Please use images with matching DPI.")
                             .arg(dpiX)
                             .arg(dpiY)
                             .arg(canvas->canvasDpiX())
                             .arg(canvas->canvasDpiY()));
    return false;
}

void MainWindow::_unlockCanvasDpiIfNoImages()
{
    auto *canvas = m_pView->canvasItem();
    if (!canvas || !canvas->isDpiLocked())
        return;

    // 检查画布上是否还有 ImageItem
    const auto allItems = m_pView->scene()->items();
    bool hasImage = false;
    for (auto *item : allItems) {
        if (dynamic_cast<ImageItem *>(item)) {
            hasImage = true;
            break;
        }
    }

    if (!hasImage) {
        canvas->unlockDpi();
        _updateCanvasLabel();
    }
}

void MainWindow::onImportImage()
{
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

    if (!m_pView->canvasItem())
        return;

    if (paths.size() == 1)
        importSingleImage(paths);
    else
        importMultipleImages(paths);
}

void MainWindow::importSingleImage(const QStringList &paths)
{
    CanvasItem *canvas = m_pView->canvasItem();

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
            [this, watcher, importedItems, runningY](int index) {
                auto result = watcher->resultAt(index);
                if (result.isValid()) {
                    // DPI 检查：画布 DPI 已锁定时必须匹配
                    if (result.dpiX > 0 && result.dpiY > 0) {
                        if (!_tryLockCanvasDpi(result.dpiX, result.dpiY)) {
                            qWarning() << "Skipped (DPI mismatch):" << result.path;
                            return; // DPI 不匹配，拒绝导入
                        }
                    }

                    auto *item = new ImageItem(result.pixmap, result.size);
                    item->setItemPen(QPen(Qt::NoPen));
                    item->setFilePath(result.path);
                    item->setOriginalSize(result.size);

                    // 设置图片 DPI 到 ImageItem
                    if (result.dpiX > 0 && result.dpiY > 0)
                        item->setDpi(result.dpiX, result.dpiY);

                    item->setPos(0, *runningY);
                    *runningY += result.size.height() + 10;

                    m_undoStack->push(new AddItemCommand(m_pView->scene(), item));
                    importedItems->append(item);
                } else {
                    qWarning() << "Import failed:" << result.path << result.errorMessage;
                }
            });

    connect(watcher, &QFutureWatcher<ImageUtils::ImportWorkerResult>::finished, this,
            [this, watcher, taskId, importedItems, fitType, fitVal]() {
                m_pProgressMgr->finishTask(taskId);
                watcher->deleteLater();

                // 导入完成后刷新状态栏（DPI 锁定可能已变更）
                _updateCanvasLabel();
                _updatePosLabel(m_lastScenePos);

                if (fitType != fctNone && !importedItems->isEmpty()) {
                    CanvasItem *canvas = m_pView->canvasItem();
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
                            m_undoStack->beginMacro(tr("Fit Canvas on Import"));

                            if (offsetX != 0 || offsetY != 0) {
                                QPointF delta(offsetX, offsetY);
                                QList<QPointF> oldPositions, newPositions;
                                for (auto *item : *importedItems) {
                                    oldPositions << item->pos();
                                    newPositions << item->pos() + delta;
                                    item->setPos(item->pos() + delta);
                                }
                                m_undoStack->push(new MoveItemsCommand(
                                    QList<QGraphicsItem *>(importedItems->begin(),
                                                           importedItems->end()),
                                    oldPositions, newPositions, m_pView->scene()));
                            }

                            m_undoStack->push(new CanvasResizeCommand(canvas, oldSize, newSize,
                                                                      m_pView->scene()));
                            m_undoStack->endMacro();
                            _updateCanvasLabel();
                            m_pView->fitToCanvas();
                        }
                    }
                }

                delete importedItems;
                qDebug() << QTime::currentTime().toString("HH:mm:ss.zzz") << "Finished import";
            });

    auto future = QtConcurrent::mapped(paths, ImageUtils::runImportWorker);
    watcher->setFuture(future);
    qDebug() << QTime::currentTime().toString("HH:mm:ss.zzz");
}

void MainWindow::importMultipleImages(const QStringList &paths)
{
    CanvasItem *canvas = m_pView->canvasItem();

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
    // 多图导入 DPI 共享状态：记录首个有效 DPI，后续图片必须匹配
    auto commonDpi = std::make_shared<QPair<int, int>>(0, 0);
    auto runningCoord = std::make_shared<qreal>(0);

    connect(
        watcher, &QFutureWatcher<ImageUtils::ImportWorkerResult>::progressValueChanged, this,
        [this, taskId](int progressValue) { m_pProgressMgr->updateTask(taskId, progressValue); });

    connect(watcher, &QFutureWatcher<ImageUtils::ImportWorkerResult>::resultReadyAt, this,
            [this, watcher, importedItems, runningCoord, arr, commonDpi](int index) {
                auto result = watcher->resultAt(index);
                if (result.isValid()) {
                    // 多图 DPI 筛选：以首个有效 DPI 为准，筛选相同 DPI 的图片
                    if (result.dpiX > 0 && result.dpiY > 0) {
                        if (commonDpi->first == 0 && commonDpi->second == 0) {
                            // 首个有效 DPI：尝试锁定画布，失败则跳过
                            if (!_tryLockCanvasDpi(result.dpiX, result.dpiY)) {
                                qWarning() << "Skipped (DPI rejected):" << result.path;
                                return;
                            }
                            *commonDpi = { result.dpiX, result.dpiY };
                        } else if (result.dpiX != commonDpi->first
                                   || result.dpiY != commonDpi->second) {
                            // DPI 不匹配，跳过此图片
                            qWarning()
                                << "Skipped (DPI mismatch):" << result.path << "DPI:" << result.dpiX
                                << "x" << result.dpiY << "expected:" << commonDpi->first << "x"
                                << commonDpi->second;
                            return;
                        }
                    }

                    auto *item = new ImageItem(result.pixmap, result.size);
                    item->setItemPen(QPen(Qt::NoPen));
                    item->setFilePath(result.path);
                    item->setOriginalSize(result.size);

                    // 设置图片 DPI
                    if (result.dpiX > 0 && result.dpiY > 0)
                        item->setDpi(result.dpiX, result.dpiY);

                    if (arr == ArrangeHorizontal) {
                        item->setPos(*runningCoord, 0);
                        *runningCoord += result.size.width();
                    } else {
                        item->setPos(0, *runningCoord);
                        *runningCoord += result.size.height();
                    }

                    m_undoStack->push(new AddItemCommand(m_pView->scene(), item));
                    importedItems->append(item);
                } else {
                    qWarning() << "Import failed:" << result.path << result.errorMessage;
                }
            });

    connect(watcher, &QFutureWatcher<ImageUtils::ImportWorkerResult>::finished, this,
            [this, watcher, taskId, importedItems, fitType, fitVal]() {
                m_pProgressMgr->finishTask(taskId);
                watcher->deleteLater();

                // 多图导入完成后刷新状态栏
                _updateCanvasLabel();
                _updatePosLabel(m_lastScenePos);

                if (fitType != fctNone && !importedItems->isEmpty()) {
                    CanvasItem *canvas = m_pView->canvasItem();
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
                            m_undoStack->beginMacro(tr("Fit Canvas on Import"));

                            if (offsetX != 0 || offsetY != 0) {
                                QPointF delta(offsetX, offsetY);
                                QList<QPointF> oldPositions, newPositions;
                                for (auto *item : *importedItems) {
                                    oldPositions << item->pos();
                                    newPositions << item->pos() + delta;
                                    item->setPos(item->pos() + delta);
                                }
                                m_undoStack->push(new MoveItemsCommand(
                                    QList<QGraphicsItem *>(importedItems->begin(),
                                                           importedItems->end()),
                                    oldPositions, newPositions, m_pView->scene()));
                            }

                            m_undoStack->push(new CanvasResizeCommand(canvas, oldSize, newSize,
                                                                      m_pView->scene()));
                            m_undoStack->endMacro();
                            _updateCanvasLabel();
                            m_pView->fitToCanvas();
                        }
                    }
                }

                delete importedItems;
                qDebug() << QTime::currentTime().toString("HH:mm:ss.zzz") << "Finished import";
            });

    auto future = QtConcurrent::mapped(ordered, ImageUtils::runImportWorker);
    watcher->setFuture(future);
    qDebug() << QTime::currentTime().toString("HH:mm:ss.zzz");
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
            qreal mmW = oldSize.width() / oldPpi * 25.4;
            qreal mmH = oldSize.height() / oldPpi * 25.4;

            // 更新画布 DPI（不锁定）
            canvas->setCanvasDpi(dpiOverride, dpiOverride);
            canvas->setPpi(static_cast<qreal>(dpiOverride));

            // 从物理 mm 反算新像素尺寸（ceil 保证整数边界，消除取整不一致）
            qreal mmToPx = static_cast<qreal>(dpiOverride) / 25.4;
            QSizeF newSize(std::ceil(mmW * mmToPx), std::ceil(mmH * mmToPx));
            m_pView->setCanvasSize(newSize);

            // 同步缩放非图片图元的像素尺寸和位置（factor = newPpi/oldPpi）
            qreal factor = static_cast<qreal>(dpiOverride) / oldPpi;
            if (!qFuzzyCompare(factor, 1.0)) {
                const auto selectable = ::filterSelectableItems(m_pView->scene()->items());
                for (auto *item : selectable) {
                    if (dynamic_cast<ImageItem *>(item) || item->parentItem())
                        continue;
                    item->setPos(item->pos() * factor);
                    item->setTransform(QTransform::fromScale(factor, factor) * item->transform());
                }
            }

            m_hRuler->setPpi(static_cast<qreal>(dpiOverride));
            m_vRuler->setPpi(static_cast<qreal>(dpiOverride));
            m_pPropertyPanel->setDisplayPpi(static_cast<qreal>(dpiOverride));
            m_hRuler->updateRuler();
            m_vRuler->updateRuler();
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
}

// ============================================================
// 撤销/重做
// ============================================================
void MainWindow::onUndo()
{
    m_undoStack->undo();
}
void MainWindow::onRedo()
{
    m_undoStack->redo();
}

// ============================================================
// 剪贴板操作
// ============================================================
void MainWindow::onCut()
{
    auto items = m_pView->scene()->selectedItems();
    if (items.isEmpty())
        return;

    // 使用 macro 将 Copy+Delete 合并为单步撤销
    m_undoStack->beginMacro(tr("Cut"));
    onCopy();
    onDelete();
    m_undoStack->endMacro();
}

void MainWindow::onCopy()
{
    auto items = m_pView->scene()->selectedItems();
    if (items.isEmpty())
        return;
    copyItemsToClipboard(items);
}

void MainWindow::onPaste()
{
    auto items = pasteItemsFromClipboard();
    if (items.isEmpty())
        return;

    // 偏移粘贴位置
    for (auto *item : items)
        item->moveBy(20, 20);

    m_undoStack->push(new PasteItemsCommand(m_pView->scene(), items));

    // 选中粘贴的项
    m_pView->scene()->clearSelection();
    for (auto *item : items)
        item->setSelected(true);

    // 标记为粘贴图元（用于区分选中框样式）
    m_pView->setPastedItems(items);
}

void MainWindow::onDelete()
{
    auto items = m_pView->scene()->selectedItems();
    if (items.isEmpty())
        return;

    auto deletable = ::filterSelectableItems(items);
    if (deletable.isEmpty())
        return;

    // 检查是否包含 ImageItem
    bool hasImageItem = false;
    for (auto *item : deletable) {
        if (dynamic_cast<ImageItem *>(item)) {
            hasImageItem = true;
            break;
        }
    }

    m_undoStack->push(new RemoveItemsCommand(m_pView->scene(), deletable));

    // 如果删除了图片图元，检查是否需要解除 DPI 锁定
    if (hasImageItem)
        _unlockCanvasDpiIfNoImages();
}

void MainWindow::onSelectAll()
{
    auto items = m_pView->scene()->items();
    auto selectable = ::filterSelectableItems(items);
    for (auto *item : selectable)
        item->setSelected(true);
}

// ============================================================
// Z 序操作
// ============================================================
void MainWindow::onBringToFront()
{
    auto items = m_pView->scene()->selectedItems();
    if (items.isEmpty())
        return;

    QList<qreal> oldZ, newZ;
    for (auto *item : items) {
        oldZ << item->zValue();
        newZ << item->zValue() + 1.0;
    }
    m_undoStack->push(new ZValueChangeCommand(items, oldZ, newZ, m_pView->scene()));
}

void MainWindow::onSendToBack()
{
    auto items = m_pView->scene()->selectedItems();
    if (items.isEmpty())
        return;

    QList<qreal> oldZ, newZ;
    for (auto *item : items) {
        oldZ << item->zValue();
        newZ << item->zValue() - 1.0;
    }
    m_undoStack->push(new ZValueChangeCommand(items, oldZ, newZ, m_pView->scene()));
}

// ============================================================
// 成组 / 解散组
// ============================================================
void MainWindow::onGroup()
{
    auto items = filterSelectableItems();
    if (items.size() < 2)
        return;

    // 过滤掉已经是组成员的图元（已在某个 GraphicsItemGroup 内的跳过）
    QList<QGraphicsItem *> topLevel;
    for (auto *item : items) {
        if (!item->parentItem())
            topLevel << item;
    }
    if (topLevel.size() < 2)
        return;

    auto *cmd = new GroupItemsCommand(m_pView->scene(), topLevel);
    m_undoStack->push(cmd);

    // 选中新组
    m_pView->scene()->clearSelection();
    if (cmd->groupItem())
        cmd->groupItem()->setSelected(true);
}

void MainWindow::onUngroup()
{
    auto items = filterSelectableItems();
    if (items.isEmpty())
        return;

    // 找出所有选中的 GraphicsItemGroup，并提前收集子图元
    QList<QGraphicsItem *> groupsToUngroup;
    QList<QGraphicsItem *> childrenToSelect;

    for (auto *item : items) {
        auto *grp = qgraphicsitem_cast<GraphicsItemGroup *>(item);
        if (grp) {
            groupsToUngroup << item;
            childrenToSelect << grp->childGraphicsItems();
        }
    }
    if (groupsToUngroup.isEmpty())
        return;

    m_undoStack->beginMacro(tr("Ungroup"));

    for (auto *groupItem : groupsToUngroup) {
        m_undoStack->push(new UngroupItemsCommand(m_pView->scene(), groupItem));
    }

    m_undoStack->endMacro();

    // 选中解散后的子图元
    m_pView->scene()->clearSelection();
    for (auto *child : childrenToSelect) {
        if (child && child->scene())
            child->setSelected(true);
    }
}

// ============================================================
// 对齐与分布面板
// ============================================================
void MainWindow::onAlignLayoutDialog()
{
    m_alignLayoutDlg->refreshSelectionInfo();
    m_alignLayoutDlg->show();
    m_alignLayoutDlg->raise();
}

// ============================================================
// 设置对话框
// ============================================================
void MainWindow::onSettings()
{
    SettingsDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    getToolInfo();
}

void MainWindow::onPreferences()
{
    PreferencesDialog dlg(this);
    dlg.exec();
}

void MainWindow::onAbout()
{
    QString strText = QString(tr("<h3>AT Drawing Tools</h3>"
                                 "<p>Current Version: %1</p>"
                                 "<p>    Rip Version: %2</p>"))
                          .arg(qApp->applicationVersion())
                          .arg(RipVersion);

    QMessageBox::about(this, tr("About AT Drawing Tools"), strText);
}

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
// 工具切换
// ============================================================
void MainWindow::onToolTriggered(Tool tool)
{
    m_currentTool = tool;
    m_pView->setTool(tool);

    // 更新工具栏按钮状态
    for (auto it = m_toolActions.begin(); it != m_toolActions.end(); ++it)
        it.value()->setChecked(it.key() == tool);

    _updateToolLabel();
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

    m_hRuler->updateRuler();
    m_vRuler->updateRuler();
    _updateCanvasLabel();
    _updatePosLabel(m_lastScenePos);
    m_pView->fitToCanvas();
}

// ============================================================
// 批量旋转选中图元 — 绕图元中心(单选)或组中心(多选)旋转
// ============================================================
void MainWindow::rotateSelectedItems(qreal angleDelta)
{
    auto items = filterSelectableItems();
    if (items.isEmpty())
        return;

    // 过滤掉不支持旋转的图元（如图片）
    QList<QGraphicsItem *> rotatableItems;
    for (auto *item : items) {
        auto *igi = dynamic_cast<IGraphicsItem *>(item);
        if (igi && (igi->propertyFlags() & IGraphicsItem::HasRotation))
            rotatableItems << item;
    }
    if (rotatableItems.isEmpty())
        return;

    items = rotatableItems;

    if (items.size() == 1) {
        // 单个图元：绕自身中心旋转（RotationChangeCommand 已内置中心补偿）
        auto *item = items.first();
        qreal oldRotation = item->rotation();
        qreal newRotation = oldRotation + angleDelta;
        m_undoStack->push(
            new RotationChangeCommand(item, oldRotation, newRotation, m_pView->scene()));
    } else {
        // 多个图元：绕组中心整体旋转
        // 1. 计算组中心（所有图元场景包围矩形的并集中心）
        QRectF groupSceneRect;
        for (auto *item : items) {
            QRectF itemSceneRect = item->mapToScene(item->boundingRect()).boundingRect();
            groupSceneRect = groupSceneRect.united(itemSceneRect);
        }
        QPointF groupCenter = groupSceneRect.center();

        // 2. 对每个图元：先绕自身中心旋转，再绕组中心公转
        m_undoStack->beginMacro(tr("Rotate %1\u00b0").arg(angleDelta, 0, 'f', 0));

        QList<QGraphicsItem *> moveItems;
        QList<QPointF> oldPositions;
        QList<QPointF> newPositions;

        for (auto *item : items) {
            qreal oldRotation = item->rotation();
            qreal newRotation = oldRotation + angleDelta;

            // 推入旋转命令（含中心补偿：旋转后图元中心位置不变）
            m_undoStack->push(
                new RotationChangeCommand(item, oldRotation, newRotation, m_pView->scene()));

            // 旋转命令 redo 后，图元中心仍位于旋转前的场景位置
            // 记录中心补偿后的位置（公转前的位置）
            QPointF posAfterCenterComp = item->pos();

            // 计算绕组中心的公转位移
            QPointF currentCenter = item->mapToScene(item->boundingRect().center());
            QLineF line(groupCenter, currentCenter);
            line.setAngle(line.angle() + angleDelta);
            QPointF orbitedCenter = line.p2();
            QPointF orbitalDelta = orbitedCenter - currentCenter;
            item->setPos(item->pos() + orbitalDelta);

            moveItems << item;
            oldPositions << posAfterCenterComp; // 公转前位置
            newPositions << item->pos(); // 公转后最终位置
        }

        // 公转位移变更作为一个命令
        if (!moveItems.isEmpty()) {
            m_undoStack->push(
                new MoveItemsCommand(moveItems, oldPositions, newPositions, m_pView->scene()));
        }

        m_undoStack->endMacro();
    }

    // 旋转后需要更新 ResizeHandleItem
    m_pView->scheduleResizeHandleUpdate();
}

// ============================================================
// 剪贴板序列化
// ============================================================
QList<QGraphicsItem *> MainWindow::filterSelectableItems() const
{
    return ::filterSelectableItems(m_pView->scene()->selectedItems());
}

void MainWindow::copyItemsToClipboard(const QList<QGraphicsItem *> &items)
{
    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);

    // 写入序列化版本号
    out << IGraphicsItem::kSerializationVersion;

    auto filtered = ::filterSelectableItems(items);
    int count = 0;
    for (auto *item : filtered) {
        if (dynamic_cast<IGraphicsItem *>(item))
            count++;
    }

    out << count;
    for (auto *item : filtered) {
        auto *gi = dynamic_cast<IGraphicsItem *>(item);
        if (!gi)
            continue;
        // 序列化到独立缓冲区，写入长度前缀（支持 CMYK 扩展数据）
        QByteArray itemBinary = serializeItemToBytes(gi);
        out << static_cast<quint32>(itemBinary.size());
        out.writeRawData(itemBinary.constData(), itemBinary.size());
    }

    auto *mime = new QMimeData;
    mime->setData(kMimeFormat, data);
    QApplication::clipboard()->setMimeData(mime);
}

QList<QGraphicsItem *> MainWindow::pasteItemsFromClipboard()
{
    QList<QGraphicsItem *> result;
    const QMimeData *mime = QApplication::clipboard()->mimeData();
    if (!mime || !mime->hasFormat(kMimeFormat))
        return result;

    QByteArray data = mime->data(kMimeFormat);
    QDataStream in(&data, QIODevice::ReadOnly);

    // 读取并校验序列化版本号
    int version = 0;
    in >> version;
    if (version < 1 || version > IGraphicsItem::kSerializationVersion) {
        qWarning("Unsupported clipboard format version: %d (current: %d)", version,
                 IGraphicsItem::kSerializationVersion);
        return result;
    }

    int count = 0;
    in >> count;
    for (int i = 0; i < count; ++i) {
        if (version >= 2) {
            // v2+: 长度前缀格式，每个图元数据独立
            quint32 dataLen = 0;
            in >> dataLen;
            if (in.status() != QDataStream::Ok || dataLen == 0) {
                qWarning("Clipboard: invalid item data length at index %d", i);
                break;
            }
            QByteArray itemData(dataLen, '\0');
            in.readRawData(itemData.data(), dataLen);
            if (in.status() != QDataStream::Ok) {
                qWarning("Clipboard: failed to read item data at index %d", i);
                break;
            }
            QDataStream itemIn(&itemData, QIODevice::ReadOnly);
            int typeInt = 0;
            itemIn >> typeInt;
            auto *gi = createItemByType(static_cast<IGraphicsItem::ItemType>(typeInt));
            if (!gi) {
                qWarning("Clipboard: unknown item type %d at index %d", typeInt, i);
                continue;
            }
            if (!gi->deserialize(itemIn)) {
                qWarning("Clipboard: failed to deserialize item type %d at index %d", typeInt, i);
                delete gi;
                continue;
            }
            result << dynamic_cast<QGraphicsItem *>(gi);
        } else {
            // v1: 旧格式，直接从流中读取
            int typeInt = 0;
            in >> typeInt;
            auto *gi = createItemByType(static_cast<IGraphicsItem::ItemType>(typeInt));
            if (!gi) {
                qWarning("Clipboard: unknown item type %d at index %d", typeInt, i);
                continue;
            }
            if (!gi->deserialize(in)) {
                qWarning("Clipboard: failed to deserialize item type %d "
                         "(stream corrupted)",
                         typeInt);
                delete gi;
                continue;
            }
            result << dynamic_cast<QGraphicsItem *>(gi);
        }
    }
    return result;
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // 快捷键切换工具
    switch (event->key()) {
    case Qt::Key_V:
        if (!event->modifiers())
            onToolTriggered(Tool::Select);
        return;
    case Qt::Key_R:
        if (!event->modifiers())
            onToolTriggered(Tool::Rect);
        return;
    case Qt::Key_E:
        if (!event->modifiers())
            onToolTriggered(Tool::Ellipse);
        return;
    case Qt::Key_L:
        if (!event->modifiers())
            onToolTriggered(Tool::Line);
        return;
    case Qt::Key_C:
        if (!event->modifiers())
            onToolTriggered(Tool::BezierCurve);
        return;
    case Qt::Key_F:
        if (!event->modifiers())
            onToolTriggered(Tool::Freehand);
        return;
    case Qt::Key_T:
        if (!event->modifiers())
            onToolTriggered(Tool::Text);
        return;
    case Qt::Key_Delete:
        onDelete();
        return;
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
    if (settings.contains("view/gridVisible")) {
        bool gridVisible = settings.value("view/gridVisible").toBool();
        if (m_pView) {
            m_pView->setGridVisible(gridVisible);
        }
        if (m_gridAction) {
            m_gridAction->setChecked(gridVisible);
        }
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

    // 保存其他设置
    if (m_pView) {
        settings.setValue("view/gridVisible", m_pView->isGridVisible());
    }

    settings.sync();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // 保存窗口状态
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
        _unlockCanvasDpiIfNoImages();
        return;
    }

    // 8. 替换图元：删除旧图元，添加新图元（类似导入多图流程，但不弹对话框，默认竖向排列后画布自适应）
    QList<QGraphicsItem *> oldItems;
    for (auto *imgItem : imageItems)
        oldItems << static_cast<QGraphicsItem *>(imgItem);

    m_undoStack->beginMacro(tr("Auto Layout"));

    if (!oldItems.isEmpty())
        m_undoStack->push(new RemoveItemsCommand(m_pView->scene(), oldItems));

    // 删除旧图元后解锁画布 DPI，让新图元通过 _tryLockCanvasDpi 重新锁定
    CanvasItem *canvas = m_pView->canvasItem();
    if (canvas && canvas->isDpiLocked())
        canvas->unlockDpi();

    // 导入排版后的图片，纵向排列
    constexpr qreal kVerticalGap = 10.0;
    constexpr int kThumbScaleDiv = 4; // 缩略图缩放比，与 runImportWorker 一致
    qreal yOffset = 0;
    QList<ImageItem *> newItems;
    // DPI 共享状态：以首个有效 DPI 锁定画布，后续图片必须匹配（与导入多图逻辑一致）
    QPair<int, int> firstDpi(0, 0);
    for (const auto &path : outputPaths) {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        reader.setAllocationLimit(0);
        QImage image = reader.read();
        if (image.isNull())
            continue;

        QSize originalSize = image.size();
        int dpiX = 0, dpiY = 0;

        // TIFF：从标签读取 DPI 与原始尺寸（缩略图模式下 pixmap 可能小于原始尺寸）
        if (ImageUtils::isTiffFile(path)) {
            QPair<int, int> dpi = ImageUtils::readTiffDpi(path);
            dpiX = dpi.first;
            dpiY = dpi.second;
            originalSize = ImageUtils::readTiffSize(path);
        }

        // 多图 DPI 筛选：以首个有效 DPI 为准，筛选相同 DPI 的图片（与导入多图逻辑一致）
        if (dpiX > 0 && dpiY > 0) {
            if (firstDpi.first == 0 && firstDpi.second == 0) {
                // 首个有效 DPI：尝试锁定画布，失败则跳过
                if (!_tryLockCanvasDpi(dpiX, dpiY)) {
                    qWarning() << "Skipped (DPI rejected):" << path;
                    continue;
                }
                firstDpi = { dpiX, dpiY };
            } else if (dpiX != firstDpi.first || dpiY != firstDpi.second) {
                // DPI 不匹配，跳过此图片
                qWarning() << "Skipped (DPI mismatch):" << path << "DPI:" << dpiX << "x" << dpiY
                           << "expected:" << firstDpi.first << "x" << firstDpi.second;
                continue;
            }
        }

        // 生成缩略图（固定 1/4 缩放，与导入流程一致，节省内存）
        if (image.width() > kThumbScaleDiv || image.height() > kThumbScaleDiv) {
            QSize thumbSize(qMax(1, image.width() / kThumbScaleDiv),
                            qMax(1, image.height() / kThumbScaleDiv));
            image = image.scaled(thumbSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }

        QPixmap pix = QPixmap::fromImage(image);
        auto *item = new ImageItem(pix, originalSize);
        item->setItemPen(QPen(Qt::NoPen));
        item->setFilePath(path);
        item->setOriginalSize(originalSize);
        item->setPos(0, yOffset);

        if (dpiX > 0 && dpiY > 0)
            item->setDpi(dpiX, dpiY);

        m_undoStack->push(new AddItemCommand(m_pView->scene(), item));
        newItems.append(item);
        yOffset += originalSize.height() + kVerticalGap;
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