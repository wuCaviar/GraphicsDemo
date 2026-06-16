#ifndef STATUSBARDIRECTOR_H
#define STATUSBARDIRECTOR_H

#include <QObject>
#include <QPointF>

class QLabel;
class QComboBox;
class QToolButton;
class QSlider;
enum class Tool;

class StatusBarDirector : public QObject
{
    Q_OBJECT
public:
    explicit StatusBarDirector(QObject *parent = nullptr);

    void setPositionLabel(QLabel *label);
    void setZoomControls(QComboBox *combo, QToolButton *outBtn, QToolButton *inBtn, QSlider *slider);
    void setCanvasLabel(QLabel *label);
    void setToolLabel(QLabel *label);

    void applyZoomFromCombo();
    void applyPresetFromCombo(int index);

public slots:
    void onPageSwitched(const QString &pageId, const QString &pageType);
    void onMousePositionChanged(const QPointF &scenePos);
    void onZoomChanged(qreal level);
    void onToolChanged(Tool tool);

private:
    QLabel *m_posLabel = nullptr;
    QComboBox *m_zoomCombo = nullptr;
    QToolButton *m_zoomOutBtn = nullptr;
    QToolButton *m_zoomInBtn = nullptr;
    QSlider *m_zoomSlider = nullptr;
    QLabel *m_canvasLabel = nullptr;
    QLabel *m_toolLabel = nullptr;
    bool m_updatingZoom = false;

    static qreal _sliderToZoom(int sliderValue);
    static int _zoomToSliderValue(qreal zoom);
};

#endif // STATUSBARDIRECTOR_H
