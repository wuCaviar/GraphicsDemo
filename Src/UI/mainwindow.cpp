#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "AlignLayoutDialog.h"
#include "AlignmentUtils.h"
#include "AppConfig.h"
#include "BezierCurveItem.h"
#include "CanvasItem.h"
#include "Commands.h"
#include "colortransform.h"
#include "EllipseItem.h"

#include "ImageItem.h"
#include "ImageUtils.h"
#include "ProjectFile.h"

#include "LineItem.h"
#include "NewFileDialog.h"
#include "SettingsDialog.h"
#include "RectItem.h"
#include "ResizeHandleItem.h"
#include "RulerBar.h"
#include "TextItem.h"
#include "GraphicsItemGroup.h"
#include "FitCanvasDlg.h"

#include <algorithm>
#include <memory>

#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QCloseEvent>
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
#include <QLabel>
#include <QMap>
#include <QMimeData>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QSlider>
#include <QStyle>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
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

    // 加载 QSS 样式表
    loadStyleSheet();

    setWindowTitle(tr("AT Drawing Tools"));
    resize(1200, 800);

    // 加载窗口状态（工具栏位置、可见性等）
    loadWindowState();
}

void MainWindow::loadStyleSheet()
{
    QFile f(":/style/style.qss");
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qApp->setStyleSheet(f.readAll());
        f.close();
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
    m_pView = new QAtGraphicsView(this);
    m_pView->setUndoStack(m_undoStack);
    m_pView->setEnabled(false); // 没有画板前禁止操作
    setCentralWidget(m_pView);
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

    QAction *newAct =
        fileMenu->addAction(QIcon(":/icons/icons/file-new.svg"), tr("&New..."));
    newAct->setShortcut(QKeySequence::New);
    newAct->setToolTip(tr("Create a new canvas"));
    connect(newAct, &QAction::triggered, this, &MainWindow::onNew);

    fileMenu->addSeparator();

    QAction *openProjAct = fileMenu->addAction(
        style()->standardIcon(QStyle::SP_DialogOpenButton), tr("&Open Project..."));
    openProjAct->setShortcut(QKeySequence::Open);
    openProjAct->setToolTip(tr("Open a project file"));
    connect(openProjAct, &QAction::triggered, this, &MainWindow::onOpenProject);

    QAction *saveProjAct = fileMenu->addAction(
        style()->standardIcon(QStyle::SP_DialogSaveButton), tr("&Save Project..."));
    saveProjAct->setShortcut(QKeySequence::Save);
    saveProjAct->setToolTip(tr("Save the current project"));
    connect(saveProjAct, &QAction::triggered, this, &MainWindow::onSaveProject);

    fileMenu->addSeparator();

    QAction *importAct = fileMenu->addAction(
        QIcon(":/icons/icons/file-import.svg"), tr("&Import Image..."));
    importAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
    importAct->setToolTip(tr("Import an image onto the canvas"));
    connect(importAct, &QAction::triggered, this, &MainWindow::onImportImage);

    QAction *exportAct = fileMenu->addAction(
        QIcon(":/icons/icons/file-export.svg"), tr("&Export Image..."));
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

    m_undoAction =
        editMenu->addAction(QIcon(":/icons/icons/edit-undo.svg"), tr("&Undo"));
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setToolTip(tr("Undo the last action"));
    connect(m_undoAction, &QAction::triggered, this, &MainWindow::onUndo);

    m_redoAction =
        editMenu->addAction(QIcon(":/icons/icons/edit-redo.svg"), tr("&Redo"));
    m_redoAction->setShortcut(QKeySequence::Redo);
    m_redoAction->setToolTip(tr("Redo the last undone action"));
    connect(m_redoAction, &QAction::triggered, this, &MainWindow::onRedo);

    editMenu->addSeparator();

    QAction *cutAct =
        editMenu->addAction(QIcon(":/icons/icons/edit-cut.svg"), tr("Cu&t"));
    cutAct->setShortcut(QKeySequence::Cut);
    cutAct->setToolTip(tr("Cut the selected items to clipboard"));
    connect(cutAct, &QAction::triggered, this, &MainWindow::onCut);

    QAction *copyAct =
        editMenu->addAction(QIcon(":/icons/icons/edit-copy.svg"), tr("&Copy"));
    copyAct->setShortcut(QKeySequence::Copy);
    copyAct->setToolTip(tr("Copy the selected items to clipboard"));
    connect(copyAct, &QAction::triggered, this, &MainWindow::onCopy);

    QAction *pasteAct = editMenu->addAction(
        QIcon(":/icons/icons/edit-paste.svg"), tr("&Paste"));
    pasteAct->setShortcut(QKeySequence::Paste);
    pasteAct->setToolTip(tr("Paste items from clipboard"));
    connect(pasteAct, &QAction::triggered, this, &MainWindow::onPaste);

    editMenu->addSeparator();

    QAction *deleteAct = editMenu->addAction(
        QIcon(":/icons/icons/edit-delete.svg"), tr("&Delete"));
    deleteAct->setShortcut(QKeySequence::Delete);
    deleteAct->setToolTip(tr("Delete the selected items"));
    connect(deleteAct, &QAction::triggered, this, &MainWindow::onDelete);

    QAction *selectAllAct = editMenu->addAction(
        QIcon(":/icons/icons/edit-select-all.svg"), tr("Select &All"));
    selectAllAct->setShortcut(QKeySequence::SelectAll);
    selectAllAct->setToolTip(tr("Select all items on the canvas"));
    connect(selectAllAct, &QAction::triggered, this, &MainWindow::onSelectAll);

    // ---- 排列 ----
    QMenu *arrMenu = menu->addMenu(tr("&Arrange"));
    arrMenu
        ->addAction(QIcon(":/icons/icons/bring-front.svg"), tr("Bring Forward"),
                    this, &MainWindow::onBringToFront)
        ->setToolTip(tr("Bring selected items forward one step"));
    arrMenu
        ->addAction(QIcon(":/icons/icons/send-back.svg"), tr("Send Backward"),
                    this, &MainWindow::onSendToBack)
        ->setToolTip(tr("Send selected items backward one step"));
    arrMenu->addSeparator();
    QAction *groupAct =
        arrMenu->addAction(QIcon(":/icons/icons/group.svg"), tr("&Group"));
    groupAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    groupAct->setToolTip(tr("Group selected items together"));
    connect(groupAct, &QAction::triggered, this, &MainWindow::onGroup);
    QAction *ungroupAct =
        arrMenu->addAction(QIcon(":/icons/icons/ungroup.svg"), tr("&Ungroup"));
    ungroupAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_G));
    ungroupAct->setToolTip(tr("Ungroup selected items"));
    connect(ungroupAct, &QAction::triggered, this, &MainWindow::onUngroup);
    arrMenu->addSeparator();
    QMenu *alignMenu = arrMenu->addMenu(tr("Align"));
    alignMenu->addAction(tr("Left"), this, &MainWindow::onAlignLeft)
        ->setToolTip(tr("Align selected items to the left edge"));
    alignMenu->addAction(tr("Right"), this, &MainWindow::onAlignRight)
        ->setToolTip(tr("Align selected items to the right edge"));
    alignMenu->addAction(tr("Top"), this, &MainWindow::onAlignTop)
        ->setToolTip(tr("Align selected items to the top edge"));
    alignMenu->addAction(tr("Bottom"), this, &MainWindow::onAlignBottom)
        ->setToolTip(tr("Align selected items to the bottom edge"));
    alignMenu->addAction(tr("Center H"), this, &MainWindow::onAlignHCenter)
        ->setToolTip(tr("Align selected items to the horizontal center"));
    alignMenu->addAction(tr("Center V"), this, &MainWindow::onAlignVCenter)
        ->setToolTip(tr("Align selected items to the vertical center"));
    QMenu *distMenu = arrMenu->addMenu(tr("Distribute"));
    distMenu->addAction(tr("Horizontally"), this, &MainWindow::onDistributeH)
        ->setToolTip(tr("Distribute selected items evenly horizontally"));
    distMenu->addAction(tr("Vertically"), this, &MainWindow::onDistributeV)
        ->setToolTip(tr("Distribute selected items evenly vertically"));
    distMenu->addSeparator();
    distMenu
        ->addAction(tr("Align && Layout..."), this,
                    &MainWindow::onAlignLayoutDialog)
        ->setToolTip(tr("Open the Align & Layout dialog"));
    arrMenu->addSeparator();
    QAction *fitCanvasAct = arrMenu->addAction(
        tr("Fit Canvas to Selection"), this, &MainWindow::onFitCanvasToItems);
    fitCanvasAct->setToolTip(tr("Resize the canvas to fit the selected items"));
    arrMenu->addSeparator();
    QMenu *rotateMenu = arrMenu->addMenu(tr("Rotate"));
    rotateMenu
        ->addAction(QIcon(":/icons/icons/rotate-cw.svg"),
                    tr("90\u00b0 Clockwise"), this,
                    [this]() { rotateSelectedItems(90.0); })
        ->setToolTip(tr("Rotate selected items 90 degrees clockwise"));
    rotateMenu
        ->addAction(QIcon(":/icons/icons/rotate-ccw.svg"),
                    tr("90\u00b0 Counter-clockwise"), this,
                    [this]() { rotateSelectedItems(-90.0); })
        ->setToolTip(tr("Rotate selected items 90 degrees counter-clockwise"));
    rotateMenu
        ->addAction(tr("180\u00b0"), this,
                    [this]() { rotateSelectedItems(180.0); })
        ->setToolTip(tr("Rotate selected items 180 degrees"));

    // ---- 设置 ----
    QMenu *settingsMenu = menu->addMenu(tr("&Settings"));
    settingsMenu->addAction(tr("&Settings..."), this, &MainWindow::onSettings)
        ->setToolTip(tr("Open application settings"));

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
    m_gridAction = viewMenu->addAction(QIcon(":/icons/icons/view-grid.svg"),
                                       tr("Show Grid"));
    m_gridAction->setCheckable(true);
    m_gridAction->setChecked(true);
    m_gridAction->setToolTip(tr("Show or hide the grid"));
    connect(m_gridAction, &QAction::toggled, this,
            [this](bool checked) { m_pView->setGridVisible(checked); });

    // 刻度尺单位切换（px ↔ mm）
    m_rulerUnitAction = viewMenu->addAction(tr("Ruler Unit: mm"));
    m_rulerUnitAction->setCheckable(true);
    m_rulerUnitAction->setToolTip(
        tr("Toggle ruler unit between millimeters and pixels"));
    connect(m_rulerUnitAction, &QAction::toggled, this, [this](bool checked) {
        auto unit = checked ? RulerBar::Millimeter : RulerBar::Pixel;
        m_hRuler->setUnit(unit);
        m_vRuler->setUnit(unit);
        m_rulerUnitAction->setText(checked ? tr("Ruler Unit: mm")
                                           : tr("Ruler Unit: px"));
        // 同步更新状态栏坐标和画布尺寸的单位显示
        _updateCanvasLabel();
        _updatePosLabel(m_lastScenePos);
    });

    viewMenu->addSeparator();
    // 缩放适配
    QAction *fitAct = new QAction(QIcon(":/icons/icons/view-fit.svg"),
                                  tr("Fit to Canvas"), this);
    fitAct->setToolTip(tr("Fit the view to the canvas"));
    connect(fitAct, &QAction::triggered, this,
            [this]() { m_pView->fitToCanvas(); });
    viewMenu->addAction(fitAct);

    QAction *resetZoomAct = new QAction(
        QIcon(":/icons/icons/view-zoom-reset.svg"), tr("Reset Zoom (0)"), this);
    resetZoomAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    resetZoomAct->setToolTip(tr("Reset zoom to 100%"));
    connect(resetZoomAct, &QAction::triggered, this,
            [this]() { m_pView->setZoomLevel(1.0); });
    viewMenu->addAction(resetZoomAct);

    _updateUndoRedoActions();
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
    QAction *newAct =
        new QAction(QIcon(":/icons/icons/file-new.svg"), tr("New"), this);
    newAct->setToolTip(tr("Create a new canvas"));
    connect(newAct, &QAction::triggered, this, &MainWindow::onNew);
    fileEditBar->addAction(newAct);

    // Import 按钮
    QAction *importAct = new QAction(QIcon(":/icons/icons/file-import.svg"),
                                     tr("Import Image"), this);
    importAct->setToolTip(tr("Import an image onto the canvas"));
    connect(importAct, &QAction::triggered, this, &MainWindow::onImportImage);
    fileEditBar->addAction(importAct);

    // Export 按钮
    QAction *exportAct = new QAction(QIcon(":/icons/icons/file-export.svg"),
                                     tr("Export Image"), this);
    exportAct->setToolTip(tr("Export the canvas to an image file"));
    connect(exportAct, &QAction::triggered, this, &MainWindow::onExportImage);
    fileEditBar->addAction(exportAct);

    fileEditBar->addSeparator();

    fileEditBar->addAction(m_undoAction);
    fileEditBar->addAction(m_redoAction);

    fileEditBar->addSeparator();

    // Cut 按钮
    QAction *cutAct =
        new QAction(QIcon(":/icons/icons/edit-cut.svg"), tr("Cut"), this);
    cutAct->setShortcut(QKeySequence::Cut);
    cutAct->setToolTip(tr("Cut the selected items to clipboard"));
    connect(cutAct, &QAction::triggered, this, &MainWindow::onCut);
    fileEditBar->addAction(cutAct);

    // Copy 按钮
    QAction *copyAct =
        new QAction(QIcon(":/icons/icons/edit-copy.svg"), tr("Copy"), this);
    copyAct->setShortcut(QKeySequence::Copy);
    copyAct->setToolTip(tr("Copy the selected items to clipboard"));
    connect(copyAct, &QAction::triggered, this, &MainWindow::onCopy);
    fileEditBar->addAction(copyAct);

    // Paste 按钮
    QAction *pasteAct =
        new QAction(QIcon(":/icons/icons/edit-paste.svg"), tr("Paste"), this);
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

    auto addToolAction = [&](const QString &iconPath, const QString &text,
                             Tool tool, const QString &shortcut = {}) {
        QAction *act = drawBar->addAction(QIcon(iconPath), text);
        act->setCheckable(true);
        act->setToolTip(text);
        actionGroup->addAction(act);
        if (!shortcut.isEmpty())
            act->setShortcut(QKeySequence(shortcut));
        connect(act, &QAction::triggered, this,
                [this, tool]() { onToolTriggered(tool); });
        m_toolActions[tool] = act;
        return act;
    };

    auto *selectAct = addToolAction(":/icons/icons/tool-select.svg",
                                    tr("Select (V)"), Tool::Select, "V");
    selectAct->setChecked(true);
    addToolAction(":/icons/icons/tool-rect.svg", tr("Rectangle (R)"),
                  Tool::Rect, "R");
    addToolAction(":/icons/icons/tool-ellipse.svg", tr("Ellipse (E)"),
                  Tool::Ellipse, "E");
    addToolAction(":/icons/icons/tool-line.svg", tr("Line (L)"), Tool::Line,
                  "L");
    addToolAction(":/icons/icons/tool-curve.svg", tr("Curve (C)"),
                  Tool::BezierCurve, "C");
    addToolAction(":/icons/icons/tool-freehand.svg", tr("Freehand (F)"),
                  Tool::Freehand, "F");
    addToolAction(":/icons/icons/tool-text.svg", tr("Text (T)"), Tool::Text,
                  "T");

    // 对齐工具栏
    QToolBar *alignToolBar = new QToolBar(tr("Align"), this);
    alignToolBar->setObjectName("AlignToolBar");
    alignToolBar->setMovable(false);
    alignToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    alignToolBar->setIconSize(QSize(20, 20));
    addToolBar(Qt::TopToolBarArea, alignToolBar);

    QAction *alignLayoutAct = alignToolBar->addAction(
        QIcon(":/icons/icons/align-layout.svg"), tr("Align && Layout..."));
    alignLayoutAct->setToolTip(tr("Open Align & Layout dialog"));
    connect(alignLayoutAct, &QAction::triggered, this,
            &MainWindow::onAlignLayoutDialog);

    alignToolBar->addSeparator();

    // 成组/解组
    QAction *groupAct =
        alignToolBar->addAction(QIcon(":/icons/icons/group.svg"), tr("Group"));
    groupAct->setToolTip(tr("Group selected items (Ctrl+G)"));
    groupAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    connect(groupAct, &QAction::triggered, this, &MainWindow::onGroup);

    QAction *ungroupAct = alignToolBar->addAction(
        QIcon(":/icons/icons/ungroup.svg"), tr("Ungroup"));
    ungroupAct->setToolTip(tr("Ungroup selected items (Ctrl+Shift+G)"));
    ungroupAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_G));
    connect(ungroupAct, &QAction::triggered, this, &MainWindow::onUngroup);

    alignToolBar->addSeparator();

    // 顺时针旋转 90°
    QAction *rotateCWAct = alignToolBar->addAction(
        QIcon(":/icons/icons/rotate-cw.svg"), tr("Rotate 90\u00b0 CW"));
    rotateCWAct->setToolTip(tr("Rotate 90\u00b0 clockwise"));
    connect(rotateCWAct, &QAction::triggered, this,
            [this]() { rotateSelectedItems(90.0); });

    // 逆时针旋转 90°
    QAction *rotateCCWAct = alignToolBar->addAction(
        QIcon(":/icons/icons/rotate-ccw.svg"), tr("Rotate 90\u00b0 CCW"));
    rotateCCWAct->setToolTip(tr("Rotate 90\u00b0 counter-clockwise"));
    connect(rotateCCWAct, &QAction::triggered, this,
            [this]() { rotateSelectedItems(-90.0); });

    alignToolBar->addSeparator();

    // 画布适配选中图元
    QAction *fitCanvasAct = alignToolBar->addAction(
        QIcon(":/icons/icons/view-fit.svg"), tr("Fit Canvas to Selection"));
    fitCanvasAct->setToolTip(tr("Resize the canvas to fit the selected items"));
    connect(fitCanvasAct, &QAction::triggered, this,
            &MainWindow::onFitCanvasToItems);
}

