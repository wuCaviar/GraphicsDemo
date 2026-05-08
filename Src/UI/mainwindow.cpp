#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "RectItem.h"
#include "EllipseItem.h"
#include "LineItem.h"
#include "BezierCurveItem.h"
#include "TextItem.h"
#include "ImageItem.h"
#include "CanvasItem.h"
#include "ResizeHandleItem.h"
#include "Commands.h"
#include "NewFileDialog.h"
#include "RulerBar.h"
#include "AlignLayoutDialog.h"
#include "AlignmentUtils.h"
#include "ImageUtils.h"

#include <algorithm>

#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QDataStream>
#include <QFile>
#include <QFileDialog>
#include <QImage>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMap>
#include <QMimeData>
#include <QPainter>
#include <QScrollBar>
#include <QShortcut>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <tiff.h>
#include <tiffio.h>

static const char *kMimeFormat = "application/x-graphicsdemo-items";

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
    fileMenu->addAction(QIcon(":/icons/file-new.svg"), tr("&New..."), QKeySequence::New, this, &MainWindow::onNew);
    fileMenu->addSeparator();
    fileMenu->addAction(QIcon(":/icons/file-import.svg"), tr("&Import Image..."), QKeySequence(Qt::CTRL | Qt::Key_I),
                        this, &MainWindow::onImportImage);
    fileMenu->addAction(QIcon(":/icons/file-export.svg"), tr("&Export Image..."), QKeySequence(Qt::CTRL | Qt::Key_E),
                        this, &MainWindow::onExportImage);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);

    // ---- 编辑 ----
    QMenu *editMenu = menu->addMenu(tr("&Edit"));
    m_undoAction = editMenu->addAction(QIcon(":/icons/edit-undo.svg"), tr("&Undo"), QKeySequence::Undo, this, &MainWindow::onUndo);
    m_redoAction = editMenu->addAction(QIcon(":/icons/edit-redo.svg"), tr("&Redo"), QKeySequence::Redo, this, &MainWindow::onRedo);
    editMenu->addSeparator();
    editMenu->addAction(QIcon(":/icons/edit-cut.svg"), tr("Cu&t"), QKeySequence::Cut, this, &MainWindow::onCut);
    editMenu->addAction(QIcon(":/icons/edit-copy.svg"), tr("&Copy"), QKeySequence::Copy, this, &MainWindow::onCopy);
    editMenu->addAction(QIcon(":/icons/edit-paste.svg"), tr("&Paste"), QKeySequence::Paste, this, &MainWindow::onPaste);
    editMenu->addSeparator();
    editMenu->addAction(QIcon(":/icons/edit-delete.svg"), tr("&Delete"), QKeySequence::Delete, this, &MainWindow::onDelete);
    editMenu->addAction(QIcon(":/icons/edit-select-all.svg"), tr("Select &All"), QKeySequence::SelectAll, this, &MainWindow::onSelectAll);

    // ---- 排列 ----
    QMenu *arrMenu = menu->addMenu(tr("&Arrange"));
    arrMenu->addAction(QIcon(":/icons/bring-front.svg"), tr("Bring to Front"), this, &MainWindow::onBringToFront);
    arrMenu->addAction(QIcon(":/icons/send-back.svg"), tr("Send to Back"), this, &MainWindow::onSendToBack);
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
    rotateMenu->addAction(QIcon(":/icons/rotate-cw.svg"), tr("90\u00b0 Clockwise"), this, [this]() { rotateSelectedItems(90.0); });
    rotateMenu->addAction(QIcon(":/icons/rotate-ccw.svg"), tr("90\u00b0 Counter-clockwise"), this, [this]() { rotateSelectedItems(-90.0); });
    rotateMenu->addAction(tr("180\u00b0"), this, [this]() { rotateSelectedItems(180.0); });

    // ---- 视图 ----
    QMenu *viewMenu = menu->addMenu(tr("&View"));
    viewMenu->addAction(m_pPropertyPanel->toggleViewAction());
    viewMenu->addSeparator();

    // 网格显示/隐藏
    m_gridAction = viewMenu->addAction(QIcon(":/icons/view-grid.svg"), tr("Show Grid"));
    m_gridAction->setCheckable(true);
    m_gridAction->setChecked(true);
    connect(m_gridAction, &QAction::toggled, this, [this](bool checked) {
        m_pView->setGridVisible(checked);
    });

    viewMenu->addSeparator();
    // 缩放适配
    viewMenu->addAction(QIcon(":/icons/view-fit.svg"), tr("Fit to Canvas"), this, [this]() {
        if (m_pView->canvasItem()) {
            m_pView->fitInView(m_pView->canvasItem()->rect(), Qt::KeepAspectRatio);
            m_pView->scale(0.9, 0.9); // 稍微缩小一点留边
        }
    });
    viewMenu->addAction(QIcon(":/icons/view-zoom-reset.svg"), tr("Reset Zoom (Ctrl+0)"), QKeySequence(Qt::CTRL | Qt::Key_0),
                        this, [this]() { m_pView->setZoomLevel(1.0); });

    _updateUndoRedoActions();
}

