#include "StatusBarDirector.h"
#include "atMath.h"
#include "AppContext.h"
#include "QAtCanvasPage.h"
#include "qatgraphicsview.h"

#include <QLabel>
#include <QComboBox>
#include <QToolButton>
#include <QSlider>
#include <QLineEdit>
#include <Qt>
#include <cmath>

StatusBarDirector::StatusBarDirector(QObject *parent) : QObject(parent) { }

void StatusBarDirector::setPositionLabel(QLabel *label)
{
    m_posLabel = label;
}

void StatusBarDirector::setZoomControls(QComboBox *combo, QToolButton *outBtn,
                                         QToolButton *inBtn, QSlider *slider)
{
    m_zoomCombo = combo;
    m_zoomOutBtn = outBtn;
    m_zoomInBtn = inBtn;
    m_zoomSlider = slider;
}

void StatusBarDirector::setCanvasLabel(QLabel *label)
{
    m_canvasLabel = label;
}

void StatusBarDirector::setToolLabel(QLabel *label)
{
    m_toolLabel = label;
}

// ---- Log-slider mapping ----

qreal StatusBarDirector::_sliderToZoom(int sliderValue)
{
    return 0.01 * std::pow(3200.0, sliderValue / 100.0);
}

int StatusBarDirector::_zoomToSliderValue(qreal zoom)
{
    if (zoom <= 0.01) return 0;
    return qRound(100.0 * std::log(zoom / 0.01) / std::log(3200.0));
}

// ---- Combo input ----

void StatusBarDirector::applyZoomFromCombo()
{
    if (!m_zoomCombo) return;
    auto *p = AppContext::get().activeCanvasPage();
    if (!p || !p->view()) return;

    QString text = m_zoomCombo->currentText().remove(QLatin1Char('%')).trimmed();
    bool ok = false;
    int pct = text.toInt(&ok);
    if (!ok || pct < 1) {
        onZoomChanged(p->view()->zoomLevel());
        return;
    }
    pct = qBound(1, pct, 3200);
    qreal level = pct / 100.0;
    p->view()->setZoomLevel(level);
}

void StatusBarDirector::applyPresetFromCombo(int index)
{
    if (!m_zoomCombo) return;
    QVariant data = m_zoomCombo->itemData(index);
    if (!data.isValid()) return;

    auto *p = AppContext::get().activeCanvasPage();
    if (!p || !p->view()) return;

    if (data.type() == QVariant::String) {
        QString action = data.toString();
        if (action == QStringLiteral("fit")) {
            p->view()->fitToCanvas();
        } else if (action == QStringLiteral("fit-selection")) {
            p->view()->fitToSelection();
        }
    } else {
        int pct = data.toInt();
        p->view()->setZoomLevel(pct / 100.0);
    }

    // After zoom applied, sync combo text to the resulting percentage
    // (e.g. "Fit to Canvas" → "45%")
    onZoomChanged(p->view()->zoomLevel());
}

// ---- Page switching ----

void StatusBarDirector::onPageSwitched(const QString &, const QString &pageType)
{
    // Disconnect previous page signals
    auto pages = AppContext::get().allPages();
    for (auto *p : pages) {
        auto *cp = qobject_cast<QAtCanvasPage *>(p);
        if (cp) cp->disconnect(this);
    }

    bool isCanvas = (pageType == QStringLiteral("canvas"));
    if (m_zoomCombo) m_zoomCombo->setEnabled(isCanvas);
    if (m_zoomOutBtn) m_zoomOutBtn->setEnabled(isCanvas);
    if (m_zoomInBtn) m_zoomInBtn->setEnabled(isCanvas);
    if (m_zoomSlider) m_zoomSlider->setEnabled(isCanvas);

    if (isCanvas) {
        auto *p = AppContext::get().activeCanvasPage();
        if (p) {
            connect(p, &QAtCanvasPage::mousePositionChanged, this,
                    &StatusBarDirector::onMousePositionChanged);
            connect(p, &QAtCanvasPage::zoomChanged, this,
                    &StatusBarDirector::onZoomChanged);
            connect(p, &QAtCanvasPage::toolChanged, this,
                    &StatusBarDirector::onToolChanged);
            // Sync controls to current zoom
            if (p->view()) onZoomChanged(p->view()->zoomLevel());
        }
    }
}

// ---- Signal handlers ----

void StatusBarDirector::onMousePositionChanged(const QPointF &scenePos)
{
    if (!m_posLabel) return;
    m_posLabel->setText(
        QStringLiteral("X: %1  Y: %2").arg(scenePos.x(), 0, 'f', 2).arg(scenePos.y(), 0, 'f', 2));
}

void StatusBarDirector::onZoomChanged(qreal level)
{
    int pct = qRound(level * 100.0);
    if (m_zoomCombo) {
        m_zoomCombo->blockSignals(true);
        m_zoomCombo->setCurrentText(QString::number(pct) + QStringLiteral("%"));
        m_zoomCombo->blockSignals(false);
    }
    if (m_zoomSlider) {
        m_zoomSlider->blockSignals(true);
        m_zoomSlider->setValue(_zoomToSliderValue(level));
        m_zoomSlider->blockSignals(false);
    }
}

void StatusBarDirector::onToolChanged(Tool tool)
{
    if (!m_toolLabel) return;
    static const QMap<int, QString> names = {
        { 0, tr("Select") }, { 1, tr("Hand") },   { 2, tr("Rectangle") }, { 3, tr("Ellipse") },
        { 4, tr("Line") },   { 5, tr("Bezier") }, { 6, tr("Freehand") },  { 7, tr("Text") },
    };
    m_toolLabel->setText(tr("Tool: %1").arg(names.value(static_cast<int>(tool), tr("Unknown"))));
}
