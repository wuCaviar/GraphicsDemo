#include "CanvasPage.h"
#include "AppConfig.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

CanvasPage::CanvasPage(QWidget *parent) : PreferencesPage(parent)
{
    setupUI();
}

QString CanvasPage::title() const
{
    return tr("Canvas");
}

void CanvasPage::setupUI()
{
    auto *layout = new QVBoxLayout(this);

    auto *group = new QGroupBox(tr("Canvas Margins (applied when fitting canvas to items)"), this);
    auto *formLayout = new QFormLayout(group);

    auto createSpin = [&](const QString &tooltip) -> QDoubleSpinBox * {
        auto *spin = new QDoubleSpinBox(group);
        spin->setRange(0.0, 100.0);
        spin->setDecimals(2);
        spin->setSingleStep(0.5);
        spin->setSuffix(tr(" mm"));
        spin->setToolTip(tooltip);
        return spin;
    };

    m_marginLeftSpin = createSpin(tr("Canvas left margin"));
    m_marginRightSpin = createSpin(tr("Canvas right margin"));
    m_marginTopSpin = createSpin(tr("Canvas top margin"));
    m_marginBottomSpin = createSpin(tr("Canvas bottom margin"));

    formLayout->addRow(tr("Left:"), m_marginLeftSpin);
    formLayout->addRow(tr("Right:"), m_marginRightSpin);
    formLayout->addRow(tr("Top:"), m_marginTopSpin);
    formLayout->addRow(tr("Bottom:"), m_marginBottomSpin);

    layout->addWidget(group);
    layout->addStretch();
}

void CanvasPage::load()
{
    const AppConfig &cfg = AppConfig::instance();
    m_marginLeftSpin->setValue(cfg.canvasMarginLeft());
    m_marginRightSpin->setValue(cfg.canvasMarginRight());
    m_marginTopSpin->setValue(cfg.canvasMarginTop());
    m_marginBottomSpin->setValue(cfg.canvasMarginBottom());
}

bool CanvasPage::save()
{
    AppConfig &cfg = AppConfig::instance();
    cfg.setCanvasMarginLeft(m_marginLeftSpin->value());
    cfg.setCanvasMarginRight(m_marginRightSpin->value());
    cfg.setCanvasMarginTop(m_marginTopSpin->value());
    cfg.setCanvasMarginBottom(m_marginBottomSpin->value());
    cfg.saveConfig();
    return true;
}
