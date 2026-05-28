#include "ResizeCanvasDialog.h"
#include "ATHCPresets.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

ResizeCanvasDialog::ResizeCanvasDialog(QWidget *parent) : QDialog(parent)
{
    setupUI();
}

void ResizeCanvasDialog::setupUI()
{
    setWindowTitle(tr("Resize Canvas"));
    setMinimumWidth(320);

    auto *mainLayout = new QVBoxLayout(this);

    auto *formLayout = new QFormLayout;

    m_widthSpin = new QDoubleSpinBox;
    m_widthSpin->setRange(1, 999999);
    m_widthSpin->setDecimals(1);
    formLayout->addRow(tr("Width:"), m_widthSpin);

    m_heightSpin = new QDoubleSpinBox;
    m_heightSpin->setRange(1, 999999);
    m_heightSpin->setDecimals(1);
    formLayout->addRow(tr("Height:"), m_heightSpin);

    m_dpiCombo = new QComboBox;
    for (int dpi : kDpiValues)
        m_dpiCombo->addItem(QString::number(dpi) + tr(" dpi"), dpi);
    m_dpiCombo->setCurrentIndex(2); // 默认 300 dpi
    formLayout->addRow(tr("DPI:"), m_dpiCombo);

    mainLayout->addLayout(formLayout);

    m_infoLabel = new QLabel;
    m_infoLabel->setStyleSheet("color: gray;");
    mainLayout->addWidget(m_infoLabel);

    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ResizeCanvasDialog::setCurrentSize(const QSizeF &pixelSize, qreal ppi, bool isMmMode)
{
    m_ppi = ppi;
    m_isMmMode = isMmMode;

    // 选中当前 DPI
    int dpiIdx = m_dpiCombo->findData(qRound(ppi));
    if (dpiIdx >= 0)
        m_dpiCombo->setCurrentIndex(dpiIdx);

    if (isMmMode) {
        qreal factor = 25.4 / ppi;
        m_widthSpin->setValue(pixelSize.width() * factor);
        m_heightSpin->setValue(pixelSize.height() * factor);
        m_widthSpin->setSuffix(QStringLiteral(" mm"));
        m_heightSpin->setSuffix(QStringLiteral(" mm"));
        m_infoLabel->setText(tr("Resolution: %1 PPI  |  1 mm = %2 px")
                                 .arg(ppi, 0, 'f', 0)
                                 .arg(ppi / 25.4, 0, 'f', 2));
    } else {
        m_widthSpin->setValue(pixelSize.width());
        m_heightSpin->setValue(pixelSize.height());
        m_widthSpin->setSuffix(QStringLiteral(" px"));
        m_heightSpin->setSuffix(QStringLiteral(" px"));
        m_infoLabel->setText(
            tr("Resolution: %1 PPI  |  %2 × %3 px = %4 × %5 mm")
                .arg(ppi, 0, 'f', 0)
                .arg(pixelSize.width(), 0, 'f', 1)
                .arg(pixelSize.height(), 0, 'f', 1)
                .arg(pixelSize.width() * 25.4 / ppi, 0, 'f', 1)
                .arg(pixelSize.height() * 25.4 / ppi, 0, 'f', 1));
    }
}

QSizeF ResizeCanvasDialog::newPixelSize() const
{
    qreal ppi = selectedDpi();
    if (m_isMmMode) {
        qreal factor = ppi / 25.4;
        return QSizeF(m_widthSpin->value() * factor, m_heightSpin->value() * factor);
    }
    return QSizeF(m_widthSpin->value(), m_heightSpin->value());
}

int ResizeCanvasDialog::selectedDpi() const
{
    return m_dpiCombo->currentData().toInt();
}
