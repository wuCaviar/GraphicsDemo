#include "ExportImageDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QDialogButtonBox>

namespace ImageUtils {

ExportImageDialog::ExportImageDialog(QWidget *parent,
                                       const ExportParameters &params,
                                       const QString &format)
    : QDialog(parent)
{
    setWindowTitle(tr("Export Image - Parameters"));
    setModal(true);

    auto *layout = new QFormLayout(this);

    // 分辨率/DPI 设置
    m_dpiSpin = new QSpinBox(this);
    m_dpiSpin->setRange(1, 1200);
    layout->addRow(tr("DPI:"), m_dpiSpin);

    // 压缩设置
    m_compressionCombo = new QComboBox(this);
    m_compressionCombo->addItem(tr("None"), static_cast<int>(ExportParameters::CompressionType::None));
    m_compressionCombo->addItem(tr("LZW"), static_cast<int>(ExportParameters::CompressionType::LZW));
    m_compressionCombo->addItem(tr("ZIP/DEFLATE"), static_cast<int>(ExportParameters::CompressionType::ZIP));
    m_compressionCombo->addItem(tr("JPEG"), static_cast<int>(ExportParameters::CompressionType::JPEG));
    connect(m_compressionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ExportImageDialog::onCompressionChanged);
    layout->addRow(tr("Compression:"), m_compressionCombo);

    // JPEG 质量
    m_jpegQualitySpin = new QSpinBox(this);
    m_jpegQualitySpin->setRange(1, 100);
    m_jpegQualitySpin->setValue(95);
    connect(m_jpegQualitySpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ExportImageDialog::onQualityChanged);
    layout->addRow(tr("JPEG Quality:"), m_jpegQualitySpin);

    m_jpegQualitySlider = new QSlider(Qt::Horizontal, this);
    m_jpegQualitySlider->setRange(1, 100);
    m_jpegQualitySlider->setValue(95);
    connect(m_jpegQualitySlider, &QSlider::valueChanged,
            m_jpegQualitySpin, &QSpinBox::setValue);
    connect(m_jpegQualitySpin, QOverload<int>::of(&QSpinBox::valueChanged),
            m_jpegQualitySlider, &QSlider::setValue);
    layout->addRow(tr(""), m_jpegQualitySlider);

    m_qualityLabel = new QLabel("95%", this);
    layout->addRow(tr(""), m_qualityLabel);

    // PNG 压缩级别
    m_pngCompressionSpin = new QSpinBox(this);
    m_pngCompressionSpin->setRange(0, 9);
    m_pngCompressionSpin->setValue(6);
    layout->addRow(tr("PNG Compression (0-9):"), m_pngCompressionSpin);

    // 颜色空间
    m_colorCombo = new QComboBox(this);
    m_colorCombo->addItem(tr("Keep Original"), static_cast<int>(ExportParameters::ColorSpace::KeepOriginal));
    m_colorCombo->addItem(tr("Convert to sRGB"), static_cast<int>(ExportParameters::ColorSpace::ConvertToSRGB));
    m_colorCombo->addItem(tr("Convert to Adobe RGB"), static_cast<int>(ExportParameters::ColorSpace::ConvertToAdobeRGB));
    layout->addRow(tr("Color Space:"), m_colorCombo);

    // 元数据处理
    m_metadataCheck = new QCheckBox(tr("Preserve Metadata (EXIF, XMP)"), this);
    m_metadataCheck->setChecked(true);
    layout->addRow(tr(""), m_metadataCheck);

    m_iccCheck = new QCheckBox(tr("Preserve ICC Profile"), this);
    m_iccCheck->setChecked(true);
    layout->addRow(tr(""), m_iccCheck);

    // 透明度处理
    m_transparencyCombo = new QComboBox(this);
    m_transparencyCombo->addItem(tr("Keep Transparency"), static_cast<int>(ExportParameters::TransparencyHandling::Keep));
    m_transparencyCombo->addItem(tr("Flatten on White"), static_cast<int>(ExportParameters::TransparencyHandling::FlattenOnWhite));
    layout->addRow(tr("Transparency:"), m_transparencyCombo);

    // TIFF 特定选项
    QGroupBox *tiffGroup = new QGroupBox(tr("TIFF Options"), this);
    auto *tiffLayout = new QFormLayout(tiffGroup);
    m_zipDiffCheck = new QCheckBox(tr("ZIP Horizontal Differencing"), this);
    m_zipDiffCheck->setChecked(false);
    tiffLayout->addRow(tr(""), m_zipDiffCheck);
    layout->addRow(tr(""), tiffGroup);

    // 按钮
    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addRow(buttonBox);

    // 设置初始参数
    setParameters(params);
    onFormatChanged(format);
}

ExportParameters ExportImageDialog::getParameters() const
{
    ExportParameters params;
    params.dpi = m_dpiSpin->value();
    params.compression = static_cast<ExportParameters::CompressionType>(
        m_compressionCombo->currentData().toInt());
    params.jpegQuality = m_jpegQualitySpin->value();
    params.pngCompression = m_pngCompressionSpin->value();
    params.colorSpace = static_cast<ExportParameters::ColorSpace>(
        m_colorCombo->currentData().toInt());
    params.preserveMetadata = m_metadataCheck->isChecked();
    params.preserveICCProfile = m_iccCheck->isChecked();
    params.tiffZipHorizontalDifferencing = m_zipDiffCheck->isChecked();
    params.transparency = static_cast<ExportParameters::TransparencyHandling>(
        m_transparencyCombo->currentData().toInt());
    return params;
}

void ExportImageDialog::setParameters(const ExportParameters &params)
{
    m_dpiSpin->setValue(params.dpi);

    int compIndex = m_compressionCombo->findData(static_cast<int>(params.compression));
    m_compressionCombo->setCurrentIndex(qMax(0, compIndex));

    m_jpegQualitySpin->setValue(params.jpegQuality);
    m_pngCompressionSpin->setValue(params.pngCompression);

    int colorIndex = m_colorCombo->findData(static_cast<int>(params.colorSpace));
    m_colorCombo->setCurrentIndex(qMax(0, colorIndex));

    m_metadataCheck->setChecked(params.preserveMetadata);
    m_iccCheck->setChecked(params.preserveICCProfile);
    m_zipDiffCheck->setChecked(params.tiffZipHorizontalDifferencing);

    int transIndex = m_transparencyCombo->findData(static_cast<int>(params.transparency));
    m_transparencyCombo->setCurrentIndex(qMax(0, transIndex));
}

QString ExportImageDialog::getSelectedFormat() const
{
    // This should be passed from the file dialog
    return QString();
}

void ExportImageDialog::onCompressionChanged(int index)
{
    ExportParameters::CompressionType type =
        static_cast<ExportParameters::CompressionType>(
            m_compressionCombo->itemData(index).toInt());

    bool isJpeg = (type == ExportParameters::CompressionType::JPEG);
    m_jpegQualitySpin->setEnabled(isJpeg);
    m_jpegQualitySlider->setEnabled(isJpeg);
}

void ExportImageDialog::onQualityChanged(int value)
{
    m_qualityLabel->setText(QString("%1%").arg(value));
}

void ExportImageDialog::onFormatChanged(const QString &format)
{
    QString fmt = format.toLower();
    bool isTiff = fmt.contains("tif");
    bool isJpeg = fmt.contains("jpg") || fmt.contains("jpeg");
    bool isPng = fmt.contains("png");

    // 根据格式启用/禁用选项
    m_compressionCombo->setEnabled(isTiff || isPng);
    m_jpegQualitySpin->setEnabled(isJpeg);
    m_jpegQualitySlider->setEnabled(isJpeg);
    m_pngCompressionSpin->setEnabled(isPng);
}

} // namespace ImageUtils