void MainWindow::_initToolBar()
{
    // 绘图工具栏
    QToolBar *drawBar = addToolBar(tr("Drawing Tools"));
    drawBar->setMovable(false);
    drawBar->setObjectName("DrawingToolBar");
    drawBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    drawBar->setIconSize(QSize(20, 20));

    auto *actionGroup = new QActionGroup(this);
    actionGroup->setExclusive(true);

    auto addToolAction = [&](const QString &iconPath, const QString &text, Tool tool, const QString &shortcut = {}) {
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

    auto *selectAct = addToolAction(":/icons/tool-select.svg", tr("Select (V)"), Tool::Select, "V");
    selectAct->setChecked(true);
    addToolAction(":/icons/tool-rect.svg", tr("Rectangle (R)"), Tool::Rect, "R");
    addToolAction(":/icons/tool-ellipse.svg", tr("Ellipse (E)"), Tool::Ellipse, "E");
    addToolAction(":/icons/tool-line.svg", tr("Line (L)"), Tool::Line, "L");
    addToolAction(":/icons/tool-curve.svg", tr("Curve (C)"), Tool::BezierCurve, "C");
    addToolAction(":/icons/tool-freehand.svg", tr("Freehand (F)"), Tool::Freehand, "F");
    addToolAction(":/icons/tool-text.svg", tr("Text (T)"), Tool::Text, "T");
    addToolAction(":/icons/tool-image.svg", tr("Image (I)"), Tool::Image, "I");

    // 对齐工具栏
    m_alignToolBar = addToolBar(tr("Align"));
    m_alignToolBar->setMovable(false);
    m_alignToolBar->setObjectName("AlignToolBar");
    m_alignToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_alignToolBar->setIconSize(QSize(20, 20));

    QAction *alignLayoutAct = m_alignToolBar->addAction(QIcon(":/icons/align-layout.svg"), tr("Align && Layout..."));
    alignLayoutAct->setToolTip(tr("Open Align & Layout dialog"));
    connect(alignLayoutAct, &QAction::triggered, this, &MainWindow::onAlignLayoutDialog);

    m_alignToolBar->addSeparator();

    // 顺时针旋转 90°
    QAction *rotateCWAct = m_alignToolBar->addAction(QIcon(":/icons/rotate-cw.svg"), tr("Rotate 90\u00b0 CW"));
    rotateCWAct->setToolTip(tr("Rotate 90\u00b0 clockwise"));
    connect(rotateCWAct, &QAction::triggered, this, [this]() { rotateSelectedItems(90.0); });

    // 逆时针旋转 90°
    QAction *rotateCCWAct = m_alignToolBar->addAction(QIcon(":/icons/rotate-ccw.svg"), tr("Rotate 90\u00b0 CCW"));
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
    connect(m_pView, &QAtGraphicsView::selectionChanged, this,
            &MainWindow::onSelectionChanged);
    connect(m_pView, &QAtGraphicsView::itemAdded, this, &MainWindow::onItemAdded);

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

    connect(m_undoStack, &QUndoStack::canUndoChanged, this, [this]() {
        _updateUndoRedoActions();
    });
    connect(m_undoStack, &QUndoStack::canRedoChanged, this, [this]() {
        _updateUndoRedoActions();
    });

    // undo/redo 后更新 ResizeHandleItem（旋转变更等不触发 selectionChanged）
    connect(m_undoStack, &QUndoStack::indexChanged, this, [this]() {
        m_pView->scheduleResizeHandleUpdate();
    });

    // 视图滚动/缩放时更新刻度尺
    connect(m_pView->horizontalScrollBar(), &QScrollBar::valueChanged, m_hRuler,
            &RulerBar::updateRuler);
    connect(m_pView->verticalScrollBar(), &QScrollBar::valueChanged, m_vRuler,
            &RulerBar::updateRuler);

    // 状态栏：鼠标位置 & 缩放变化 & 刻度尺鼠标位置同步
    connect(m_pView, &QAtGraphicsView::mousePositionChanged, this, [this](const QPointF &pos) {
        m_posLabel->setText(tr("X: %1  Y: %2").arg(pos.x(), 0, 'f', 1).arg(pos.y(), 0, 'f', 1));
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
}

void MainWindow::_initStatusBar()
{
    auto *bar = statusBar();

    // 鼠标坐标
    m_posLabel = new QLabel(tr("X: 0.0  Y: 0.0"));
    m_posLabel->setMinimumWidth(180);

    // 缩放标签
    m_zoomLabel = new QLabel(tr("100%"));
    m_zoomLabel->setMinimumWidth(60);

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
    if (m_pView->canvasItem()) {
        QSizeF sz = m_pView->canvasItem()->canvasSize();
        // 像素转 mm (96dpi: 1px ≈ 0.2646mm)
        static constexpr qreal kPxToMM = 0.2645833333;
        m_canvasLabel->setText(tr("Canvas: %1 \u00d7 %2 mm")
                                       .arg(sz.width() * kPxToMM, 0, 'f', 1)
                                       .arg(sz.height() * kPxToMM, 0, 'f', 1));
    }

    // 当前工具
    m_toolLabel = new QLabel(tr("Tool: Select"));
    m_toolLabel->setMinimumWidth(100);

    bar->addWidget(m_posLabel);
    bar->addPermanentWidget(m_zoomLabel);
    bar->addPermanentWidget(m_zoomSlider);
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

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    // 更新刻度尺
    if (m_hRuler) m_hRuler->updateRuler();
    if (m_vRuler) m_vRuler->updateRuler();
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

    // 先清空 undo 栈，避免命令引用即将被删除的图元
    m_undoStack->clear();

    // 清空属性面板引用
    m_pPropertyPanel->setItem(nullptr);

    // 安全清空场景并重建画布
    m_pView->resetCanvas(canvasSize);
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
    QString path = QFileDialog::getSaveFileName(
            this, tr("Export Image"), QString(),
            tr("TIFF (*.tif *.tiff);;PNG (*.png);;JPEG (*.jpg);;BMP (*.bmp)"));
    if (path.isEmpty()) return;

    // 如果有画布，按画布尺寸导出
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

    if (path.endsWith(".tif", Qt::CaseInsensitive) || path.endsWith(".tiff", Qt::CaseInsensitive))
        exportTiffLossless(path, image);
    else
        image.save(path);
}

// ============================================================
// TIFF 无损导出（使用 libtiff）
// ============================================================
bool MainWindow::exportTiffLossless(const QString &path, const QImage &image)
{
    QImage img = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    TIFF *tif = TIFFOpen(path.toUtf8().constData(), "w");
    if (!tif) return false;

    int width = img.width();
    int height = img.height();

    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE); // 无压缩，无损
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 4); // RGBA
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, 1);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

    // 转换 ARGB32 → RGBA
    QVector<uint8_t> rgbaRow(width * 4);
    for (int y = 0; y < height; ++y) {
        const QRgb *scanLine = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        for (int x = 0; x < width; ++x) {
            QRgb c = scanLine[x];
            rgbaRow[x * 4 + 0] = qRed(c);
            rgbaRow[x * 4 + 1] = qGreen(c);
            rgbaRow[x * 4 + 2] = qBlue(c);
            rgbaRow[x * 4 + 3] = qAlpha(c);
        }
        TIFFWriteScanline(tif, rgbaRow.data(), y);
    }

    TIFFClose(tif);
    return true;
}

