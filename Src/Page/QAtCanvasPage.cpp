#include "QAtCanvasPage.h"
#include "atMath.h"
#include "qatgraphicsview.h"
#include "CanvasItem.h"
#include "QRuler.h"
#include "ViewConverter.h"
#include "Unit.h"

#include <QGraphicsScene>
#include <QUndoStack>
#include <QTimer>
#include <QGridLayout>
#include <QScrollBar>

// ==================== Layout helper ====================
void QAtCanvasPage::_initLayout()
{
    // Create ViewConverters (one per ruler, though they share the same zoom)
    m_hConverter = new ViewConverter();
    m_vConverter = new ViewConverter();

    m_hRuler = new QRuler(this, Qt::Horizontal, m_hConverter);
    m_vRuler = new QRuler(this, Qt::Vertical, m_vConverter);

    // Use Millimeter unit for display (matching old RulerBar behavior)
    m_hRuler->setUnit(Unit(Unit::Millimeter));
    m_vRuler->setUnit(Unit(Unit::Millimeter));

    // Show mouse position indicator on both rulers
    m_hRuler->setShowMousePosition(true);
    m_vRuler->setShowMousePosition(true);

    // Match corner widget size to ruler thickness
    int rulerThickH = m_hRuler->sizeHint().height();
    int rulerThickV = m_vRuler->sizeHint().width();
    m_cornerWidget = new QWidget(this);
    m_cornerWidget->setFixedSize(rulerThickV, rulerThickH);

    m_layout = new QGridLayout(this);
    m_layout->setSpacing(0);
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_layout->addWidget(m_cornerWidget, 0, 0);
    m_layout->addWidget(m_hRuler, 0, 1);
    m_layout->addWidget(m_vRuler, 1, 0);
    m_layout->addWidget(m_view, 1, 1);

    // Stretch column 1 (view area) and row 1 so the view fills remaining space
    m_layout->setColumnStretch(0, 0);
    m_layout->setColumnStretch(1, 1);
    m_layout->setRowStretch(0, 0);
    m_layout->setRowStretch(1, 1);

    // --- Wire rulers AFTER layout so geometry is valid ---

    // Scroll bar → ruler sync (update offset + zoom)
    connect(m_view->horizontalScrollBar(), &QScrollBar::valueChanged, this,
            [this](int) { updateRulers(); });
    connect(m_view->verticalScrollBar(), &QScrollBar::valueChanged, this,
            [this](int) { updateRulers(); });

    // Mouse position → ruler indicator
    connect(m_view, &QAtGraphicsView::mousePositionChanged, this, [this](const QPointF &scenePos) {
        if (!m_view)
            return;
        qreal scale = m_view->transform().m11();
        // QRuler::updateMouseCoordinate expects position in view pixels (pre-offset)
        m_hRuler->updateMouseCoordinate(qRound(scenePos.x() * scale));
        m_vRuler->updateMouseCoordinate(qRound(scenePos.y() * scale));
    });

    // Zoom → ruler update
    connect(m_view, &QAtGraphicsView::zoomChanged, this, [this](qreal) { updateRulers(); });

    // Initial ruler setup
    updateRulers();
}

// ==================== Constructors ====================
QAtCanvasPage::QAtCanvasPage(const QString &id, QWidget *parent) : QAtPage(parent), m_pageId(id)
{
    m_view = new QAtGraphicsView(nullptr);
    // View creates its own GraphicsScene; QAtCanvasPage uses it directly.
    m_scene = m_view->scene();
    m_undoStack = new QUndoStack(this);
    m_view->setUndoStack(m_undoStack);

    _initLayout();

    connect(m_view, &QAtGraphicsView::itemAdded, this, &QAtCanvasPage::itemAdded);
    connect(m_view, &QAtGraphicsView::selectionChanged, this, &QAtCanvasPage::selectionChanged);
    connect(m_view, &QAtGraphicsView::mousePositionChanged, this,
            &QAtCanvasPage::mousePositionChanged);
    connect(m_view, &QAtGraphicsView::zoomChanged, this, &QAtCanvasPage::zoomChanged);
    connect(m_view, &QAtGraphicsView::toolChanged, this, &QAtCanvasPage::toolChanged);
}

