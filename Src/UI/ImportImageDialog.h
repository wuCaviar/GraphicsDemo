#ifndef IMPORTIMAGEDIALOG_H
#define IMPORTIMAGEDIALOG_H

#include "ImageUtils.h"
#include <QDialog>

class QComboBox;
class QSpinBox;
class QSlider;
class QCheckBox;
class QLabel;

namespace ImageUtils {

class ImportImageDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ImportImageDialog(QWidget *parent = nullptr,
                               const ImportParameters &params = ImportParameters());

    ImportParameters getParameters() const;
    void setParameters(const ImportParameters &params);

private slots:
    void onDpiPolicyChanged(int index);
    void onScaleChanged(int value);

private:
    QComboBox *m_dpiCombo;
    QSpinBox *m_dpiSpin;
    QComboBox *m_colorCombo;
    QComboBox *m_alphaCombo;
    QCheckBox *m_scaleCheck;
    QSlider *m_scaleSlider;
    QLabel *m_scaleLabel;
    QCheckBox *m_tiffMetaCheck;
};

} // namespace ImageUtils

#endif // IMPORTIMAGEDIALOG_H