// ============================================================
// 撤销/重做
// ============================================================
void MainWindow::onUndo() { m_undoStack->undo(); }
void MainWindow::onRedo() { m_undoStack->redo(); }

// ============================================================
// 剪贴板操作
// ============================================================
void MainWindow::onCut()
{
    auto items = m_pView->scene()->selectedItems();
    if (items.isEmpty()) return;

    // 使用 macro 将 Copy+Delete 合并为单步撤销
    m_undoStack->beginMacro(tr("Cut"));
    onCopy();
    onDelete();
    m_undoStack->endMacro();
}

void MainWindow::onCopy()
{
    auto items = m_pView->scene()->selectedItems();
    if (items.isEmpty()) return;
    copyItemsToClipboard(items);
}

void MainWindow::onPaste()
{
    auto items = pasteItemsFromClipboard();
    if (items.isEmpty()) return;

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
    if (items.isEmpty()) return;

    // 过滤掉 CanvasItem 和 ResizeHandleItem
    QList<QGraphicsItem *> deletable;
    for (auto *item : items) {
        if (qgraphicsitem_cast<CanvasItem *>(item))
            continue;
        if (item->type() == ResizeHandleItem::Type)
            continue;
        deletable << item;
    }
    if (deletable.isEmpty()) return;

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
    if (items.isEmpty()) return;

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
    if (items.isEmpty()) return;

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
        connect(m_alignLayoutDlg, &QObject::destroyed, this, [this]() {
            m_alignLayoutDlg = nullptr;
        });
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
    if (items.size() < 2) return;

    auto result = AlignmentUtils::computeAlign(items, direction);
    if (!result.valid) return;

    m_undoStack->push(new AlignItemsCommand(
        items, result.oldPositions, result.newPositions,
        tr("Align %1").arg(AlignmentUtils::alignDirectionName(direction)),
        m_pView->scene()));
}

