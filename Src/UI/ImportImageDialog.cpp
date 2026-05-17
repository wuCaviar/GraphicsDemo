#include "ImportImageDialog.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QSlider>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QComboBox>

namespace ImageUtils {

ImportImageDialog::ImportImageDialog(QWidget *parent, const ImportParameters &params)
    : QDialog(parent)
{
    setWindowTitle(tr("Import Image - Parameters"));
    setModal(true);

    auto *layout = new QFormLayout(this);

    // DPI 策略
    m_dpiCombo = new QComboBox(this);
    m_dpiCombo->addItem(tr("Use Image DPI"), static_cast<int>(ImportParameters::DpiPolicy::UseImageDpi));
    m_dpiCombo->addItem(tr("Force DPI"), static_cast<int>(ImportParameters::DpiPolicy::ForceDpi));
    m_dpiCombo->addItem(tr("Ignore DPI (72 DPI)"), static_cast<int>(ImportParameters::DpiPolicy::IgnoreDpi));
    connect(m_dpiCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ImportImageDialog::onDpiPolicyChanged);
    layout->addRow(tr("DPI Policy:"), m_dpiCombo);

    // 强制 DPI 值
    m_dpiSpin = new QSpinBox(this);
    m_dpiSpin->setRange(1, 1200);
    m_dpiSpin->setSuffix(tr(" DPI"));
    layout->addRow(tr("Forced DPI:"), m_dpiSpin);

    // 颜色空间
    m_colorCombo = new QComboBox(this);
    m_colorCombo->addItem(tr("Keep Original"), static_cast<int>(ImportParameters::ColorSpace::KeepOriginal));
    m_colorCombo->addItem(tr("Convert to sRGB"), static_cast<int>(ImportParameters::ColorSpace::ConvertToSRGB));
    m_colorCombo->addItem(tr("Convert to Adobe RGB"), static_cast<int>(ImportParameters::ColorSpace::ConvertToAdobeRGB));
    layout->addRow(tr("Color Space:"), m_colorCombo);

    // Alpha 通道处理
    m_alphaCombo = new QComboBox(this);
    m_alphaCombo->addItem(tr("Keep Alpha"), static_cast<int>(ImportParameters::AlphaHandling::Keep));
    m_alphaCombo->addItem(tr("Discard Alpha (replace with white)"), static_cast<int>(ImportParameters::AlphaHandling::Discard));
    m_alphaCombo->addItem(tr("Premultiply Alpha"), static_cast<int>(ImportParameters::AlphaHandling::Premultiply));
    layout->addRow(tr("Alpha Handling:"), m_alphaCombo);

    // 图像缩放
    m_scaleCheck = new QCheckBox(tr("Enable Scaling"), this);
    layout->addRow(tr(""), m_scaleCheck);

    m_scaleSlider = new QSlider(Qt::Horizontal, this);
    m_scaleSlider->setRange(10, 400);
    m_scaleSlider->setValue(100);
    m_scaleSlider->setTickPosition(QSlider::TicksBelow);
    m_scaleSlider->setTickInterval(50);
    connect(m_scaleSlider, &QSlider::valueChanged, this, &ImportImageDialog::onScaleChanged);
    layout->addRow(tr("Scale:"), m_scaleSlider);

    m_scaleLabel = new QLabel("100%", this);
    layout->addRow(tr(""), m_scaleLabel);

    // TIFF 元数据
    m_tiffMetaCheck = new QCheckBox(tr("Preserve TIFF Metadata"), this);
    m_tiffMetaCheck->setChecked(true);
    layout->addRow(tr(""), m_tiffMetaCheck);

    // 按钮
    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Ok)->setToolTip(tr("Import the image with the selected settings"));
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addRow(buttonBox);

    // 设置初始参数
    setParameters(params);
}

ImportParameters ImportImageDialog::getParameters() const
{
    ImportParameters params;
    params.dpiPolicy = static_cast<ImportParameters::DpiPolicy>(
        m_dpiCombo->currentData().toInt());
    params.forcedDpi = m_dpiSpin->value();
    params.colorSpace = static_cast<ImportParameters::ColorSpace>(
        m_colorCombo->currentData().toInt());
    params.alphaHandling = static_cast<ImportParameters::AlphaHandling>(
        m_alphaCombo->currentData().toInt());
    params.enableScaling = m_scaleCheck->isChecked();
    params.scaleFactor = m_scaleSlider->value() / 100.0;
    params.preserveTiffMetadata = m_tiffMetaCheck->isChecked();
    return params;
}

void ImportImageDialog::setParameters(const ImportParameters &params)
{
    int dpiIndex = m_dpiCombo->findData(static_cast<int>(params.dpiPolicy));
    m_dpiCombo->setCurrentIndex(qMax(0, dpiIndex));
    m_dpiSpin->setValue(params.forcedDpi);

    int colorIndex = m_colorCombo->findData(static_cast<int>(params.colorSpace));
    m_colorCombo->setCurrentIndex(qMax(0, colorIndex));

    int alphaIndex = m_alphaCombo->findData(static_cast<int>(params.alphaHandling));
    m_alphaCombo->setCurrentIndex(qMax(0, alphaIndex));

    m_scaleCheck->setChecked(params.enableScaling);
    m_scaleSlider->setValue(qRound(params.scaleFactor * 100));
    m_tiffMetaCheck->setChecked(params.preserveTiffMetadata);
}

void ImportImageDialog::onDpiPolicyChanged(int index)
{
    bool enable = m_dpiCombo->itemData(index).toInt() ==
                  static_cast<int>(ImportParameters::DpiPolicy::ForceDpi);
    m_dpiSpin->setEnabled(enable);
}

void ImportImageDialog::onScaleChanged(int value)
{
    m_scaleLabel->setText(QString("%1%").arg(value));
}

} // namespace ImageUtils
