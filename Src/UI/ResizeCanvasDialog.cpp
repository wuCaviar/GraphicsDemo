#include "ResizeCanvasDialog.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
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
    m_widthSpin->setSuffix(QStringLiteral(" mm"));
    formLayout->addRow(tr("Width:"), m_widthSpin);

    m_heightSpin = new QDoubleSpinBox;
    m_heightSpin->setRange(1, 999999);
    m_heightSpin->setDecimals(1);
    m_heightSpin->setSuffix(QStringLiteral(" mm"));
    formLayout->addRow(tr("Height:"), m_heightSpin);

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

void ResizeCanvasDialog::setCurrentSizeMM(const QSizeF &pixelSize, qreal ppi)
{
    qreal factor = 25.4 / ppi;
    m_widthSpin->setValue(pixelSize.width() * factor);
    m_heightSpin->setValue(pixelSize.height() * factor);
    m_infoLabel->setText(tr("PPI: %1")
                             .arg(ppi, 0, 'f', 0));
}

QSizeF ResizeCanvasDialog::newPixelSize(qreal ppi) const
{
    qreal factor = ppi / 25.4;
    return QSizeF(m_widthSpin->value() * factor, m_heightSpin->value() * factor);
}
