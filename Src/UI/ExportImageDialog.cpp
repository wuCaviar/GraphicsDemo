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
    , m_tiffCompressionCombo(nullptr)
    , m_tiffByteOrderCombo(nullptr)
    , m_tiffBitDepthCombo(nullptr)
    , m_tiffPlanarConfigCombo(nullptr)
    , m_tiffPredictorCombo(nullptr)
    , m_tiffICCCheck(nullptr)
    , m_tiffMetadataCheck(nullptr)
    , m_tiffJpegQualitySpin(nullptr)
    , m_tiffJpegQualitySlider(nullptr)
    , m_tiffJpegQualityLabel(nullptr)
    , m_pngCompressionSpin(nullptr)
    , m_jpegQualitySpin(nullptr)
    , m_jpegQualitySlider(nullptr)
    , m_jpegQualityLabel(nullptr)
{
    setWindowTitle(tr("Export Image - Parameters"));
    setModal(true);

    auto *mainLayout = new QFormLayout(this);

    // ===== 通用参数 =====
    m_dpiSpin = new QSpinBox(this);
    m_dpiSpin->setRange(1, 1200);
    mainLayout->addRow(tr("DPI:"), m_dpiSpin);

    m_colorCombo = new QComboBox(this);
    m_colorCombo->addItem(tr("Keep Original"),
                          static_cast<int>(ExportParameters::ColorSpace::KeepOriginal));
    m_colorCombo->addItem(tr("Convert to sRGB"),
                          static_cast<int>(ExportParameters::ColorSpace::ConvertToSRGB));
    m_colorCombo->addItem(tr("Convert to Adobe RGB"),
                          static_cast<int>(ExportParameters::ColorSpace::ConvertToAdobeRGB));
    mainLayout->addRow(tr("Color Space:"), m_colorCombo);

    m_transparencyCombo = new QComboBox(this);
    m_transparencyCombo->addItem(tr("Keep Transparency"),
                                 static_cast<int>(ExportParameters::TransparencyHandling::Keep));
    m_transparencyCombo->addItem(tr("Flatten on White"),
                                 static_cast<int>(ExportParameters::TransparencyHandling::FlattenOnWhite));
    mainLayout->addRow(tr("Transparency:"), m_transparencyCombo);

    // ===== 格式专属参数区域 =====
    m_formatGroup = new QGroupBox(this);
    m_formatLayout = new QFormLayout(m_formatGroup);
    mainLayout->addRow(m_formatGroup);

    // 根据格式构建专属控件
    rebuildFormatGroup(format);

    // ===== 按钮 =====
    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addRow(buttonBox);

    // 加载参数
    setParameters(params);
}