QAtCanvasPage::QAtCanvasPage(const QString &id, const QSizeF &canvasSize, qreal ppi,
                             QWidget *parent)
    : QAtPage(parent), m_pageId(id)
{
    m_view = new QAtGraphicsView(nullptr);
    // View creates its own GraphicsScene; QAtCanvasPage uses it directly.
    m_scene = m_view->scene();
    m_undoStack = new QUndoStack(this);
    m_view->setUndoStack(m_undoStack);

    _initLayout();

    // Create the canvas internally — initCanvas uses the view's own scene
    m_view->setCanvasSize(canvasSize);
    if (auto *c = m_view->canvasItem()) {
        c->setPpi(ppi);
        c->setCanvasDpi(0, 0);
    }
    // Propagate PPI to rulers
    setRulerPpi(ppi);

    m_view->setEnabled(true);

    // Defer centering until the view has its final geometry
    QTimer::singleShot(0, m_view, [v = m_view]() { v->fitToCanvas(); });

    connect(m_view, &QAtGraphicsView::itemAdded, this, &QAtCanvasPage::itemAdded);
    connect(m_view, &QAtGraphicsView::selectionChanged, this, &QAtCanvasPage::selectionChanged);
    connect(m_view, &QAtGraphicsView::mousePositionChanged, this,
            &QAtCanvasPage::mousePositionChanged);
    connect(m_view, &QAtGraphicsView::zoomChanged, this, &QAtCanvasPage::zoomChanged);
    connect(m_view, &QAtGraphicsView::toolChanged, this, &QAtCanvasPage::toolChanged);
}

QAtCanvasPage::~QAtCanvasPage()
{
    // All child widgets are owned by the Qt parent-child tree.
    // Qt disconnects signals automatically.
}

// ==================== Page activation ====================
void QAtCanvasPage::onActivated()
{
    // Update rulers in case window was resized while this page was inactive
    updateRulers();
}

// ==================== Ruler helpers ====================
void QAtCanvasPage::setRulerPpi(qreal ppi)
{
    m_ppi = AtMath::clamp(ppi, 1.0, 9999.0);
    updateRulers();
}

void QAtCanvasPage::updateRulers()
{
    if (!m_view)
        return;

    QTransform transform = m_view->transform();
    qreal scale = transform.m11();

    // Calculate origin offset: scene (0,0) → screen pixel position
    QPoint vpOrigin = m_view->mapFromScene(QPointF(0, 0));
    QPoint widgetOrigin = m_view->viewport()->mapToParent(vpOrigin);

    // Get canvas size
    CanvasItem *canvas = m_view->canvasItem();
    if (!canvas)
        return;
    QRectF canvasRect = canvas->rect();
    qreal canvasW = canvasRect.width();
    qreal canvasH = canvasRect.height();

    // --- Horizontal ruler ---
    if (m_hRuler && m_hConverter) {
        // Convert scene pixels → points: 72 points/inch, PPI scene-pixels/inch
        // docPoint = scenePixel * (72 / PPI)
        const qreal sceneToPoint = AtMath::Units::DPIContext(m_ppi).pxToPt(1.0);
        m_hConverter->setZoom(
            scale * AtMath::Units::DPIContext(m_ppi).ptToPx(1.0)); // point → screen-pixel
        m_hRuler->setOffset(widgetOrigin.x());
        m_hRuler->setRulerLength(canvasW * sceneToPoint);
    }

    // --- Vertical ruler ---
    if (m_vRuler && m_vConverter) {
        const qreal sceneToPoint = AtMath::Units::DPIContext(m_ppi).pxToPt(1.0);
        m_vConverter->setZoom(scale * AtMath::Units::DPIContext(m_ppi).ptToPx(1.0));
        m_vRuler->setOffset(widgetOrigin.y());
        m_vRuler->setRulerLength(canvasH * sceneToPoint);
    }
}

// ==================== Accessors ====================
QString QAtCanvasPage::title() const
{
    return m_pageId.isEmpty() ? tr("Canvas") : m_pageId;
}

QAtGraphicsView *QAtCanvasPage::view() const
{
    return m_view;
}
QGraphicsScene *QAtCanvasPage::scene() const
{
    return m_scene;
}
QUndoStack *QAtCanvasPage::undoStack() const
{
    return m_undoStack;
}
CanvasItem *QAtCanvasPage::canvasItem() const
{
    return m_view ? m_view->canvasItem() : nullptr;
}
Tool QAtCanvasPage::currentTool() const
{
    return m_view ? m_view->currentTool() : Tool::Select;
}
qreal QAtCanvasPage::zoomLevel() const
{
    return m_view ? m_view->zoomLevel() : 1.0;
}

void QAtCanvasPage::setTool(Tool tool)
{
    if (m_view)
        m_view->setTool(tool);
}
void QAtCanvasPage::setCanvasSize(const QSizeF &s)
{
    if (m_view)
        m_view->setCanvasSize(s);
}
void QAtCanvasPage::clearCanvas(const QSizeF &s)
{
    if (m_view)
        m_view->resetCanvas(s);
}

QList<QGraphicsItem *> QAtCanvasPage::selectedItems() const
{
    return m_scene ? m_scene->selectedItems() : QList<QGraphicsItem *>();
}
