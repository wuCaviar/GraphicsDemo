#include "GraphicsScene.h"

#include "CanvasItem.h"
#include "IGraphicsItem.h"
#include "ResizeHandleItem.h"

#include <QGraphicsItem>
#include <QUndoStack>

GraphicsScene::GraphicsScene(QObject *parent)
    : QGraphicsScene(parent)
{
    connect(this, &QGraphicsScene::selectionChanged, this, &GraphicsScene::onSelectionChanged);
}

GraphicsScene::~GraphicsScene()
{
    disconnect(this, &QGraphicsScene::selectionChanged, this, &GraphicsScene::onSelectionChanged);
    removeResizeHandle();
}

// ============================================================
// Public API
// ============================================================

void GraphicsScene::scheduleResizeHandleUpdate()
{
    if (m_updatePending)
        return;
    m_updatePending = true;
    QMetaObject::invokeMethod(
        this,
        [this]() {
            m_updatePending = false;
            updateResizeHandle();
        },
        Qt::QueuedConnection);
}

void GraphicsScene::refreshResizeHandle()
{
    if (m_resizeHandle) {
        if (!m_resizeHandle->isTargetValid()) {
            if (!m_resizeHandle->isResizing())
                removeResizeHandle();
            return;
        }
        m_resizeHandle->updateHandlePositions();
    }
}

void GraphicsScene::setPastedItems(const QList<QGraphicsItem *> &items)
{
    m_pastedItems = QSet<QGraphicsItem *>(items.begin(), items.end());
}

bool GraphicsScene::isResizeHandleActive() const
{
    return m_resizeHandle && m_resizeHandle->isResizing();
}

void GraphicsScene::syncResizeHandleDuringMove()
{
    if (!m_resizeHandle)
        return;
    m_resizeHandle->updateHandlePositions();
}

void GraphicsScene::cleanupInvalidHandle()
{
    if (m_resizeHandle && !m_resizeHandle->isResizing() && !m_resizeHandle->isTargetValid()) {
        removeResizeHandle();
        updateResizeHandle();
    }
}

// ============================================================
// Clear
// ============================================================

void GraphicsScene::clearScene()
{
    disconnect(this, &QGraphicsScene::selectionChanged, this, &GraphicsScene::onSelectionChanged);
    removeResizeHandle();
    m_pastedItems.clear();
    clear();
    connect(this, &QGraphicsScene::selectionChanged, this, &GraphicsScene::onSelectionChanged);
}

// ============================================================
// Selection change handler
// ============================================================

void GraphicsScene::onSelectionChanged()
{
    auto currentSelection = selectedItems();

    // Clear pasted state if no pasted items are selected
    bool anyPastedSelected = false;
    for (auto *item : currentSelection) {
        if (m_pastedItems.contains(item)) {
            anyPastedSelected = true;
            break;
        }
    }
    if (!anyPastedSelected)
        m_pastedItems.clear();

    // Resize in progress: skip, let the drag finish
    if (m_resizeHandle && m_resizeHandle->isResizing())
        return;

    // Rubber-banding: deferred via scheduleResizeHandleUpdate (called by view)
    // If update is already pending, skip
    if (m_updatePending)
        return;

    auto selectableItems = ::filterSelectableItems(currentSelection);
    if (selectableItems.isEmpty())
        removeResizeHandle();
    else
        updateResizeHandle();
}

// ============================================================
// Handle lifecycle
// ============================================================

void GraphicsScene::updateResizeHandle()
{
    if (m_resizeHandle && m_resizeHandle->isResizing())
        return;

    auto selectableItems = ::filterSelectableItems(selectedItems());

    if (selectableItems.isEmpty()) {
        removeResizeHandle();
        return;
    }

    if (!m_resizeHandle) {
        m_resizeHandle = new ResizeHandleItem(nullptr);
        addItem(m_resizeHandle);
        m_resizeHandle->setUndoStack(m_undoStack);
    }

    if (selectableItems.size() == 1)
        m_resizeHandle->setTargetItem(selectableItems.first());
    else
        m_resizeHandle->setTargetItems(selectableItems);

    bool hasPasted = false;
    for (auto *item : selectableItems) {
        if (m_pastedItems.contains(item)) {
            hasPasted = true;
            break;
        }
    }
    m_resizeHandle->setPastedStyle(hasPasted);
}

void GraphicsScene::removeResizeHandle()
{
    if (m_resizeHandle) {
        removeItem(m_resizeHandle);
        delete m_resizeHandle;
        m_resizeHandle = nullptr;
    }
}
