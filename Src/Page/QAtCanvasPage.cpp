#include "QAtCanvasPage.h"
#include "qatgraphicsview.h"
#include "CanvasItem.h"
#include "RulerBar.h"

#include <QGraphicsScene>
#include <QUndoStack>
#include <QTimer>
#include <QGridLayout>
#include <QScrollBar>

// ==================== Layout helper ====================
void QAtCanvasPage::_initLayout()
{
    m_hRuler = new RulerBar(RulerBar::Horizontal, this);
    m_vRuler = new RulerBar(RulerBar::Vertical, this);

    m_cornerWidget = new QWidget(this);
    m_cornerWidget->setFixedSize(30, 30);

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

    // Wire rulers AFTER the view is in the layout (viewport geometry is then valid)
    m_hRuler->setGraphicsView(m_view);
    m_vRuler->setGraphicsView(m_view);

    // Scroll bar → ruler sync
    connect(m_view->horizontalScrollBar(), &QScrollBar::valueChanged, m_hRuler,
            &RulerBar::updateRuler);
    connect(m_view->verticalScrollBar(), &QScrollBar::valueChanged, m_vRuler,
            &RulerBar::updateRuler);

    // Mouse position → ruler indicator
    connect(m_view, &QAtGraphicsView::mousePositionChanged, this, [this](const QPointF &pos) {
        m_hRuler->setMousePosition(pos);
        m_vRuler->setMousePosition(pos);
    });

    // Zoom → ruler update
    connect(m_view, &QAtGraphicsView::zoomChanged, this, [this](qreal /*level*/) {
        m_hRuler->updateRuler();
        m_vRuler->updateRuler();
    });
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
    if (m_hRuler)
        m_hRuler->setPpi(ppi);
    if (m_vRuler)
        m_vRuler->setPpi(ppi);
}

void QAtCanvasPage::updateRulers()
{
    if (m_hRuler)
        m_hRuler->updateRuler();
    if (m_vRuler)
        m_vRuler->updateRuler();
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
