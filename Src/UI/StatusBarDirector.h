#ifndef STATUSBARDIRECTOR_H
#define STATUSBARDIRECTOR_H

#include <QObject>
#include <QPointF>

class QLabel;
class QSlider;
class QToolButton;
enum class Tool;

// Manages status bar widget bindings — rebinds signals on page switch
class StatusBarDirector : public QObject
{
    Q_OBJECT
public:
    explicit StatusBarDirector(QObject *parent = nullptr);

    void setPositionLabel(QLabel *label);
    void setZoomControls(QLabel *label, QSlider *slider);
    void setCanvasLabel(QLabel *label);
    void setToolLabel(QLabel *label);

public slots:
    void onPageSwitched(const QString &pageId, const QString &pageType);
    void onMousePositionChanged(const QPointF &scenePos);
    void onZoomChanged(qreal level);
    void onToolChanged(Tool tool);

private:
    QLabel  *m_posLabel    = nullptr;
    QLabel  *m_zoomLabel   = nullptr;
    QSlider *m_zoomSlider  = nullptr;
    QLabel  *m_canvasLabel = nullptr;
    QLabel  *m_toolLabel   = nullptr;
};

#endif // STATUSBARDIRECTOR_H
