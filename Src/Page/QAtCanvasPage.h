#ifndef QATCANVASPAGE_H
#define QATCANVASPAGE_H

#include "QAtPage.h"

#include <QGraphicsItem>
#include <QSizeF>
#include <QMap>

class QAtGraphicsView;
class QGraphicsScene;
class QUndoStack;
class CanvasItem;
class QGridLayout;
class QRuler;
class ViewConverter;

enum class Tool;

// Canvas page — encapsulates a full drawing canvas with its own rulers
class QAtCanvasPage : public QAtPage
{
    Q_OBJECT

public:
    // Create a canvas page with a default-size canvas
    explicit QAtCanvasPage(const QString &id, QWidget *parent = nullptr);
    // Create a canvas page with a canvas of the given pixel size
    QAtCanvasPage(const QString &id, const QSizeF &canvasSize, qreal ppi = 150.0,
                  QWidget *parent = nullptr);
    ~QAtCanvasPage() override;

    QString pageId() const override { return m_pageId; }
    QString pageType() const override { return PageType::Canvas; }
    QString title() const override;

    void onActivated() override;

    QAtGraphicsView *view() const;
    QGraphicsScene *scene() const;
    QUndoStack *undoStack() const;
    CanvasItem *canvasItem() const;

    // Ruler accessors
    QRuler *hRuler() const { return m_hRuler; }
    QRuler *vRuler() const { return m_vRuler; }
    void setRulerPpi(qreal ppi);
    void updateRulers();
    qreal rulerPpi() const { return m_ppi; }

    Tool currentTool() const;
    void setTool(Tool tool);
    qreal zoomLevel() const;

    void setCanvasSize(const QSizeF &size);
    void clearCanvas(const QSizeF &size);
    QList<QGraphicsItem *> selectedItems() const;

signals:
    void itemAdded(QGraphicsItem *item);
    void selectionChanged();
    void toolChanged(Tool tool);
    void zoomChanged(qreal level);
    void mousePositionChanged(const QPointF &scenePos);

private:
    void _initLayout();

    QString m_pageId;
    QAtGraphicsView *m_view = nullptr;
    QGraphicsScene *m_scene = nullptr;
    QUndoStack *m_undoStack = nullptr;
    QGridLayout *m_layout = nullptr;
    QRuler *m_hRuler = nullptr;
    QRuler *m_vRuler = nullptr;
    ViewConverter *m_hConverter = nullptr;
    ViewConverter *m_vConverter = nullptr;
    QWidget *m_cornerWidget = nullptr;
    qreal m_ppi = 150.0;
};

#endif // QATCANVASPAGE_H
