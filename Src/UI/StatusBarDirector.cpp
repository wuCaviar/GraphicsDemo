#include "StatusBarDirector.h"
#include "AppContext.h"
#include "QAtCanvasPage.h"

#include <QLabel>
#include <QSlider>
#include <Qt>

StatusBarDirector::StatusBarDirector(QObject *parent)
    : QObject(parent)
{
}

void StatusBarDirector::setPositionLabel(QLabel *label)  { m_posLabel = label; }
void StatusBarDirector::setZoomControls(QLabel *label, QSlider *slider)
{
    m_zoomLabel = label;
    m_zoomSlider = slider;
}
void StatusBarDirector::setCanvasLabel(QLabel *label)    { m_canvasLabel = label; }
void StatusBarDirector::setToolLabel(QLabel *label)      { m_toolLabel = label; }

void StatusBarDirector::onPageSwitched(const QString &, const QString &pageType)
{
    // Disconnect old page signals
    auto pages = AppContext::get().allPages();
    for (auto *p : pages) {
        auto *cp = qobject_cast<QAtCanvasPage*>(p);
        if (cp) {
            cp->disconnect(this);
        }
    }

    // Bind to new active canvas page
    if (pageType == QStringLiteral("canvas")) {
        auto *p = AppContext::get().activeCanvasPage();
        if (p) {
            connect(p, &QAtCanvasPage::mousePositionChanged,
                    this, &StatusBarDirector::onMousePositionChanged);
            connect(p, &QAtCanvasPage::zoomChanged,
                    this, &StatusBarDirector::onZoomChanged);
            connect(p, &QAtCanvasPage::toolChanged,
                    this, &StatusBarDirector::onToolChanged);
        }
    }
}

void StatusBarDirector::onMousePositionChanged(const QPointF &scenePos)
{
    if (!m_posLabel) return;
    m_posLabel->setText(QStringLiteral("X: %1  Y: %2")
                            .arg(scenePos.x(), 0, 'f', 2)
                            .arg(scenePos.y(), 0, 'f', 2));
}

void StatusBarDirector::onZoomChanged(qreal level)
{
    if (m_zoomLabel)
        m_zoomLabel->setText(QString::number(qRound(level * 100.0)) + QStringLiteral("%"));
    if (m_zoomSlider) {
        m_zoomSlider->blockSignals(true);
        m_zoomSlider->setValue(qRound(level * 100.0));
        m_zoomSlider->blockSignals(false);
    }
}

void StatusBarDirector::onToolChanged(Tool tool)
{
    if (!m_toolLabel) return;
    static const QMap<int, QString> names = {
        {0, tr("Select")}, {1, tr("Hand")},    {2, tr("Rectangle")},
        {3, tr("Ellipse")}, {4, tr("Line")},    {5, tr("Bezier")},
        {6, tr("Freehand")}, {7, tr("Text")},
    };
    m_toolLabel->setText(tr("Tool: %1").arg(names.value(static_cast<int>(tool), tr("Unknown"))));
}
