#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "AlignLayoutDialog.h"
#include "AlignmentUtils.h"
#include "BezierCurveItem.h"
#include "CanvasItem.h"
#include "Commands.h"
#include "EllipseItem.h"
#include "ExportImageDialog.h"
#include "ImageItem.h"
#include "ImageUtils.h"
#include "ImportImageDialog.h"
#include "LineItem.h"
#include "NewFileDialog.h"
#include "RectItem.h"
#include "ResizeHandleItem.h"
#include "RulerBar.h"
#include "TextItem.h"

#include <algorithm>
#include <QDateTime>

#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDataStream>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QImageWriter>
#include <QImage>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMap>
#include <QMimeData>
#include <QPainter>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QColorSpace>
#include <tiff.h>
#include <tiffio.h>

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

    // 加载 QSS 样式表
    loadStyleSheet();

    setWindowTitle(tr("GraphicsDemo"));
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
    // 先断开场景信号，防止析构期间信号触发访问半销毁状态的对象
    if (m_pView && m_pView->scene())
        disconnect(m_pView->scene(), &QGraphicsScene::selectionChanged, this,
                   &MainWindow::onSelectionChanged);

    // 清空属性面板引用，避免悬空指针
    if (m_pPropertyPanel)
        m_pPropertyPanel->setItem(nullptr);

    // 清空 undo 栈，避免命令引用已销毁的图元
    if (m_undoStack)
        m_undoStack->clear();

    delete ui;
}

