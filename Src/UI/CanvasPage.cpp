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
    return tr("画布");
}

void CanvasPage::setupUI()
{
    auto *layout = new QVBoxLayout(this);

    auto *group = new QGroupBox(tr("画布留白（适配图元时生效）"), this);
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

    m_marginLeftSpin = createSpin(tr("画布左侧留白"));
    m_marginRightSpin = createSpin(tr("画布右侧留白"));
    m_marginTopSpin = createSpin(tr("画布上方留白"));
    m_marginBottomSpin = createSpin(tr("画布下方留白"));

    formLayout->addRow(tr("左侧留白:"), m_marginLeftSpin);
    formLayout->addRow(tr("右侧留白:"), m_marginRightSpin);
    formLayout->addRow(tr("上方留白:"), m_marginTopSpin);
    formLayout->addRow(tr("下方留白:"), m_marginBottomSpin);

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
