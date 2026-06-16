#ifndef STATUSBARDIRECTOR_H
#define STATUSBARDIRECTOR_H

#include <QObject>
#include <QPointF>

class QLabel;
class QLineEdit;
class QToolButton;
class QMenu;
class QSlider;
enum class Tool;

class StatusBarDirector : public QObject
{
    Q_OBJECT
public:
    explicit StatusBarDirector(QObject *parent = nullptr);

    void setPositionLabel(QLabel *label);
    void setZoomControls(QLineEdit *edit, QLabel *pctLabel, QToolButton *presetBtn,
                         QMenu *presetMenu, QToolButton *outBtn, QToolButton *inBtn,
                         QSlider *slider);
    void setCanvasLabel(QLabel *label);
    void setToolLabel(QLabel *label);

    void applyZoomFromEdit();
    void applyZoomPreset(int pct);

public slots:
    void onPageSwitched(const QString &pageId, const QString &pageType);
    void onMousePositionChanged(const QPointF &scenePos);
    void onZoomChanged(qreal level);
    void onToolChanged(Tool tool);

private:
    QLabel *m_posLabel = nullptr;
    QLineEdit *m_zoomEdit = nullptr;
    QLabel *m_zoomPctLabel = nullptr;
    QToolButton *m_zoomPresetBtn = nullptr;
    QMenu *m_zoomPresetMenu = nullptr;
    QToolButton *m_zoomOutBtn = nullptr;
    QToolButton *m_zoomInBtn = nullptr;
    QSlider *m_zoomSlider = nullptr;
    QLabel *m_canvasLabel = nullptr;
    QLabel *m_toolLabel = nullptr;

    static qreal _sliderToZoom(int sliderValue);
    static int _zoomToSliderValue(qreal zoom);
};

#endif // STATUSBARDIRECTOR_H