void MainWindow::_initWidget()
{
    m_pView = new QAtGraphicsView(this);
    m_pView->setUndoStack(m_undoStack);
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

    QAction *newAct = fileMenu->addAction(QIcon(":/icons/icons/file-new.svg"), tr("&New..."));
    newAct->setShortcut(QKeySequence::New);
    connect(newAct, &QAction::triggered, this, &MainWindow::onNew);

    fileMenu->addSeparator();

    QAction *importAct =
        fileMenu->addAction(QIcon(":/icons/icons/file-import.svg"), tr("&Import Image..."));
    importAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
    connect(importAct, &QAction::triggered, this, &MainWindow::onImportImage);

    QAction *exportAct =
        fileMenu->addAction(QIcon(":/icons/icons/file-export.svg"), tr("&Export Image..."));
    exportAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(exportAct, &QAction::triggered, this, &MainWindow::onExportImage);

    fileMenu->addSeparator();

    QAction *exitAct = fileMenu->addAction(tr("E&xit"));
    exitAct->setShortcut(QKeySequence::Quit);
    connect(exitAct, &QAction::triggered, this, &QWidget::close);

    // ---- 编辑 ----
    QMenu *editMenu = menu->addMenu(tr("&Edit"));

    m_undoAction = editMenu->addAction(QIcon(":/icons/icons/edit-undo.svg"), tr("&Undo"));
    m_undoAction->setShortcut(QKeySequence::Undo);
    connect(m_undoAction, &QAction::triggered, this, &MainWindow::onUndo);

    m_redoAction = editMenu->addAction(QIcon(":/icons/icons/edit-redo.svg"), tr("&Redo"));
    m_redoAction->setShortcut(QKeySequence::Redo);
    connect(m_redoAction, &QAction::triggered, this, &MainWindow::onRedo);

    editMenu->addSeparator();

    QAction *cutAct = editMenu->addAction(QIcon(":/icons/icons/edit-cut.svg"), tr("Cu&t"));
    cutAct->setShortcut(QKeySequence::Cut);
    connect(cutAct, &QAction::triggered, this, &MainWindow::onCut);

    QAction *copyAct = editMenu->addAction(QIcon(":/icons/icons/edit-copy.svg"), tr("&Copy"));
    copyAct->setShortcut(QKeySequence::Copy);
    connect(copyAct, &QAction::triggered, this, &MainWindow::onCopy);

    QAction *pasteAct = editMenu->addAction(QIcon(":/icons/icons/edit-paste.svg"), tr("&Paste"));
    pasteAct->setShortcut(QKeySequence::Paste);
    connect(pasteAct, &QAction::triggered, this, &MainWindow::onPaste);

    editMenu->addSeparator();

    QAction *deleteAct = editMenu->addAction(QIcon(":/icons/icons/edit-delete.svg"), tr("&Delete"));
    deleteAct->setShortcut(QKeySequence::Delete);
    connect(deleteAct, &QAction::triggered, this, &MainWindow::onDelete);

    QAction *selectAllAct =
        editMenu->addAction(QIcon(":/icons/icons/edit-select-all.svg"), tr("Select &All"));
    selectAllAct->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAct, &QAction::triggered, this, &MainWindow::onSelectAll);

    // ---- 排列 ----
    QMenu *arrMenu = menu->addMenu(tr("&Arrange"));
    arrMenu->addAction(QIcon(":/icons/icons/bring-front.svg"), tr("Bring to Front"), this,
                       &MainWindow::onBringToFront);
    arrMenu->addAction(QIcon(":/icons/icons/send-back.svg"), tr("Send to Back"), this,
                       &MainWindow::onSendToBack);
    arrMenu->addSeparator();
    QMenu *alignMenu = arrMenu->addMenu(tr("Align"));
    alignMenu->addAction(tr("Left"), this, &MainWindow::onAlignLeft);
    alignMenu->addAction(tr("Right"), this, &MainWindow::onAlignRight);
    alignMenu->addAction(tr("Top"), this, &MainWindow::onAlignTop);
    alignMenu->addAction(tr("Bottom"), this, &MainWindow::onAlignBottom);
    alignMenu->addAction(tr("Center H"), this, &MainWindow::onAlignHCenter);
    alignMenu->addAction(tr("Center V"), this, &MainWindow::onAlignVCenter);
    QMenu *distMenu = arrMenu->addMenu(tr("Distribute"));
    distMenu->addAction(tr("Horizontally"), this, &MainWindow::onDistributeH);
    distMenu->addAction(tr("Vertically"), this, &MainWindow::onDistributeV);
    distMenu->addSeparator();
    distMenu->addAction(tr("Align && Layout..."), this, &MainWindow::onAlignLayoutDialog);
    arrMenu->addSeparator();
    QMenu *rotateMenu = arrMenu->addMenu(tr("Rotate"));
    rotateMenu->addAction(QIcon(":/icons/icons/rotate-cw.svg"), tr("90\u00b0 Clockwise"), this,
                          [this]() { rotateSelectedItems(90.0); });
    rotateMenu->addAction(QIcon(":/icons/icons/rotate-ccw.svg"), tr("90\u00b0 Counter-clockwise"),
                          this, [this]() { rotateSelectedItems(-90.0); });
    rotateMenu->addAction(tr("180\u00b0"), this, [this]() { rotateSelectedItems(180.0); });

    // ---- 视图 ----
    QMenu *viewMenu = menu->addMenu(tr("&View"));
    viewMenu->addAction(m_pPropertyPanel->toggleViewAction());
    viewMenu->addSeparator();

    // 网格显示/隐藏
    m_gridAction = viewMenu->addAction(QIcon(":/icons/icons/view-grid.svg"), tr("Show Grid"));
    m_gridAction->setCheckable(true);
    m_gridAction->setChecked(true);
    connect(m_gridAction, &QAction::toggled, this,
            [this](bool checked) { m_pView->setGridVisible(checked); });

    // 刻度尺单位切换（px ↔ mm）
    m_rulerUnitAction = viewMenu->addAction(tr("Ruler Unit: mm"));
    m_rulerUnitAction->setCheckable(true);
    connect(m_rulerUnitAction, &QAction::toggled, this, [this](bool checked) {
        auto unit = checked ? RulerBar::Millimeter : RulerBar::Pixel;
        m_hRuler->setUnit(unit);
        m_vRuler->setUnit(unit);
        m_rulerUnitAction->setText(checked ? tr("Ruler Unit: mm") : tr("Ruler Unit: px"));
        // 同步更新状态栏坐标和画布尺寸的单位显示
        _updateCanvasLabel();
        _updatePosLabel(m_lastScenePos);
    });

    viewMenu->addSeparator();
    // 缩放适配
    QAction *fitAct = new QAction(QIcon(":/icons/icons/view-fit.svg"), tr("Fit to Canvas"), this);
    connect(fitAct, &QAction::triggered, this, [this]() { m_pView->fitToCanvas(); });
    viewMenu->addAction(fitAct);

    QAction *resetZoomAct =
        new QAction(QIcon(":/icons/icons/view-zoom-reset.svg"), tr("Reset Zoom (Ctrl+0)"), this);
    resetZoomAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(resetZoomAct, &QAction::triggered, this, [this]() { m_pView->setZoomLevel(1.0); });
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
    QAction *newAct = new QAction(QIcon(":/icons/icons/file-new.svg"), tr("New"), this);
    connect(newAct, &QAction::triggered, this, &MainWindow::onNew);
    fileEditBar->addAction(newAct);

    // Import 按钮
    QAction *importAct =
        new QAction(QIcon(":/icons/icons/file-import.svg"), tr("Import Image"), this);
    connect(importAct, &QAction::triggered, this, &MainWindow::onImportImage);
    fileEditBar->addAction(importAct);

    // Export 按钮
    QAction *exportAct =
        new QAction(QIcon(":/icons/icons/file-export.svg"), tr("Export Image"), this);
    connect(exportAct, &QAction::triggered, this, &MainWindow::onExportImage);
    fileEditBar->addAction(exportAct);

    fileEditBar->addSeparator();

    fileEditBar->addAction(m_undoAction);
    fileEditBar->addAction(m_redoAction);

    fileEditBar->addSeparator();

    // Cut 按钮
    QAction *cutAct = new QAction(QIcon(":/icons/icons/edit-cut.svg"), tr("Cut"), this);
    cutAct->setShortcut(QKeySequence::Cut);
    connect(cutAct, &QAction::triggered, this, &MainWindow::onCut);
    fileEditBar->addAction(cutAct);

    // Copy 按钮
    QAction *copyAct = new QAction(QIcon(":/icons/icons/edit-copy.svg"), tr("Copy"), this);
    copyAct->setShortcut(QKeySequence::Copy);
    connect(copyAct, &QAction::triggered, this, &MainWindow::onCopy);
    fileEditBar->addAction(copyAct);

    // Paste 按钮
    QAction *pasteAct = new QAction(QIcon(":/icons/icons/edit-paste.svg"), tr("Paste"), this);
    pasteAct->setShortcut(QKeySequence::Paste);
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
                             const QString &shortcut = {}) {
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
    m_alignToolBar = new QToolBar(tr("Align"), this);
    m_alignToolBar->setObjectName("AlignToolBar");
    m_alignToolBar->setMovable(false);
    m_alignToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_alignToolBar->setIconSize(QSize(20, 20));
    addToolBar(Qt::TopToolBarArea, m_alignToolBar);

    QAction *alignLayoutAct = m_alignToolBar->addAction(QIcon(":/icons/icons/align-layout.svg"),
                                                        tr("Align && Layout..."));
    alignLayoutAct->setToolTip(tr("Open Align & Layout dialog"));
    connect(alignLayoutAct, &QAction::triggered, this, &MainWindow::onAlignLayoutDialog);

    m_alignToolBar->addSeparator();

    // 顺时针旋转 90°
    QAction *rotateCWAct =
        m_alignToolBar->addAction(QIcon(":/icons/icons/rotate-cw.svg"), tr("Rotate 90\u00b0 CW"));
    rotateCWAct->setToolTip(tr("Rotate 90\u00b0 clockwise"));
    connect(rotateCWAct, &QAction::triggered, this, [this]() { rotateSelectedItems(90.0); });

    // 逆时针旋转 90°
    QAction *rotateCCWAct =
        m_alignToolBar->addAction(QIcon(":/icons/icons/rotate-ccw.svg"), tr("Rotate 90\u00b0 CCW"));
    rotateCCWAct->setToolTip(tr("Rotate 90\u00b0 counter-clockwise"));
    connect(rotateCCWAct, &QAction::triggered, this, [this]() { rotateSelectedItems(-90.0); });
}