void ExportImageDialog::rebuildFormatGroup(const QString &format)
{
    // 清除旧控件
    while (m_formatLayout->count() > 0) {
        QLayoutItem *item = m_formatLayout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    // 重置指针
    m_tiffCompressionCombo = nullptr;
    m_tiffByteOrderCombo = nullptr;
    m_tiffBitDepthCombo = nullptr;
    m_tiffPlanarConfigCombo = nullptr;
    m_tiffPredictorCombo = nullptr;
    m_tiffICCCheck = nullptr;
    m_tiffMetadataCheck = nullptr;
    m_tiffJpegQualitySpin = nullptr;
    m_tiffJpegQualitySlider = nullptr;
    m_tiffJpegQualityLabel = nullptr;
    m_pngCompressionSpin = nullptr;
    m_jpegQualitySpin = nullptr;
    m_jpegQualitySlider = nullptr;
    m_jpegQualityLabel = nullptr;

    m_currentFormat = format.toLower();
    bool isTiff = m_currentFormat.contains("tif");
    bool isPng = m_currentFormat.contains("png");
    bool isJpeg = m_currentFormat.contains("jpg") || m_currentFormat.contains("jpeg");

    if (isTiff) {
        m_formatGroup->setTitle(tr("TIFF Professional Options"));
        m_formatGroup->setVisible(true);

        // 压缩方式
        m_tiffCompressionCombo = new QComboBox(m_formatGroup);
        m_tiffCompressionCombo->addItem(tr("None"),
            static_cast<int>(ExportParameters::CompressionType::None));
        m_tiffCompressionCombo->addItem(tr("LZW"),
            static_cast<int>(ExportParameters::CompressionType::LZW));
        m_tiffCompressionCombo->addItem(tr("ZIP/Deflate"),
            static_cast<int>(ExportParameters::CompressionType::ZIP));
        m_tiffCompressionCombo->addItem(tr("JPEG"),
            static_cast<int>(ExportParameters::CompressionType::JPEG));
        m_tiffCompressionCombo->addItem(tr("PackBits"),
            static_cast<int>(ExportParameters::CompressionType::PackBits));
        connect(m_tiffCompressionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &ExportImageDialog::onTiffCompressionChanged);
        m_formatLayout->addRow(tr("Compression:"), m_tiffCompressionCombo);

        // 字节序
        m_tiffByteOrderCombo = new QComboBox(m_formatGroup);
        m_tiffByteOrderCombo->addItem(tr("Little Endian (PC)"),
            static_cast<int>(ExportParameters::ByteOrder::LittleEndian));
        m_tiffByteOrderCombo->addItem(tr("Big Endian (Mac)"),
            static_cast<int>(ExportParameters::ByteOrder::BigEndian));
        m_formatLayout->addRow(tr("Byte Order:"), m_tiffByteOrderCombo);

        // 位深度
        m_tiffBitDepthCombo = new QComboBox(m_formatGroup);
        m_tiffBitDepthCombo->addItem(tr("8-bit"),
            static_cast<int>(ExportParameters::BitDepth::Bits8));
        m_tiffBitDepthCombo->addItem(tr("16-bit"),
            static_cast<int>(ExportParameters::BitDepth::Bits16));
        m_formatLayout->addRow(tr("Bit Depth:"), m_tiffBitDepthCombo);

        // 平面配置
        m_tiffPlanarConfigCombo = new QComboBox(m_formatGroup);
        m_tiffPlanarConfigCombo->addItem(tr("Interleaved (RGBRGB)"),
            static_cast<int>(ExportParameters::PlanarConfig::Contig));
        m_tiffPlanarConfigCombo->addItem(tr("Per-channel (RRGGBB)"),
            static_cast<int>(ExportParameters::PlanarConfig::Separate));
        m_formatLayout->addRow(tr("Pixel Order:"), m_tiffPlanarConfigCombo);

        // 预测器
        m_tiffPredictorCombo = new QComboBox(m_formatGroup);
        m_tiffPredictorCombo->addItem(tr("None"),
            static_cast<int>(ExportParameters::Predictor::None));
        m_tiffPredictorCombo->addItem(tr("Horizontal Differencing"),
            static_cast<int>(ExportParameters::Predictor::Horizontal));
        m_formatLayout->addRow(tr("Predictor:"), m_tiffPredictorCombo);

        // JPEG 质量（仅 JPEG 压缩时有效）
        m_tiffJpegQualitySpin = new QSpinBox(m_formatGroup);
        m_tiffJpegQualitySpin->setRange(1, 100);
        m_tiffJpegQualitySpin->setValue(95);
        connect(m_tiffJpegQualitySpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &ExportImageDialog::onQualityChanged);
        m_formatLayout->addRow(tr("JPEG Quality:"), m_tiffJpegQualitySpin);

        m_tiffJpegQualitySlider = new QSlider(Qt::Horizontal, m_formatGroup);
        m_tiffJpegQualitySlider->setRange(1, 100);
        m_tiffJpegQualitySlider->setValue(95);
        connect(m_tiffJpegQualitySlider, &QSlider::valueChanged,
                m_tiffJpegQualitySpin, &QSpinBox::setValue);
        connect(m_tiffJpegQualitySpin, QOverload<int>::of(&QSpinBox::valueChanged),
                m_tiffJpegQualitySlider, &QSlider::setValue);
        m_formatLayout->addRow(QString(), m_tiffJpegQualitySlider);

        m_tiffJpegQualityLabel = new QLabel("95%", m_formatGroup);
        m_formatLayout->addRow(QString(), m_tiffJpegQualityLabel);

        // 复选框
        m_tiffICCCheck = new QCheckBox(tr("Embed ICC Profile"), m_formatGroup);
        m_tiffICCCheck->setChecked(true);
        m_formatLayout->addRow(QString(), m_tiffICCCheck);

        m_tiffMetadataCheck = new QCheckBox(tr("Preserve Metadata"), m_formatGroup);
        m_tiffMetadataCheck->setChecked(true);
        m_formatLayout->addRow(QString(), m_tiffMetadataCheck);

        // 初始启用状态
        onTiffCompressionChanged(m_tiffCompressionCombo->currentIndex());

    } else if (isPng) {
        m_formatGroup->setTitle(tr("PNG Options"));
        m_formatGroup->setVisible(true);

        m_pngCompressionSpin = new QSpinBox(m_formatGroup);
        m_pngCompressionSpin->setRange(0, 9);
        m_pngCompressionSpin->setValue(6);
        m_formatLayout->addRow(tr("Compression (0-9):"), m_pngCompressionSpin);

    } else if (isJpeg) {
        m_formatGroup->setTitle(tr("JPEG Options"));
        m_formatGroup->setVisible(true);

        m_jpegQualitySpin = new QSpinBox(m_formatGroup);
        m_jpegQualitySpin->setRange(1, 100);
        m_jpegQualitySpin->setValue(95);
        connect(m_jpegQualitySpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &ExportImageDialog::onQualityChanged);
        m_formatLayout->addRow(tr("Quality:"), m_jpegQualitySpin);

        m_jpegQualitySlider = new QSlider(Qt::Horizontal, m_formatGroup);
        m_jpegQualitySlider->setRange(1, 100);
        m_jpegQualitySlider->setValue(95);
        connect(m_jpegQualitySlider, &QSlider::valueChanged,
                m_jpegQualitySpin, &QSpinBox::setValue);
        connect(m_jpegQualitySpin, QOverload<int>::of(&QSpinBox::valueChanged),
                m_jpegQualitySlider, &QSlider::setValue);
        m_formatLayout->addRow(QString(), m_jpegQualitySlider);

        m_jpegQualityLabel = new QLabel("95%", m_formatGroup);
        m_formatLayout->addRow(QString(), m_jpegQualityLabel);

    } else {
        // 其他格式（BMP 等）：隐藏格式专属区域
        m_formatGroup->setVisible(false);
    }
}

ExportParameters ExportImageDialog::getParameters() const
{
    ExportParameters params;
    params.dpi = m_dpiSpin->value();
    params.colorSpace = static_cast<ExportParameters::ColorSpace>(
        m_colorCombo->currentData().toInt());
    params.transparency = static_cast<ExportParameters::TransparencyHandling>(
        m_transparencyCombo->currentData().toInt());

    // TIFF 选项
    if (m_tiffCompressionCombo) {
        params.tiff.compression = static_cast<ExportParameters::CompressionType>(
            m_tiffCompressionCombo->currentData().toInt());
        params.tiff.byteOrder = static_cast<ExportParameters::ByteOrder>(
            m_tiffByteOrderCombo->currentData().toInt());
        params.tiff.bitDepth = static_cast<ExportParameters::BitDepth>(
            m_tiffBitDepthCombo->currentData().toInt());
        params.tiff.planarConfig = static_cast<ExportParameters::PlanarConfig>(
            m_tiffPlanarConfigCombo->currentData().toInt());
        params.tiff.predictor = static_cast<ExportParameters::Predictor>(
            m_tiffPredictorCombo->currentData().toInt());
        params.tiff.jpegQuality = m_tiffJpegQualitySpin->value();
        params.tiff.embedICCProfile = m_tiffICCCheck->isChecked();
        params.tiff.preserveMetadata = m_tiffMetadataCheck->isChecked();
    }

    // PNG 选项
    if (m_pngCompressionSpin)
        params.png.compressionLevel = m_pngCompressionSpin->value();

    // JPEG 选项
    if (m_jpegQualitySpin)
        params.jpeg.quality = m_jpegQualitySpin->value();

    return params;
}

void ExportImageDialog::setParameters(const ExportParameters &params)
{
    m_dpiSpin->setValue(params.dpi);

    int colorIndex = m_colorCombo->findData(static_cast<int>(params.colorSpace));
    m_colorCombo->setCurrentIndex(qMax(0, colorIndex));

    int transIndex = m_transparencyCombo->findData(static_cast<int>(params.transparency));
    m_transparencyCombo->setCurrentIndex(qMax(0, transIndex));

    // TIFF 选项
    if (m_tiffCompressionCombo) {
        int compIndex = m_tiffCompressionCombo->findData(
            static_cast<int>(params.tiff.compression));
        m_tiffCompressionCombo->setCurrentIndex(qMax(0, compIndex));

        int boIndex = m_tiffByteOrderCombo->findData(
            static_cast<int>(params.tiff.byteOrder));
        m_tiffByteOrderCombo->setCurrentIndex(qMax(0, boIndex));

        int bdIndex = m_tiffBitDepthCombo->findData(
            static_cast<int>(params.tiff.bitDepth));
        m_tiffBitDepthCombo->setCurrentIndex(qMax(0, bdIndex));

        int pcIndex = m_tiffPlanarConfigCombo->findData(
            static_cast<int>(params.tiff.planarConfig));
        m_tiffPlanarConfigCombo->setCurrentIndex(qMax(0, pcIndex));

        int predIndex = m_tiffPredictorCombo->findData(
            static_cast<int>(params.tiff.predictor));
        m_tiffPredictorCombo->setCurrentIndex(qMax(0, predIndex));

        m_tiffJpegQualitySpin->setValue(params.tiff.jpegQuality);
        m_tiffICCCheck->setChecked(params.tiff.embedICCProfile);
        m_tiffMetadataCheck->setChecked(params.tiff.preserveMetadata);
    }

    // PNG 选项
    if (m_pngCompressionSpin)
        m_pngCompressionSpin->setValue(params.png.compressionLevel);

    // JPEG 选项
    if (m_jpegQualitySpin)
        m_jpegQualitySpin->setValue(params.jpeg.quality);
}

void ExportImageDialog::onTiffCompressionChanged(int index)
{
    if (!m_tiffCompressionCombo)
        return;

    ExportParameters::CompressionType type =
        static_cast<ExportParameters::CompressionType>(
            m_tiffCompressionCombo->itemData(index).toInt());

    bool isJpeg = (type == ExportParameters::CompressionType::JPEG);
    bool isLzwOrZip = (type == ExportParameters::CompressionType::LZW ||
                       type == ExportParameters::CompressionType::ZIP);

    // JPEG 质量仅在 JPEG 压缩时可用
    m_tiffJpegQualitySpin->setEnabled(isJpeg);
    m_tiffJpegQualitySlider->setEnabled(isJpeg);
    m_tiffJpegQualityLabel->setEnabled(isJpeg);

    // 预测器仅在 LZW/ZIP 时可用
    m_tiffPredictorCombo->setEnabled(isLzwOrZip);
}

void ExportImageDialog::onQualityChanged(int value)
{
    // 更新最近活跃的质量标签
    if (m_tiffJpegQualityLabel && m_tiffJpegQualitySpin &&
        m_tiffJpegQualitySpin->hasFocus())
        m_tiffJpegQualityLabel->setText(QString("%1%").arg(value));
    if (m_jpegQualityLabel && m_jpegQualitySpin &&
        m_jpegQualitySpin->hasFocus())
        m_jpegQualityLabel->setText(QString("%1%").arg(value));
}

} // namespace ImageUtils