void MainWindow::applyDistribute(AlignmentUtils::DistributeDirection direction,
                                 const AlignmentUtils::DistributeParams &params)
{
    auto items = filterSelectableItems();
    if (items.size() < 2) return;

    auto result = AlignmentUtils::computeDistribute(items, direction, params);
    if (!result.valid) return;

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

    // 更新状态栏工具名
    static const QMap<Tool, QString> toolNames = {
        { Tool::Select, tr("Select") },
        { Tool::Rect, tr("Rectangle") },
        { Tool::Ellipse, tr("Ellipse") },
        { Tool::Line, tr("Line") },
        { Tool::BezierCurve, tr("B\u00e9zier Curve") },
        { Tool::Text, tr("Text") },
        { Tool::Image, tr("Image") },
    };
    if (m_toolLabel)
        m_toolLabel->setText(tr("Tool: %1").arg(toolNames.value(tool, tr("Unknown"))));
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
    m_undoStack->push(new PropertyChangeCommand(
            item, PropertyChangeCommand::Pen, QVariant::fromValue(oldPen),
            QVariant::fromValue(newPen), m_pView->scene()));
}

void MainWindow::onBrushChanged(QGraphicsItem *item, const QBrush &oldBrush, const QBrush &newBrush)
{
    m_undoStack->push(new PropertyChangeCommand(
            item, PropertyChangeCommand::Brush, QVariant::fromValue(oldBrush),
            QVariant::fromValue(newBrush), m_pView->scene()));
}