void MainWindow::_initPropertyPanel()
{
    m_pPropertyPanel = new PropertyPanel(this);
    addDockWidget(Qt::RightDockWidgetArea, m_pPropertyPanel);
}

void MainWindow::_initConnections()
{
    connect(m_pView, &QAtGraphicsView::selectionChanged, this, &MainWindow::onSelectionChanged);
    connect(m_pView, &QAtGraphicsView::itemAdded, this, &MainWindow::onItemAdded);

    // 右键菜单 → 复用菜单栏的 Bring to Front / Send to Back
    connect(m_pView, &QAtGraphicsView::bringToFrontRequested,
            this, &MainWindow::onBringToFront);
    connect(m_pView, &QAtGraphicsView::sendToBackRequested,
            this, &MainWindow::onSendToBack);

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
    connect(m_undoStack, &QUndoStack::indexChanged, this,
            [this]() { m_pView->refreshResizeHandle(); });

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
    m_posLabel = new QLabel(tr("X: 0.0 px  Y: 0.0 px"));
    m_posLabel->setMinimumWidth(220);

    // 缩放标签
    m_zoomLabel = new QLabel(tr("100%"));
    m_zoomLabel->setMinimumWidth(60);
    m_zoomLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // 缩放滑块
    m_zoomSlider = new QSlider(Qt::Horizontal);
    m_zoomSlider->setRange(10, 500);
    m_zoomSlider->setValue(100);
    m_zoomSlider->setFixedWidth(120);
    m_zoomSlider->setTickPosition(QSlider::TicksBelow);
    m_zoomSlider->setTickInterval(50);
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
    bar->addPermanentWidget(m_zoomLabel);
    bar->addPermanentWidget(m_zoomSlider);
    bar->addPermanentWidget(createStatusSeparator(bar));
    bar->addPermanentWidget(m_canvasLabel);
    bar->addPermanentWidget(m_toolLabel);
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
        m_posLabel->setText(tr("X: %1 mm  Y: %2 mm").arg(xmm, 0, 'f', 1).arg(ymm, 0, 'f', 1));
    } else {
        m_posLabel->setText(
            tr("X: %1 px  Y: %2 px").arg(scenePos.x(), 0, 'f', 1).arg(scenePos.y(), 0, 'f', 1));
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

    // 设置画布 PPI
    if (m_pView->canvasItem())
        m_pView->canvasItem()->setPpi(ppi);

    // 同步刻度尺 PPI
    m_hRuler->setPpi(ppi);
    m_vRuler->setPpi(ppi);

    // PPI 变化后刷新刻度尺和状态栏
    m_hRuler->updateRuler();
    m_vRuler->updateRuler();
    _updateCanvasLabel();
    _updatePosLabel(m_lastScenePos);
}

