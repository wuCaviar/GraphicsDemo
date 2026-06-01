#include "NewFileDialog.h"
#include "ATHCPresets.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>

NewFileDialog::NewFileDialog(QWidget *parent) : QDialog(parent)
{
    setupUI();
}

void NewFileDialog::setupUI()
{
    setWindowTitle(tr("New Canvas"));
    setMinimumWidth(360);

    auto *mainLayout = new QVBoxLayout(this);

    // 预设尺寸
    auto *group = new QGroupBox(tr("Canvas Size"));
    auto *formLayout = new QFormLayout(group);

    m_presetCombo = new QComboBox;
    for (const auto &p : kCanvasPresets)
        m_presetCombo->addItem(p.name);
    formLayout->addRow(tr("Preset:"), m_presetCombo);

    m_widthSpin = new QDoubleSpinBox;
    m_widthSpin->setRange(1, 99999);
    m_widthSpin->setDecimals(1);
    m_widthSpin->setSuffix(QStringLiteral(" mm"));
    formLayout->addRow(tr("Width:"), m_widthSpin);

    m_heightSpin = new QDoubleSpinBox;
    m_heightSpin->setRange(1, 99999);
    m_heightSpin->setDecimals(1);
    m_heightSpin->setSuffix(QStringLiteral(" mm"));
    formLayout->addRow(tr("Height:"), m_heightSpin);

    mainLayout->addWidget(group);

    // 按钮
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttonBox->button(QDialogButtonBox::Ok)->setToolTip(tr("Create a new canvas"));
    buttonBox->button(QDialogButtonBox::Cancel)->setToolTip(tr("Cancel and close the dialog"));
    mainLayout->addWidget(buttonBox);

    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &NewFileDialog::onPresetChanged);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // 默认选中 A4
    m_presetCombo->setCurrentIndex(1);
    onPresetChanged(1);
}

void NewFileDialog::onPresetChanged(int index)
{
    if (index < 0 || index >= int(sizeof(kCanvasPresets) / sizeof(kCanvasPresets[0])))
        return;

    const auto &preset = kCanvasPresets[index];
    if (preset.widthMM > 0 && preset.heightMM > 0) {
        m_widthSpin->setValue(preset.widthMM);
        m_heightSpin->setValue(preset.heightMM);
        m_widthSpin->setEnabled(false);
        m_heightSpin->setEnabled(false);
    } else {
        // Custom
        m_widthSpin->setEnabled(true);
        m_heightSpin->setEnabled(true);
    }
}

QSizeF NewFileDialog::selectedSizeMM() const
{
    // 直接返回 mm 尺寸
    return QSizeF(m_widthSpin->value(), m_heightSpin->value());
}

QString NewFileDialog::selectedPresetName() const
{
    return m_presetCombo->currentText();
}
