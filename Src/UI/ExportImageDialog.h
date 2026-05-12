#ifndef EXPORTIMAGEDIALOG_H
#define EXPORTIMAGEDIALOG_H

#include "ImageUtils.h"
#include <QDialog>

class QComboBox;
class QSpinBox;
class QSlider;
class QCheckBox;
class QLabel;
class QGroupBox;
class QFormLayout;

namespace ImageUtils {

class ExportImageDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportImageDialog(QWidget *parent = nullptr,
                               const ExportParameters &params = ExportParameters(),
                               const QString &format = "PNG");

    ExportParameters getParameters() const;
    void setParameters(const ExportParameters &params);

private slots:
    void onTiffCompressionChanged(int index);
    void onQualityChanged(int value);

private:
    void rebuildFormatGroup(const QString &format);

    // ===== 通用控件 =====
    QSpinBox *m_dpiSpin;
    QComboBox *m_colorCombo;
    QComboBox *m_transparencyCombo;

    // ===== 格式专属区域 =====
    QGroupBox *m_formatGroup;
    QFormLayout *m_formatLayout;
    QString m_currentFormat;

    // TIFF 控件
    QComboBox *m_tiffCompressionCombo;
    QComboBox *m_tiffByteOrderCombo;
    QComboBox *m_tiffBitDepthCombo;
    QComboBox *m_tiffPlanarConfigCombo;
    QComboBox *m_tiffPredictorCombo;
    QCheckBox *m_tiffICCCheck;
    QCheckBox *m_tiffMetadataCheck;
    QSpinBox  *m_tiffJpegQualitySpin;
    QSlider   *m_tiffJpegQualitySlider;
    QLabel    *m_tiffJpegQualityLabel;

    // PNG 控件
    QSpinBox *m_pngCompressionSpin;

    // JPEG 控件
    QSpinBox *m_jpegQualitySpin;
    QSlider  *m_jpegQualitySlider;
    QLabel   *m_jpegQualityLabel;
};

} // namespace ImageUtils

#endif // EXPORTIMAGEDIALOG_H