void MainWindow::onImportImage()
{
    auto result = ImageUtils::importImageWithDialog(this);
    if (!result.isValid())
        return;

    auto *item = new ImageItem(QPixmap::fromImage(result.image));
    item->setItemPen(QPen(Qt::NoPen));
    item->setFilePath(result.filePath);
    item->setRawTiffData(result.rawTiffData);

    m_undoStack->push(new AddItemCommand(m_pView->scene(), item));
}

void MainWindow::onExportImage()
{
    // 第一步：选择保存路径和格式
    QString path = QFileDialog::getSaveFileName(
        this, tr("Export Image"), QString(),
        tr("TIFF (*.tif *.tiff);;PNG (*.png);;JPEG (*.jpg);;BMP (*.bmp)"));
    if (path.isEmpty())
        return;

    // 第二步：显示导出参数对话框
    ImageUtils::ExportParameters exportParams;
    ImageUtils::ExportImageDialog dialog(this, exportParams,
                                           QFileInfo(path).suffix());
    if (dialog.exec() != QDialog::Accepted)
        return;
    exportParams = dialog.getParameters();

    // 第三步：根据画布或场景内容创建图像
    QRectF exportRect;
    if (m_pView->canvasItem()) {
        exportRect = m_pView->canvasItem()->rect();
    } else {
        exportRect = m_pView->scene()->itemsBoundingRect().adjusted(-10, -10, 10, 10);
    }

    QImage image(exportRect.size().toSize(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    m_pView->scene()->render(&painter, QRectF(), exportRect);
    painter.end();

    // 第四步：根据参数导出
    if (path.endsWith(".tif", Qt::CaseInsensitive) || path.endsWith(".tiff", Qt::CaseInsensitive)) {
        exportTiffLossless(path, image, exportParams);
    } else if (path.endsWith(".png", Qt::CaseInsensitive)) {
        // PNG 导出（应用压缩级别）
        // Qt 的 QImage::save() 不支持直接设置 PNG 压缩级别
        // 需要通过 QImageWriter 设置
        exportImageWithParams(path, image, exportParams);
    } else if (path.endsWith(".jpg", Qt::CaseInsensitive) || path.endsWith(".jpeg", Qt::CaseInsensitive)) {
        // JPEG 导出（应用质量参数）
        exportImageWithParams(path, image, exportParams);
    } else {
        // BMP 等其他格式
        image.save(path);
    }
}

// 写入 TIFF 元数据
static void writeTiffMetadata(TIFF *tif, const ImageUtils::ExportParameters &params);

// 应用颜色空间转换（导出时）
static void applyColorSpaceForExport(QImage &image, ImageUtils::ExportParameters::ColorSpace colorSpace);

// ============================================================
// TIFF 导出（使用 libtiff，支持压缩参数）
// ============================================================
bool MainWindow::exportTiffLossless(const QString &path, const QImage &image,
                                     const ImageUtils::ExportParameters &params)
{
    QImage img = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    TIFF *tif = TIFFOpen(path.toUtf8().constData(), "w");
    if (!tif)
        return false;

    int width = img.width();
    int height = img.height();

    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);

    // 根据参数设置压缩类型
    uint16_t compression = COMPRESSION_NONE;
    switch (params.compression) {
    case ImageUtils::ExportParameters::CompressionType::None:
        compression = COMPRESSION_NONE;
        break;
    case ImageUtils::ExportParameters::CompressionType::LZW:
        compression = COMPRESSION_LZW;
        break;
    case ImageUtils::ExportParameters::CompressionType::ZIP:
        compression = COMPRESSION_ADOBE_DEFLATE;
        break;
    case ImageUtils::ExportParameters::CompressionType::JPEG:
        compression = COMPRESSION_JPEG;
        TIFFSetField(tif, TIFFTAG_JPEGQUALITY, params.jpegQuality);
        break;
    }
    TIFFSetField(tif, TIFFTAG_COMPRESSION, compression);

    // 如果启用了ZIP水平差分
    if (params.compression == ImageUtils::ExportParameters::CompressionType::ZIP &&
        params.tiffZipHorizontalDifferencing) {
        TIFFSetField(tif, TIFFTAG_PREDICTOR, PREDICTOR_HORIZONTAL);
    }

    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 4); // RGBA
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, 1);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

    // 设置 DPI
    TIFFSetField(tif, TIFFTAG_XRESOLUTION, static_cast<float>(params.dpi));
    TIFFSetField(tif, TIFFTAG_YRESOLUTION, static_cast<float>(params.dpi));
    TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);

    // 写入元数据（如果启用）
    if (params.preserveMetadata) {
        writeTiffMetadata(tif, params);
    }

    // 转换 ARGB32 → RGBA
    QVector<uint8_t> rgbaRow(width * 4);
    bool writeError = false;
    for (int y = 0; y < height; ++y) {
        const QRgb *scanLine = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        for (int x = 0; x < width; ++x) {
            QRgb c = scanLine[x];
            rgbaRow[x * 4 + 0] = qRed(c);
            rgbaRow[x * 4 + 1] = qGreen(c);
            rgbaRow[x * 4 + 2] = qBlue(c);
            rgbaRow[x * 4 + 3] = qAlpha(c);
        }
        if (TIFFWriteScanline(tif, rgbaRow.data(), y) < 0) {
            qWarning("TIFFWriteScanline failed at row %d", y);
            writeError = true;
            break;
        }
    }

    TIFFClose(tif);
    return !writeError;
}