void MainWindow::onFontChanged(QGraphicsItem *item, const QFont &oldFont, const QFont &newFont)
{
    m_undoStack->push(new PropertyChangeCommand(
            item, PropertyChangeCommand::Font, QVariant::fromValue(oldFont),
            QVariant::fromValue(newFont), m_pView->scene()));
}

void MainWindow::onTextChanged(QGraphicsItem *item, const QString &oldText, const QString &newText)
{
    m_undoStack->push(new PropertyChangeCommand(
            item, PropertyChangeCommand::Text, QVariant(oldText), QVariant(newText),
            m_pView->scene()));
}

void MainWindow::onGeometryChanged(QGraphicsItem *item, const QRectF &oldRect, const QRectF &newRect)
{
    m_undoStack->push(new PropertyChangeCommand(
            item, PropertyChangeCommand::Geometry, QVariant(oldRect), QVariant(newRect),
            m_pView->scene()));
}

void MainWindow::onCornerRadiusChanged(QGraphicsItem *item, qreal oldR, qreal newR)
{
    m_undoStack->push(new PropertyChangeCommand(
            item, PropertyChangeCommand::CornerRadius, QVariant(oldR), QVariant(newR),
            m_pView->scene()));
}

void MainWindow::onPositionChanged(QGraphicsItem *item, const QPointF &oldPos, const QPointF &newPos)
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
    if (items.isEmpty()) return;

    if (items.size() == 1) {
        // 单个图元：绕自身中心旋转（RotationChangeCommand 已内置中心补偿）
        auto *item = items.first();
        qreal oldRotation = item->rotation();
        qreal newRotation = oldRotation + angleDelta;
        m_undoStack->push(new RotationChangeCommand(item, oldRotation, newRotation, m_pView->scene()));
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
            m_undoStack->push(new RotationChangeCommand(item, oldRotation, newRotation, m_pView->scene()));

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
            oldPositions << posAfterCenterComp;  // 公转前位置
            newPositions << item->pos();          // 公转后最终位置
        }

        // 公转位移变更作为一个命令
        if (!moveItems.isEmpty()) {
            m_undoStack->push(new MoveItemsCommand(moveItems, oldPositions, newPositions,
                                                    m_pView->scene()));
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
        if (!gi) continue;
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
    if (!mime || !mime->hasFormat(kMimeFormat)) return result;

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
        auto *gi = createItemByType(static_cast<IGraphicsItem::ItemType>(typeInt));
        if (!gi) continue;
        gi->deserialize(in);
        result << dynamic_cast<QGraphicsItem *>(gi);
    }
    return result;
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // 快捷键切换工具
    switch (event->key()) {
    case Qt::Key_V: if (!event->modifiers()) onToolTriggered(Tool::Select); return;
    case Qt::Key_R: if (!event->modifiers()) onToolTriggered(Tool::Rect); return;
    case Qt::Key_E: if (!event->modifiers()) onToolTriggered(Tool::Ellipse); return;
    case Qt::Key_L: if (!event->modifiers()) onToolTriggered(Tool::Line); return;
    case Qt::Key_C: if (!event->modifiers()) onToolTriggered(Tool::BezierCurve); return;
    case Qt::Key_F: if (!event->modifiers()) onToolTriggered(Tool::Freehand); return;
    case Qt::Key_T: if (!event->modifiers()) onToolTriggered(Tool::Text); return;
    case Qt::Key_I: if (!event->modifiers()) onToolTriggered(Tool::Image); return;
    case Qt::Key_Delete: onDelete(); return;
    }
    QMainWindow::keyPressEvent(event);
}
