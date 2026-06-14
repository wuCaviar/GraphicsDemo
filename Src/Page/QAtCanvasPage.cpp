#include "QAtCanvasPage.h"
#include "qatgraphicsview.h"
#include "CanvasItem.h"

#include <QGraphicsScene>
#include <QUndoStack>
#include <QTimer>
#include <QVBoxLayout>

QAtCanvasPage::QAtCanvasPage(const QString &id, QWidget *parent)
    : QAtPage(parent), m_pageId(id)
{
    m_view = new QAtGraphicsView(nullptr);
    // View creates its own GraphicsScene; QAtCanvasPage uses it directly.
    m_scene = m_view->scene();
    m_undoStack = new QUndoStack(this);
    m_view->setUndoStack(m_undoStack);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);

    connect(m_view, &QAtGraphicsView::itemAdded, this, &QAtCanvasPage::itemAdded);
    connect(m_view, &QAtGraphicsView::selectionChanged, this, &QAtCanvasPage::selectionChanged);
    connect(m_view, &QAtGraphicsView::mousePositionChanged, this, &QAtCanvasPage::mousePositionChanged);
    connect(m_view, &QAtGraphicsView::zoomChanged, this, &QAtCanvasPage::zoomChanged);
    connect(m_view, &QAtGraphicsView::toolChanged, this, &QAtCanvasPage::toolChanged);
}

QAtCanvasPage::QAtCanvasPage(const QString &id, const QSizeF &canvasSize,
                               qreal ppi, QWidget *parent)
    : QAtPage(parent), m_pageId(id)
{
    m_view = new QAtGraphicsView(nullptr);
    // View creates its own GraphicsScene; QAtCanvasPage uses it directly.
    m_scene = m_view->scene();
    m_undoStack = new QUndoStack(this);
    m_view->setUndoStack(m_undoStack);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);

    // Create the canvas internally — initCanvas uses the view's own scene
    m_view->setCanvasSize(canvasSize);
    if (auto *c = m_view->canvasItem()) {
        c->setPpi(ppi);
        c->setCanvasDpi(0, 0);
    }
    m_view->setEnabled(true);

    // Defer centering until the view has its final geometry
    QTimer::singleShot(0, m_view, [v = m_view]() { v->fitToCanvas(); });

    connect(m_view, &QAtGraphicsView::itemAdded, this, &QAtCanvasPage::itemAdded);
    connect(m_view, &QAtGraphicsView::selectionChanged, this, &QAtCanvasPage::selectionChanged);
    connect(m_view, &QAtGraphicsView::mousePositionChanged, this, &QAtCanvasPage::mousePositionChanged);
    connect(m_view, &QAtGraphicsView::zoomChanged, this, &QAtCanvasPage::zoomChanged);
    connect(m_view, &QAtGraphicsView::toolChanged, this, &QAtCanvasPage::toolChanged);
}

QAtCanvasPage::~QAtCanvasPage()
{
    // m_view is owned by the widget tree (MainWindow::setCentralWidget),
    // not by QAtCanvasPage. Qt disconnects signals automatically.
}

QString QAtCanvasPage::title() const
{
    return m_pageId.isEmpty() ? tr("Canvas") : m_pageId;
}

QAtGraphicsView* QAtCanvasPage::view() const      { return m_view; }
QGraphicsScene*  QAtCanvasPage::scene() const      { return m_scene; }
QUndoStack*      QAtCanvasPage::undoStack() const   { return m_undoStack; }
CanvasItem*      QAtCanvasPage::canvasItem() const  { return m_view ? m_view->canvasItem() : nullptr; }
Tool             QAtCanvasPage::currentTool() const { return m_view ? m_view->currentTool() : Tool::Select; }
qreal            QAtCanvasPage::zoomLevel() const   { return m_view ? m_view->zoomLevel() : 1.0; }

void QAtCanvasPage::setTool(Tool tool)             { if (m_view) m_view->setTool(tool); }
void QAtCanvasPage::setCanvasSize(const QSizeF &s) { if (m_view) m_view->setCanvasSize(s); }
void QAtCanvasPage::clearCanvas(const QSizeF &s)   { if (m_view) m_view->resetCanvas(s); }

QList<QGraphicsItem*> QAtCanvasPage::selectedItems() const
{
    return m_scene ? m_scene->selectedItems() : QList<QGraphicsItem*>();
}