// 写入 TIFF 元数据
static void writeTiffMetadata(TIFF *tif, const ImageUtils::ExportParameters &params)
{
    Q_UNUSED(params);

    // 写入软件信息
    QString software = "GraphicsDemo";
    TIFFSetField(tif, TIFFTAG_SOFTWARE, software.toUtf8().constData());

    // 写入日期时间
    QString dateTime = QDateTime::currentDateTime().toString("yyyy:MM:dd HH:mm:ss");
    TIFFSetField(tif, TIFFTAG_DATETIME, dateTime.toUtf8().constData());

    // 可以在这里添加更多元数据写入
    // 例如：作者、版权、图像描述等
}

// ============================================================
// 使用参数导出图像（PNG/JPEG 等格式）
// ============================================================
bool MainWindow::exportImageWithParams(const QString &path, const QImage &image,
                                      const ImageUtils::ExportParameters &params)
{
    QImage img = image;

    // 处理透明度
    if (params.transparency == ImageUtils::ExportParameters::TransparencyHandling::FlattenOnWhite) {
        QImage flattened(img.size(), QImage::Format_ARGB32_Premultiplied);
        flattened.fill(Qt::white);
        QPainter painter(&flattened);
        painter.drawImage(0, 0, img);
        painter.end();
        img = flattened;
    }

    // 处理颜色空间转换
    applyColorSpaceForExport(img, params.colorSpace);

    // 使用 QImageWriter 设置参数
    QImageWriter writer(path);
    if (!writer.canWrite()) {
        qWarning("Cannot write image format: %s", qPrintable(path));
        return false;
    }

    // 应用导出参数（压缩等）
    ImageUtils::applyExportParameters(writer, params);

    // 注意：QImageWriter 在 Qt 5.15 中可能不支持直接设置 DPI
    // 对于 TIFF，已在 exportTiffLossless() 中通过 libtiff 设置 DPI
    // 对于 PNG/JPEG，DPI 信息可能通过元数据存储

    // 保留元数据（如果启用）
    if (params.preserveMetadata) {
        // QImageWriter 会自动保留一些元数据
        // 可以在这里添加更多元数据写入逻辑
    }

    return writer.write(img);
}

