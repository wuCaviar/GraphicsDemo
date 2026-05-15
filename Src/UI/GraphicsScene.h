#ifndef GRAPHICSSCENE_H
#define GRAPHICSSCENE_H

#include <QGraphicsScene>
#include <QSet>

class QGraphicsItem;
class QUndoStack;
class ResizeHandleItem;

class GraphicsScene : public QGraphicsScene
{
    Q_OBJECT
public:
    explicit GraphicsScene(QObject *parent = nullptr);
    ~GraphicsScene();

    void setUndoStack(QUndoStack *stack) { m_undoStack = stack; }
    QUndoStack *undoStack() const { return m_undoStack; }

    // Public methods for MainWindow / View
    void scheduleResizeHandleUpdate();
    void refreshResizeHandle();
    void setPastedItems(const QList<QGraphicsItem *> &items);
    void clearPastedItems() { m_pastedItems.clear(); }

    // For QAtGraphicsView mouse event handling
    bool isResizeHandleActive() const;
    void syncResizeHandleDuringMove();
    void cleanupInvalidHandle();

    // Full scene reset (clears items + handle + pasted tracking)
    void clearScene();

private slots:
    void onSelectionChanged();

private:
    void updateResizeHandle();
    void removeResizeHandle();

    ResizeHandleItem *m_resizeHandle = nullptr;
    QUndoStack *m_undoStack = nullptr;
    bool m_updatePending = false;
    QSet<QGraphicsItem *> m_pastedItems;
};

#endif // GRAPHICSSCENE_H
