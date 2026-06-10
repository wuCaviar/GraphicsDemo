#include "AutoLayoutDialog.h"
#include "AdvancedLayoutDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

AutoLayoutDialog::AutoLayoutDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Auto Layout"));
    setMinimumWidth(360);

    auto *mainLayout = new QVBoxLayout(this);

    // --- Layout Parameters ---
    auto *paramGroup = new QGroupBox(tr("Layout Parameters"), this);
    auto *formLayout = new QFormLayout(paramGroup);

    // Paper width (with CheckBox toggle)
    auto *paperWidthRow = new QHBoxLayout();
    m_paperWidthSpin = new QSpinBox(this);
    m_paperWidthSpin->setRange(800, 2000);
    m_paperWidthSpin->setValue(1600);
    m_paperWidthSpin->setSingleStep(5);
    m_paperWidthSpin->setSuffix(tr(" mm"));
    m_paperWidthSpin->setToolTip(tr("Layout canvas width, in millimeters"));
    paperWidthRow->addWidget(m_paperWidthSpin);

    m_paperWidthCheck = new QCheckBox(tr("Use Paper Count"), this);
    m_paperWidthCheck->setChecked(false);
    m_paperWidthCheck->setToolTip(
        tr("Use custom paper width when checked; otherwise auto-determined by the layout library"));
    paperWidthRow->addWidget(m_paperWidthCheck);
    formLayout->addRow(tr("Paper Width"), paperWidthRow);

    // Element interval (integer SpinBox, range 5–10 mm, consistent with ATComposingPic_main)
    m_intervalSpin = new QSpinBox(this);
    m_intervalSpin->setRange(5, 10);
    m_intervalSpin->setValue(5);
    m_intervalSpin->setSingleStep(1);
    m_intervalSpin->setSuffix(tr(" mm"));
    m_intervalSpin->setToolTip(tr("Minimum spacing between images"));
    formLayout->addRow(tr("Element Interval"), m_intervalSpin);

    // Layout threads (CPU core count)
    m_loopCountSpin = new QSpinBox(this);
    m_loopCountSpin->setRange(1, 24);
    m_loopCountSpin->setValue(8);
    m_loopCountSpin->setToolTip(
        tr("Number of parallel layout threads; recommended: set to CPU core count"));
    formLayout->addRow(tr("Thread Count"), m_loopCountSpin);

    // Operate count
    m_operateCountSpin = new QSpinBox(this);
    m_operateCountSpin->setRange(1, 20);
    m_operateCountSpin->setValue(10);
    m_operateCountSpin->setToolTip(tr("Number of images to arrange per operation"));
    formLayout->addRow(tr("Operate Count"), m_operateCountSpin);

    mainLayout->addWidget(paramGroup);

    // --- Advanced Settings button ---
    m_advancedBtn = new QPushButton(tr("Advanced Settings..."), this);
    m_advancedBtn->setToolTip(tr("Configure advanced layout library parameters such as resolution "
                                 "thresholds and color marking"));
    connect(m_advancedBtn, &QPushButton::clicked, this, &AutoLayoutDialog::openAdvancedSettings);
    mainLayout->addWidget(m_advancedBtn);

    // --- Hint ---
    auto *hintLabel = new QLabel(
        tr("Auto-layout all image items on the canvas using the above parameters.\n"
           "After layout is complete, original items will be replaced with layout result images."),
        this);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet("color: #666; font-size: 12pt;");
    mainLayout->addWidget(hintLabel);

    mainLayout->addStretch();

    // --- Buttons ---
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Start Layout"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

int AutoLayoutDialog::paperWidth() const
{
    return m_paperWidthCheck->isChecked() ? m_paperWidthSpin->value() : 0;
}

int AutoLayoutDialog::elementInterval() const
{
    return m_intervalSpin->value();
}

int AutoLayoutDialog::loopCount() const
{
    return m_loopCountSpin->value();
}

int AutoLayoutDialog::operateCount() const
{
    return m_operateCountSpin->value();
}

bool AutoLayoutDialog::usePaperWidth() const
{
    return m_paperWidthCheck->isChecked();
}

void AutoLayoutDialog::openAdvancedSettings()
{
    AdvancedLayoutDialog dlg(this);
    dlg.exec();
}