// 应用颜色空间转换（导出时）
static void applyColorSpaceForExport(QImage &image, ImageUtils::ExportParameters::ColorSpace colorSpace)
{
    if (colorSpace == ImageUtils::ExportParameters::ColorSpace::KeepOriginal)
        return;

    // 使用 Qt 的 QColorSpace 进行颜色空间转换
    QColorSpace targetSpace;
    switch (colorSpace) {
    case ImageUtils::ExportParameters::ColorSpace::ConvertToSRGB:
        targetSpace = QColorSpace::SRgb;
        break;
    case ImageUtils::ExportParameters::ColorSpace::ConvertToAdobeRGB:
        targetSpace = QColorSpace(QColorSpace::Primaries::AdobeRgb, QColorSpace::TransferFunction::Gamma, 2.2);
        break;
    default:
        return;
    }

    if (image.colorSpace() != targetSpace) {
        image.convertToColorSpace(targetSpace);
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

    // 过滤掉 CanvasItem 和 ResizeHandleItem
    QList<QGraphicsItem *> deletable;
    for (auto *item : items) {
        if (qgraphicsitem_cast<CanvasItem *>(item))
            continue;
        if (item->type() == ResizeHandleItem::Type)
            continue;
        deletable << item;
    }
    if (deletable.isEmpty())
        return;

    m_undoStack->push(new RemoveItemsCommand(m_pView->scene(), deletable));
}

void MainWindow::onSelectAll()
{
    auto items = m_pView->scene()->items();
    for (auto *item : items) {
        if (!qgraphicsitem_cast<CanvasItem *>(item) && item->type() != ResizeHandleItem::Type)
            item->setSelected(true);
    }
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
    qreal maxZ = 0;
    for (auto *item : m_pView->scene()->items())
        maxZ = qMax(maxZ, item->zValue());

    for (auto *item : items) {
        oldZ << item->zValue();
        newZ << (++maxZ);
    }
    m_undoStack->push(new ZValueChangeCommand(items, oldZ, newZ, m_pView->scene()));
}

void MainWindow::onSendToBack()
{
    auto items = m_pView->scene()->selectedItems();
    if (items.isEmpty())
        return;

    QList<qreal> oldZ, newZ;
    qreal minZ = 0;
    for (auto *item : m_pView->scene()->items())
        minZ = qMin(minZ, item->zValue());

    for (auto *item : items) {
        oldZ << item->zValue();
        newZ << (--minZ);
    }
    m_undoStack->push(new ZValueChangeCommand(items, oldZ, newZ, m_pView->scene()));
}

// ============================================================
// 对齐与布局对话框
// ============================================================
void MainWindow::onAlignLayoutDialog()
{
    if (!m_alignLayoutDlg) {
        m_alignLayoutDlg = new AlignLayoutDialog(m_pView->scene(), m_undoStack, this);
        m_alignLayoutDlg->setAttribute(Qt::WA_DeleteOnClose);
        // 关闭后清空指针，下次重新创建
        connect(m_alignLayoutDlg, &QObject::destroyed, this,
                [this]() { m_alignLayoutDlg = nullptr; });
    }
    m_alignLayoutDlg->refreshSelectionInfo();
    m_alignLayoutDlg->show();
    m_alignLayoutDlg->raise();
    m_alignLayoutDlg->activateWindow();
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
    // 过滤掉 CanvasItem 和 ResizeHandleItem
    QList<QGraphicsItem *> selectable;
    for (auto *item : items) {
        if (!qgraphicsitem_cast<CanvasItem *>(item) && item->type() != ResizeHandleItem::Type)
            selectable << item;
    }

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
}

void MainWindow::onRotationChanged(QGraphicsItem *item, qreal oldRotation, qreal newRotation)
{
    m_undoStack->push(new RotationChangeCommand(item, oldRotation, newRotation, m_pView->scene()));
    // 旋转后需要更新 ResizeHandleItem 以正确显示选中框
    m_pView->scheduleResizeHandleUpdate();
}

// ============================================================
// 批量旋转选中图元 — 绕图元中心(单选)或组中心(多选)旋转
// ============================================================
void MainWindow::rotateSelectedItems(qreal angleDelta)
{
    auto items = filterSelectableItems();
    if (items.isEmpty())
        return;

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
    auto items = m_pView->scene()->selectedItems();
    QList<QGraphicsItem *> result;
    for (auto *item : items) {
        if (!qgraphicsitem_cast<CanvasItem *>(item) && item->type() != ResizeHandleItem::Type)
            result << item;
    }
    return result;
}

void MainWindow::copyItemsToClipboard(const QList<QGraphicsItem *> &items)
{
    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);

    // 写入序列化版本号
    out << IGraphicsItem::kSerializationVersion;

    int count = 0;
    for (auto *item : items) {
        if (qgraphicsitem_cast<CanvasItem *>(item))
            continue;
        if (item->type() == ResizeHandleItem::Type)
            continue;
        count++;
    }

    out << count;
    for (auto *item : items) {
        auto *gi = dynamic_cast<IGraphicsItem *>(item);
        if (!gi)
            continue;
        if (qgraphicsitem_cast<CanvasItem *>(item))
            continue;
        if (item->type() == ResizeHandleItem::Type)
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
        qWarning("Unsupported clipboard format version: %d (current: %d)", version,
                 IGraphicsItem::kSerializationVersion);
        return result;
    }

    int count = 0;
    in >> count;
    for (int i = 0; i < count; ++i) {
        int typeInt = 0;
        in >> typeInt;
        auto *gi = createItemByType(static_cast<IGraphicsItem::ItemType>(typeInt));
        if (!gi)
            continue;
        if (!gi->deserialize(in)) {
            qWarning("Failed to deserialize item type %d (stream corrupted)", typeInt);
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

    // 先断开场景信号，防止析构期间信号触发访问半销毁状态的对象
    if (m_pView && m_pView->scene())
        disconnect(m_pView->scene(), &QGraphicsScene::selectionChanged, this,
                   &MainWindow::onSelectionChanged);

    // 清空属性面板引用，避免悬空指针
    if (m_pPropertyPanel)
        m_pPropertyPanel->setItem(nullptr);

    // 清空 undo 栈，避免命令引用已销毁的图元
    if (m_undoStack)
        m_undoStack->clear();

    QMainWindow::closeEvent(event);
}