void MainWindow::_initPropertyPanel()
{
    m_pPropertyPanel = new PropertyPanel(this);
    m_pPropertyPanel->setMinimumWidth(300);
    addDockWidget(Qt::RightDockWidgetArea, m_pPropertyPanel);

    m_alignLayoutDlg =
        new AlignLayoutDialog(m_pView->scene(), m_undoStack, this);
    m_alignLayoutDlg->setObjectName("AlignLayoutDock");
    m_alignLayoutDlg->setMinimumWidth(300);
    splitDockWidget(m_pPropertyPanel, m_alignLayoutDlg, Qt::Vertical);
    m_alignLayoutDlg->hide();
}

void MainWindow::_initConnections()
{
    connect(m_pView, &QAtGraphicsView::selectionChanged, this,
            &MainWindow::onSelectionChanged);
    connect(m_pView, &QAtGraphicsView::itemAdded, this,
            &MainWindow::onItemAdded);

    // 右键菜单 → 复用菜单栏的 Bring Forward / Send Backward
    connect(m_pView, &QAtGraphicsView::bringToFrontRequested, this,
            &MainWindow::onBringToFront);
    connect(m_pView, &QAtGraphicsView::sendToBackRequested, this,
            &MainWindow::onSendToBack);
    connect(m_pView, &QAtGraphicsView::groupRequested, this,
            &MainWindow::onGroup);
    connect(m_pView, &QAtGraphicsView::ungroupRequested, this,
            &MainWindow::onUngroup);
    connect(m_pView, &QAtGraphicsView::fitCanvasToItemsRequested, this,
            &MainWindow::onFitCanvasToItems);

    connect(m_pPropertyPanel, &PropertyPanel::penChanged, this,
            &MainWindow::onPenChanged);
    connect(m_pPropertyPanel, &PropertyPanel::brushChanged, this,
            &MainWindow::onBrushChanged);
    connect(m_pPropertyPanel, &PropertyPanel::fontChanged, this,
            &MainWindow::onFontChanged);
    connect(m_pPropertyPanel, &PropertyPanel::textChanged, this,
            &MainWindow::onTextChanged);
    connect(m_pPropertyPanel, &PropertyPanel::geometryChanged, this,
            &MainWindow::onGeometryChanged);
    connect(m_pPropertyPanel, &PropertyPanel::cornerRadiusChanged, this,
            &MainWindow::onCornerRadiusChanged);
    connect(m_pPropertyPanel, &PropertyPanel::positionChanged, this,
            &MainWindow::onPositionChanged);
    connect(m_pPropertyPanel, &PropertyPanel::rotationChanged, this,
            &MainWindow::onRotationChanged);

    connect(m_undoStack, &QUndoStack::canUndoChanged, this,
            [this]() { _updateUndoRedoActions(); });
    connect(m_undoStack, &QUndoStack::canRedoChanged, this,
            [this]() { _updateUndoRedoActions(); });

    // undo/redo 后更新 ResizeHandleItem 位置（而非重建，避免选中框闪烁）
    connect(m_undoStack, &QUndoStack::indexChanged, this,
            [this]() { m_pView->refreshResizeHandle(); });

    // 视图滚动/缩放时更新刻度尺
    connect(m_pView->horizontalScrollBar(), &QScrollBar::valueChanged, m_hRuler,
            &RulerBar::updateRuler);
    connect(m_pView->verticalScrollBar(), &QScrollBar::valueChanged, m_vRuler,
            &RulerBar::updateRuler);

    // 状态栏：鼠标位置 & 缩放变化 & 刻度尺鼠标位置同步
    connect(m_pView, &QAtGraphicsView::mousePositionChanged, this,
            [this](const QPointF &pos) {
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
            [this](QGraphicsItem *newFocus, QGraphicsItem *oldFocus,
                   Qt::FocusReason) {
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
    m_posLabel = new QLabel(tr("X: 0.0 px  Y: 0.0 px"));
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
    bar->addPermanentWidget(m_pProgressMgr->label());
    bar->addPermanentWidget(m_pProgressMgr->bar());
    bar->addPermanentWidget(createStatusSeparator(bar));
    bar->addPermanentWidget(m_zoomLabel);
    bar->addPermanentWidget(m_zoomSlider);
    bar->addPermanentWidget(createStatusSeparator(bar));
    bar->addPermanentWidget(m_canvasLabel);
    bar->addPermanentWidget(m_toolLabel);
}

void MainWindow::_initNetWork()
{
    m_pNetWorkUtils = new NetWorkUtils(this);
    connect(m_pNetWorkUtils, &NetWorkUtils::requestFinished, this,
            &MainWindow::onRequestFinished);
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
    bool isMm = m_hRuler->unit() == RulerBar::Millimeter;
    if (isMm) {
        qreal ppi = m_pView->canvasItem() ? m_pView->canvasItem()->ppi() : 96.0;
        qreal kPxToMm = 25.4 / ppi;
        qreal xmm = scenePos.x() * kPxToMm;
        qreal ymm = scenePos.y() * kPxToMm;
        m_posLabel->setText(
            tr("X: %1 mm  Y: %2 mm").arg(xmm, 0, 'f', 1).arg(ymm, 0, 'f', 1));
    } else {
        m_posLabel->setText(tr("X: %1 px  Y: %2 px")
                                .arg(scenePos.x(), 0, 'f', 1)
                                .arg(scenePos.y(), 0, 'f', 1));
    }
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
    bool isMm = m_hRuler->unit() == RulerBar::Millimeter;
    if (isMm) {
        qreal kPxToMm = 25.4 / ppi;
        m_canvasLabel->setText(tr("Canvas: %1 \u00d7 %2 mm \u00b7 %3 PPI")
                                   .arg(sz.width() * kPxToMm, 0, 'f', 1)
                                   .arg(sz.height() * kPxToMm, 0, 'f', 1)
                                   .arg(ppi, 0, 'f', 0));
    } else {
        m_canvasLabel->setText(tr("Canvas: %1 \u00d7 %2 px \u00b7 %3 PPI")
                                   .arg(sz.width(), 0, 'f', 1)
                                   .arg(sz.height(), 0, 'f', 1)
                                   .arg(ppi, 0, 'f', 0));
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
    if (m_pNetWorkUtils) {
        // 同步请求获取 Rip 版本（阻塞主线程，通常很快）
        m_pNetWorkUtils->doRipVersion();
    }

    // 获取其他工具版本，可用性
}
// ============================================================
// 文件操作
// ============================================================
void MainWindow::onNew()
{
    NewFileDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    QSizeF canvasSize = dlg.selectedSize();
    qreal ppi = dlg.selectedPpi();

    // 先清空 undo 栈，避免命令引用即将被删除的图元
    m_undoStack->clear();

    // 清空属性面板引用
    m_pPropertyPanel->setItem(nullptr);

    // 安全清空场景并重建画布
    m_pView->resetCanvas(canvasSize);
    m_pView->setEnabled(true); // 画板就绪，允许操作

    // 设置画布 PPI
    if (m_pView->canvasItem())
        m_pView->canvasItem()->setPpi(ppi);

    // 同步刻度尺 PPI
    m_hRuler->setPpi(ppi);
    m_vRuler->setPpi(ppi);

    m_pProgressMgr->resetAll();

    // PPI 变化后刷新刻度尺和状态栏
    m_hRuler->updateRuler();
    m_vRuler->updateRuler();
    _updateCanvasLabel();
    _updatePosLabel(m_lastScenePos);
}

void MainWindow::onOpenProject()
{
    // 检查当前画布是否有图元，提示用户保存
    auto existingItems = ::filterSelectableItems(m_pView->scene()->items());
    if (!existingItems.isEmpty()) {
        QMessageBox::StandardButton btn = QMessageBox::question(
            this, tr("Open Project"),
            tr("The current canvas has unsaved content.\n"
               "Do you want to save it before opening another project?"),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (btn == QMessageBox::Cancel)
            return;
        if (btn == QMessageBox::Yes) {
            // 临时清空路径以检测用户是否在保存对话框中取消
            QString previousPath = m_currentProjectPath;
            m_currentProjectPath.clear();
            onSaveProject();
            if (m_currentProjectPath.isEmpty()) {
                m_currentProjectPath = previousPath; // 恢复旧路径
                return; // 用户取消了保存对话框
            }
        }
    }

    QString path = QFileDialog::getOpenFileName(
        this, tr("Open Project"), QString(),
        tr("AT Project Files (*.atp);;All Files (*)"));
    if (path.isEmpty())
        return;

    ProjectFile pf;
    ProjectFile::ProjectInfo info;
    ProjectFile::CanvasInfo canvasInfo;
    QList<QGraphicsItem *> loadedItems;

    if (!pf.load(path, info, canvasInfo, loadedItems)) {
        QMessageBox::warning(this, tr("Open Project"),
                             tr("Failed to open project:\n%1").arg(pf.lastError()));
        return;
    }

    // 清空当前画布
    m_undoStack->clear();
    m_pPropertyPanel->setItem(nullptr);
    m_pView->resetCanvas(QSizeF(canvasInfo.width, canvasInfo.height));

    if (m_pView->canvasItem())
        m_pView->canvasItem()->setPpi(canvasInfo.dpi);

    // 添加加载的图元
    for (auto *item : loadedItems)
        m_pView->scene()->addItem(item);

    m_pView->setEnabled(true);

    // 同步刻度尺
    m_hRuler->setPpi(canvasInfo.dpi);
    m_vRuler->setPpi(canvasInfo.dpi);
    m_hRuler->updateRuler();
    m_vRuler->updateRuler();
    _updateCanvasLabel();
    _updatePosLabel(m_lastScenePos);

    m_currentProjectPath = path;
    m_pProgressMgr->resetAll();

    setWindowTitle(tr("AT Drawing Tools - %1").arg(info.name));
}

void MainWindow::onSaveProject()
{
    CanvasItem *canvas = m_pView->canvasItem();
    if (!canvas) {
        QMessageBox::warning(this, tr("Save Project"),
                             tr("No canvas to save. Create a new canvas first."));
        return;
    }

    QString path = QFileDialog::getSaveFileName(
        this, tr("Save Project"), QString(),
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
    info.author = QStringLiteral("Caviar");

    ProjectFile::CanvasInfo canvasInfo;
    canvasInfo.width = canvas->canvasSize().width();
    canvasInfo.height = canvas->canvasSize().height();
    canvasInfo.dpi = canvas->ppi();

    auto items = ::filterSelectableItems(m_pView->scene()->items());

    ProjectFile pf;
    if (!pf.save(path, info, canvasInfo, items)) {
        QMessageBox::warning(this, tr("Save Project"),
                             tr("Failed to save project:\n%1").arg(pf.lastError()));
        return;
    }

    m_currentProjectPath = path;
    setWindowTitle(tr("AT Drawing Tools - %1").arg(info.name));
}

void MainWindow::onImportImage()
{
    // 1. 多选文件
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, tr("Import Images"), QString(),
        tr("Images (*.tif *.tiff *.png *.jpg *.jpeg *.bmp);;"
           "TIFF (*.tif *.tiff);;"
           "PNG (*.png);;"
           "JPEG (*.jpg *.jpeg);;"
           "BMP (*.bmp);;"
           "All Files (*)"));
    if (paths.isEmpty())
        return;

    CanvasItem *canvas = m_pView->canvasItem();
    if (!canvas)
        return;

    // 2. 画布适配对话框
    FitCanvasDlg dlg;
    dlg.setParam(canvas->canvasSize());
    if (dlg.exec() != QDialog::Accepted)
        return;

    const FitCanvasType fitType = dlg.fitType();
    const double fitVal = dlg.fitValue();

    // 3. 启动进度任务，线程池异步加载（QPixmap 在工作线程中构造）
    const QString taskId =
        m_pProgressMgr->startTask(tr("Import"), paths.size());

    // QThreadPool *pool = QThreadPool::globalInstance();
    // pool->setMaxThreadCount(
    //     qMin(4, QThread::idealThreadCount())); // 避免 I/O 风暴

    auto *watcher = new QFutureWatcher<ImageUtils::ImportWorkerResult>(this);
    auto *importedItems = new QList<ImageItem *>();

    connect(
        watcher,
        &QFutureWatcher<ImageUtils::ImportWorkerResult>::progressValueChanged,
        this, [this, taskId](int progressValue) {
            m_pProgressMgr->updateTask(taskId, progressValue);
        });

    connect(
        watcher, &QFutureWatcher<ImageUtils::ImportWorkerResult>::resultReadyAt,
        this, [this, watcher, importedItems](int index) {
            auto result = watcher->resultAt(index);
            if (result.isValid()) {
                auto *item = new ImageItem(result.pixmap);
                item->setItemPen(QPen(Qt::NoPen));
                item->setFilePath(result.path);
                item->setPos(index * 30, index * 30);

                m_undoStack->push(new AddItemCommand(m_pView->scene(), item));
                importedItems->append(item);
            } else {
                qWarning() << "Import failed:" << result.path
                           << result.errorMessage;
            }
        });

    connect(
        watcher, &QFutureWatcher<ImageUtils::ImportWorkerResult>::finished,
        this, [this, watcher, taskId, importedItems, fitType, fitVal]() {
            m_pProgressMgr->finishTask(taskId);
            watcher->deleteLater();

            // 根据用户选择适配画布大小
            if (fitType != fctNone && !importedItems->isEmpty()) {
                CanvasItem *canvas = m_pView->canvasItem();
                if (canvas) {
                    // 计算所有导入图片的场景包围矩形并集
                    QRectF unitedRect;
                    for (auto *item : *importedItems) {
                        QRectF r = item->mapToScene(item->boundingRect())
                                       .boundingRect();
                        unitedRect =
                            unitedRect.isValid() ? unitedRect.united(r) : r;
                    }

                    // 计算偏移量：若图元在负坐标，整体平移到正坐标区域
                    qreal offsetX =
                        unitedRect.left() < 0 ? -unitedRect.left() : 0;
                    qreal offsetY =
                        unitedRect.top() < 0 ? -unitedRect.top() : 0;

                    QSizeF oldSize = canvas->canvasSize();
                    QSizeF newSize;
                    switch (fitType) {
                    case fctAdapt:
                        newSize = QSizeF(unitedRect.right() + offsetX,
                                         unitedRect.bottom() + offsetY);
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

                    if (newSize.isValid() && newSize.width() > 0
                        && newSize.height() > 0 && newSize != oldSize) {
                        m_undoStack->beginMacro(tr("Fit Canvas on Import"));

                        // 若有负坐标图元，先平移
                        if (offsetX > 0 || offsetY > 0) {
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

                        m_undoStack->push(new CanvasResizeCommand(
                            canvas, oldSize, newSize, m_pView->scene()));
                        m_undoStack->endMacro();
                        _updateCanvasLabel();
                        m_pView->fitToCanvas();
                    }
                }
            }

            delete importedItems;
            qDebug() << QTime::currentTime().toString("HH:mm:ss.zzz")
                     << "Finished import";
        });

    auto future = QtConcurrent::mapped(paths, ImageUtils::runImportWorker);

    watcher->setFuture(future);
    qDebug() << QTime::currentTime().toString("HH:mm:ss.zzz");
}

void MainWindow::onExportImage()
{
    if (m_exporting) {
        QMessageBox::information(this, tr("Export"),
                                 tr("An export is already in progress."));
        return;
    }

    // 1. 选择保存路径
    QString path = QFileDialog::getSaveFileName(
        this, tr("Export Image"), QString(), tr("prn Files (*.prn)"));
    if (path.isEmpty())
        return;

    QFileInfo fi(path);

    bool bRip = false;
    int ripXRes = 0, ripYRes = 0;
    SettingsDialog dlg(this);
    dlg.setOutputPath(fi.absolutePath());
    if (dlg.exec() == QDialog::Accepted) {
        bRip = true;
        ripXRes = dlg.resolutionX();
        ripYRes = dlg.resolutionY();
    }

    const QString tiffPath =
        fi.absolutePath() + "/" + fi.completeBaseName() + ".tif";

    // 2. 收集画布上的图元，按类型分离
    const auto allItems = ::filterSelectableItems(m_pView->scene()->items());
    QList<ImageItem *> imageItems;
    QList<QGraphicsItem *> nonImageItems;
    for (auto *gi : allItems) {
        auto *imgItem = dynamic_cast<ImageItem *>(gi);
        if (imgItem)
            imageItems.append(imgItem);
        else
            nonImageItems.append(gi);
    }

    if (imageItems.isEmpty() && nonImageItems.isEmpty()) {
        QMessageBox::warning(this, tr("Export"), tr("No items to export."));
        return;
    }

    // 3. 计算导出区域和 DPI
    CanvasItem *canvas = m_pView->canvasItem();
    QRectF exportRect;
    if (canvas) {
        exportRect = canvas->rect();
    } else {
        exportRect =
            m_pView->scene()->itemsBoundingRect().adjusted(-10, -10, 10, 10);
    }
    int targetDpi = canvas ? qRound(canvas->ppi()) : 72;
    if (bRip) {
        targetDpi = qMax(targetDpi, qMax(ripXRes, ripYRes));
    }

    // 4. 构建源 TIFF 输入列表
    QList<ImageUtils::SourceTiffInput> sources;
    for (auto *imgItem : imageItems) {
        const QString &fp = imgItem->filePath();
        if (fp.isEmpty()) {
            QMessageBox::warning(this, tr("Export"),
                                 tr("An image on the canvas has no source file "
                                    "and cannot be exported."));
            return;
        }
        if (!QFile::exists(fp)) {
            QMessageBox::warning(this, tr("Export"),
                                 tr("Source file not found:\n%1").arg(fp));
            return;
        }
        ImageUtils::SourceTiffInput src;
        src.filePath = fp;
        QRectF sceneRect = imgItem->sceneBoundingRect();
        src.outputRect = QRectF(sceneRect.left() - exportRect.left(),
                                sceneRect.top() - exportRect.top(),
                                sceneRect.width(), sceneRect.height());
        src.zOrder = static_cast<int>(imgItem->zValue());
        sources.append(src);
    }

    // 5. 将非 ImageItem 图元渲染为 CMYK 图层
    QList<ImageUtils::CmykOverlay> overlays;
    if (!nonImageItems.isEmpty()) {
        QList<QGraphicsItem *> allSceneItems = m_pView->scene()->items();
        QBrush oldSceneBg = m_pView->scene()->backgroundBrush();
        m_pView->setUpdatesEnabled(false);

        for (auto *item : nonImageItems) {
            QRectF sceneRect = item->sceneBoundingRect();
            // 裁剪到导出区域
            QRectF outRect = sceneRect.intersected(exportRect);
            if (outRect.isEmpty())
                continue;
            outRect.translate(-exportRect.topLeft());

            int w = qCeil(outRect.width());
            int h = qCeil(outRect.height());
            if (w <= 0 || h <= 0)
                continue;

            ImageUtils::CmykOverlay overlay;
            overlay.width = static_cast<uint32_t>(w);
            overlay.height = static_cast<uint32_t>(h);
            overlay.outputRect = outRect;
            overlay.zOrder = static_cast<int>(item->zValue());

            // 收集该项及其所有子项（处理分组）
            QSet<QGraphicsItem *> keepVisible;
            std::function<void(QGraphicsItem *)> collectDescendants =
                [&](QGraphicsItem *root) {
                    keepVisible.insert(root);
                    for (auto *child : root->childItems())
                        collectDescendants(child);
                };
            collectDescendants(item);

            // 保存可见性并隐藏其他项
            QHash<QGraphicsItem *, bool> savedVisibility;
            m_pView->scene()->setBackgroundBrush(Qt::NoBrush);
            for (auto *other : allSceneItems) {
                savedVisibility[other] = other->isVisible();
                if (!keepVisible.contains(other))
                    other->setVisible(false);
            }

            // 渲染
            QImage img(w, h, QImage::Format_ARGB32);
            img.fill(Qt::transparent);
            {
                QPainter painter(&img);
                painter.setRenderHint(QPainter::Antialiasing);
                painter.setRenderHint(QPainter::TextAntialiasing);
                m_pView->scene()->render(&painter, QRectF(0, 0, w, h),
                                         sceneRect.intersected(exportRect));
            }

            // 恢复可见性和场景背景
            for (auto *other : allSceneItems)
                other->setVisible(savedVisibility.value(other, true));
            m_pView->scene()->setBackgroundBrush(oldSceneBg);

            // BGRA → CMYK
            overlay.data.resize(static_cast<size_t>(w) * h * 4);
            QATColorManager &cm = QATColorManager::instance();
            if (cm.isValid()) {
                auto *xform = cm.createBgraToCmyk8(
                    INTENT_PERCEPTUAL, cmsFLAGS_BLACKPOINTCOMPENSATION
                                           | cmsFLAGS_HIGHRESPRECALC);
                QATColorManager::convertBgra8ToCmyk8(
                    xform, img.constBits(), overlay.data.data(), w, h);
                cmsDeleteTransform(xform);
                // 透明像素（A=0）→ CMYK 全零，避免将透明区域导出为黑色
                for (int y = 0; y < h; ++y) {
                    const uchar *src = img.constScanLine(y);
                    uint8_t *dst =
                        overlay.data.data() + static_cast<size_t>(y) * w * 4;
                    for (int x = 0; x < w; ++x) {
                        if (src[x * 4 + 3] == 0)
                            std::memset(dst + x * 4, 0, 4);
                    }
                }
            } else {
                for (int y = 0; y < h; ++y) {
                    const uchar *src = img.constScanLine(y);
                    uint8_t *dst =
                        overlay.data.data() + static_cast<size_t>(y) * w * 4;
                    for (int x = 0; x < w; ++x) {
                        if (src[x * 4 + 3] == 0) {
                            std::memset(dst + x * 4, 0, 4);
                            continue;
                        }
                        double b = src[x * 4 + 0] / 255.0;
                        double g = src[x * 4 + 1] / 255.0;
                        double r = src[x * 4 + 2] / 255.0;
                        double cd = 1.0 - r, md = 1.0 - g, yd = 1.0 - b;
                        double kd = std::min({cd, md, yd});
                        if (kd < 1.0) {
                            cd = (cd - kd) / (1.0 - kd) * 100.0;
                            md = (md - kd) / (1.0 - kd) * 100.0;
                            yd = (yd - kd) / (1.0 - kd) * 100.0;
                        } else {
                            cd = md = yd = 0.0;
                        }
                        kd *= 100.0;
                        int off = static_cast<int>(x) * 4;
                        dst[off + 0] =
                            static_cast<uint8_t>(std::clamp(cd * 2.55, 0.0, 255.0));
                        dst[off + 1] =
                            static_cast<uint8_t>(std::clamp(md * 2.55, 0.0, 255.0));
                        dst[off + 2] =
                            static_cast<uint8_t>(std::clamp(yd * 2.55, 0.0, 255.0));
                        dst[off + 3] =
                            static_cast<uint8_t>(std::clamp(kd * 2.55, 0.0, 255.0));
                    }
                }
            }
            overlays.append(std::move(overlay));
        }
        m_pView->setUpdatesEnabled(true);
    }

    // 6. 导出设置
    ImageUtils::TiffExportSettings settings;
    settings.dpi = targetDpi;

    const QString taskId = m_pProgressMgr->startTask(tr("Export"));
    m_exporting = true;

    QSize outSize = exportRect.size().toSize();

    auto progress = [this, taskId](int pct) {
        QMetaObject::invokeMethod(
            this,
            [this, taskId, pct]() { m_pProgressMgr->updateTask(taskId, pct); },
            Qt::QueuedConnection);
    };

    auto *thread = QThread::create([this, tiffPath, sources,
                                     overlays = std::move(overlays), outSize,
                                     settings, progress, taskId, bRip, ripXRes,
                                     ripYRes]() {
        auto result = ImageUtils::exportTiff(tiffPath, sources, overlays,
                                             outSize, settings, progress);

        QMetaObject::invokeMethod(
            this,
            [this, result, taskId, bRip, ripXRes, ripYRes, tiffPath]() {
                m_exporting = false;
                if (result.success) {
                    m_pProgressMgr->finishTask(taskId);
                    if (bRip && m_pNetWorkUtils) {
                        m_pNetWorkUtils->doAddRip(ripXRes, ripYRes,
                                                  result.filePath);
                    }
                } else {
                    m_pProgressMgr->cancelTask(taskId);
                    qWarning() << "Export failed:" << result.filePath
                               << result.errorMessage;
                }
            },
            Qt::QueuedConnection);
    });

    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
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

    m_undoStack->push(new RemoveItemsCommand(m_pView->scene(), deletable));
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
    m_undoStack->push(
        new ZValueChangeCommand(items, oldZ, newZ, m_pView->scene()));
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
    m_undoStack->push(
        new ZValueChangeCommand(items, oldZ, newZ, m_pView->scene()));
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

    m_undoStack->push(new GroupItemsCommand(m_pView->scene(), topLevel));

    // 选中新组
    m_pView->scene()->clearSelection();
    for (auto *item : m_pView->scene()->items()) {
        if (auto *grp = qgraphicsitem_cast<GraphicsItemGroup *>(item)) {
            if (!grp->childGraphicsItems().isEmpty()) {
                grp->setSelected(true);
                break;
            }
        }
    }
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
    // TODO: 使用 dlg.resolutionX(), dlg.resolutionY(),
    //       dlg.dotCurvePath(), dlg.colorCurvePath(), dlg.outputPath()
}

void MainWindow::onAbout()
{
    QString strText = QString(tr("<h3>AT Drawing Tools</h3>"
                                 "<p>Current Version: %1</p>"
                                 "<p>    Rip Version: %2</p>"))
                          .arg(ATHC_VERSION_STR_MAJ_MIN_MIC)
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
        tr("Align %1").arg(AlignmentUtils::alignDirectionName(direction)),
        m_pView->scene()));
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
        tr("Distribute %1")
            .arg(AlignmentUtils::distributeDirectionName(direction)),
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
void MainWindow::onPenChanged(QGraphicsItem *item, const QPen &oldPen,
                              const QPen &newPen)
{
    m_undoStack->push(new PropertyChangeCommand(
        item, PropertyChangeCommand::Pen, QVariant::fromValue(oldPen),
        QVariant::fromValue(newPen), m_pView->scene()));
}

void MainWindow::onBrushChanged(QGraphicsItem *item, const QBrush &oldBrush,
                                const QBrush &newBrush)
{
    m_undoStack->push(new PropertyChangeCommand(
        item, PropertyChangeCommand::Brush, QVariant::fromValue(oldBrush),
        QVariant::fromValue(newBrush), m_pView->scene()));
}

void MainWindow::onFontChanged(QGraphicsItem *item, const QFont &oldFont,
                               const QFont &newFont)
{
    m_undoStack->push(new PropertyChangeCommand(
        item, PropertyChangeCommand::Font, QVariant::fromValue(oldFont),
        QVariant::fromValue(newFont), m_pView->scene()));
}

void MainWindow::onTextChanged(QGraphicsItem *item, const QString &oldText,
                               const QString &newText)
{
    m_undoStack->push(new PropertyChangeCommand(
        item, PropertyChangeCommand::Text, QVariant(oldText), QVariant(newText),
        m_pView->scene()));
}

void MainWindow::onGeometryChanged(QGraphicsItem *item, const QRectF &oldRect,
                                   const QRectF &newRect)
{
    m_undoStack->push(new PropertyChangeCommand(
        item, PropertyChangeCommand::Geometry, QVariant(oldRect),
        QVariant(newRect), m_pView->scene()));
}

void MainWindow::onCornerRadiusChanged(QGraphicsItem *item, qreal oldR,
                                       qreal newR)
{
    m_undoStack->push(new PropertyChangeCommand(
        item, PropertyChangeCommand::CornerRadius, QVariant(oldR),
        QVariant(newR), m_pView->scene()));
}

void MainWindow::onPositionChanged(QGraphicsItem *item, const QPointF &oldPos,
                                   const QPointF &newPos)
{
    m_undoStack->push(
        new PositionChangeCommand(item, oldPos, newPos, m_pView->scene()));
}

void MainWindow::onRotationChanged(QGraphicsItem *item, qreal oldRotation,
                                   qreal newRotation)
{
    m_undoStack->push(new RotationChangeCommand(item, oldRotation, newRotation,
                                                m_pView->scene()));
    // 旋转后需要更新 ResizeHandleItem 以正确显示选中框
    m_pView->scheduleResizeHandleUpdate();
}

void MainWindow::onRequestFinished(const QJsonDocument &json,
                                   NetworkRequestType type)
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
        selected.isEmpty() ? ::filterSelectableItems(m_pView->scene()->items())
                           : selected;
    if (items.isEmpty())
        return;

    // 计算所有目标图元的场景包围矩形并集
    QRectF unitedRect;
    for (auto *item : items) {
        QRectF itemSceneRect =
            item->mapToScene(item->boundingRect()).boundingRect();
        unitedRect = unitedRect.isValid() ? unitedRect.united(itemSceneRect)
                                          : itemSceneRect;
    }

    if (!unitedRect.isValid() || unitedRect.width() < 1
        || unitedRect.height() < 1)
        return;

    // 计算偏移量：若图元在负坐标，整体平移到正坐标区域
    qreal offsetX = unitedRect.left() < 0 ? -unitedRect.left() : 0;
    qreal offsetY = unitedRect.top() < 0 ? -unitedRect.top() : 0;

    // 画布从 (0,0) 开始，尺寸需覆盖所有图元的最大延伸
    QSizeF newSize(unitedRect.right() + offsetX, unitedRect.bottom() + offsetY);
    QSizeF oldSize = canvas->canvasSize();

    if (newSize.width() <= 0 || newSize.height() <= 0)
        return;

    m_undoStack->beginMacro(tr("Fit Canvas to Selection"));

    // 若有负坐标图元，先平移
    if (offsetX > 0 || offsetY > 0) {
        QPointF delta(offsetX, offsetY);
        QList<QPointF> oldPositions, newPositions;
        for (auto *item : items) {
            oldPositions << item->pos();
            newPositions << item->pos() + delta;
            item->setPos(item->pos() + delta);
        }
        m_undoStack->push(new MoveItemsCommand(items, oldPositions,
                                               newPositions, m_pView->scene()));
    }

    // 画布尺寸变更
    if (oldSize != newSize)
        m_undoStack->push(new CanvasResizeCommand(canvas, oldSize, newSize,
                                                  m_pView->scene()));

    m_undoStack->endMacro();
    _updateCanvasLabel();
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
        m_undoStack->push(new RotationChangeCommand(
            item, oldRotation, newRotation, m_pView->scene()));
    } else {
        // 多个图元：绕组中心整体旋转
        // 1. 计算组中心（所有图元场景包围矩形的并集中心）
        QRectF groupSceneRect;
        for (auto *item : items) {
            QRectF itemSceneRect =
                item->mapToScene(item->boundingRect()).boundingRect();
            groupSceneRect = groupSceneRect.united(itemSceneRect);
        }
        QPointF groupCenter = groupSceneRect.center();

        // 2. 对每个图元：先绕自身中心旋转，再绕组中心公转
        m_undoStack->beginMacro(
            tr("Rotate %1\u00b0").arg(angleDelta, 0, 'f', 0));

        QList<QGraphicsItem *> moveItems;
        QList<QPointF> oldPositions;
        QList<QPointF> newPositions;

        for (auto *item : items) {
            qreal oldRotation = item->rotation();
            qreal newRotation = oldRotation + angleDelta;

            // 推入旋转命令（含中心补偿：旋转后图元中心位置不变）
            m_undoStack->push(new RotationChangeCommand(
                item, oldRotation, newRotation, m_pView->scene()));

            // 旋转命令 redo 后，图元中心仍位于旋转前的场景位置
            // 记录中心补偿后的位置（公转前的位置）
            QPointF posAfterCenterComp = item->pos();

            // 计算绕组中心的公转位移
            QPointF currentCenter =
                item->mapToScene(item->boundingRect().center());
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
            m_undoStack->push(new MoveItemsCommand(
                moveItems, oldPositions, newPositions, m_pView->scene()));
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
        out << static_cast<int>(gi->itemType());
        gi->serialize(out);
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
        qWarning("Unsupported clipboard format version: %d (current: %d)",
                 version, IGraphicsItem::kSerializationVersion);
        return result;
    }

    int count = 0;
    in >> count;
    for (int i = 0; i < count; ++i) {
        int typeInt = 0;
        in >> typeInt;
        auto *gi =
            createItemByType(static_cast<IGraphicsItem::ItemType>(typeInt));
        if (!gi)
            continue;
        if (!gi->deserialize(in)) {
            qWarning("Failed to deserialize item type %d (stream corrupted)",
                     typeInt);
            delete gi;
            continue;
        }
        result << dynamic_cast<QGraphicsItem *>(gi);
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
        fileEditBar->setVisible(
            settings.value("toolbar/FileEditToolBar_visible").toBool());
    }
    if (drawBar && settings.contains("toolbar/DrawingToolBar_visible")) {
        drawBar->setVisible(
            settings.value("toolbar/DrawingToolBar_visible").toBool());
    }
    if (alignToolBar && settings.contains("toolbar/AlignToolBar_visible")) {
        alignToolBar->setVisible(
            settings.value("toolbar/AlignToolBar_visible").toBool());
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
        settings.setValue("toolbar/FileEditToolBar_visible",
                          fileEditBar->isVisible());
    }
    if (drawBar) {
        settings.setValue("toolbar/DrawingToolBar_visible",
                          drawBar->isVisible());
    }
    if (alignToolBar) {
        settings.setValue("toolbar/AlignToolBar_visible",
                          alignToolBar->isVisible());
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

    QMainWindow::closeEvent(event);
}