#ifndef EXPORTIMAGEDIALOG_H
#define EXPORTIMAGEDIALOG_H

#include "ImageUtils.h"
#include <QDialog>

class QComboBox;
class QSpinBox;
class QSlider;
class QCheckBox;
class QLabel;

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

    QString getSelectedFormat() const;

private slots:
    void onCompressionChanged(int index);
    void onQualityChanged(int value);
    void onFormatChanged(const QString &format);

private:
    QComboBox *m_compressionCombo;
    QSpinBox *m_jpegQualitySpin;
    QSlider *m_jpegQualitySlider;
    QSpinBox *m_pngCompressionSpin;
    QCheckBox *m_metadataCheck;
    QCheckBox *m_iccCheck;
    QComboBox *m_transparencyCombo;
    QSpinBox *m_dpiSpin;
    QComboBox *m_colorCombo;
    QLabel *m_qualityLabel;
};

} // namespace ImageUtils

#endif // EXPORTIMAGEDIALOG_H
