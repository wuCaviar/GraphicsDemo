#include "NewFileDialog.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

// 打印行业常用尺寸 (单位: mm)
struct PrintSizePreset
{
    QString name;
    qreal widthMM;
    qreal heightMM;
};

static const PrintSizePreset kPresets[] = {
    { "A3 (297 × 420 mm)",      297,    420 },
    { "A4 (210 × 297 mm)",      210,    297 },
    { "A5 (148 × 210 mm)",      148,    210 },
    { "B3 (353 × 500 mm)",      353,    500 },
    { "B4 (250 × 353 mm)",      250,    353 },
    { "B5 (176 × 250 mm)",      176,    250 },
    { "16K (195 × 270 mm)",     195,    270 },
    { "32K (130 × 185 mm)",     130,    185 },
    { "8K (270 × 390 mm)",      270,    390 },
    { "Letter (216 × 279 mm)",  215.9,  279.4 },
    { "Legal (216 × 356 mm)",   215.9,  355.6 },
    { "Photo 3R (89 × 127 mm)", 89,     127 },
    { "Photo 4R (102 × 152 mm)",102,    152 },
    { "Photo 5R (127 × 178 mm)",127,    178 },
    { "Photo 6R (152 × 203 mm)",152,    203 },
    { "Photo 8R (203 × 254 mm)",203,    254 },
    { "Custom",                  0,      0 },
};

// 常用 PPI 预设
struct PpiPreset
{
    QString name;
    qreal ppi;
};

static const PpiPreset kPpiPresets[] = {
    { "72 PPI (Screen/Web)",     72 },
    { "96 PPI (Windows)",        96 },
    { "150 PPI (Draft Print)",   150 },
    { "300 PPI (Quality Print)", 300 },
    { "600 PPI (High Quality)",  600 },
};

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
    for (const auto &p : kPresets)
        m_presetCombo->addItem(p.name);
    formLayout->addRow(tr("Preset:"), m_presetCombo);

    m_widthSpin = new QDoubleSpinBox;
    m_widthSpin->setRange(1, 99999);
    m_widthSpin->setDecimals(1);
    m_widthSpin->setSuffix(tr(" mm"));
    formLayout->addRow(tr("Width:"), m_widthSpin);

    m_heightSpin = new QDoubleSpinBox;
    m_heightSpin->setRange(1, 99999);
    m_heightSpin->setDecimals(1);
    m_heightSpin->setSuffix(tr(" mm"));
    formLayout->addRow(tr("Height:"), m_heightSpin);

    // PPI 设置
    m_ppiSpin = new QDoubleSpinBox;
    m_ppiSpin->setRange(1, 9999);
    m_ppiSpin->setDecimals(0);
    m_ppiSpin->setSuffix(tr(" PPI"));
    m_ppiSpin->setValue(300);
    m_ppiSpin->setToolTip(tr("Pixels Per Inch — affects mm↔px conversion"));
    formLayout->addRow(tr("Resolution:"), m_ppiSpin);

    mainLayout->addWidget(group);

    // 按钮
    auto *btnLayout = new QHBoxLayout;
    auto *okBtn = new QPushButton(tr("OK"));
    okBtn->setToolTip(tr("Create a new canvas"));
    auto *cancelBtn = new QPushButton(tr("Cancel"));
    cancelBtn->setToolTip(tr("Cancel and close the dialog"));
    btnLayout->addStretch();
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    mainLayout->addLayout(btnLayout);

    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &NewFileDialog::onPresetChanged);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    // 默认选中 A4
    m_presetCombo->setCurrentIndex(1);
    onPresetChanged(1);
}

void NewFileDialog::onPresetChanged(int index)
{
    if (index < 0 || index >= int(sizeof(kPresets) / sizeof(kPresets[0])))
        return;

    const auto &preset = kPresets[index];
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

QSizeF NewFileDialog::selectedSize() const
{
    // 将 mm 转为 px（基于用户设定的 PPI）
    qreal mmToPx = m_ppiSpin->value() / 25.4;
    return QSizeF(m_widthSpin->value() * mmToPx, m_heightSpin->value() * mmToPx);
}

qreal NewFileDialog::selectedPpi() const
{
    return m_ppiSpin->value();
}

QString NewFileDialog::selectedPresetName() const
{
    return m_presetCombo->currentText();
}
