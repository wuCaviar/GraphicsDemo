#include "ResizeCanvasDialog.h"
#include "atMath.h"

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

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ResizeCanvasDialog::setCurrentSizeMM(const QSizeF &pixelSize, qreal ppi)
{
    AtMath::Units::DPIContext ctx(ppi);
    m_widthSpin->setValue(ctx.pxToMm(pixelSize.width()));
    m_heightSpin->setValue(ctx.pxToMm(pixelSize.height()));
    m_infoLabel->setText(tr("PPI: %1").arg(ppi, 0, 'f', 0));
}

QSizeF ResizeCanvasDialog::newPixelSize(qreal ppi) const
{
    AtMath::Units::DPIContext ctx(ppi);
    return QSizeF(ctx.mmToPx(m_widthSpin->value()), ctx.mmToPx(m_heightSpin->value()));
}
