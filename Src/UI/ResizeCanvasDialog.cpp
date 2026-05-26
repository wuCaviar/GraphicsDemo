#include "ResizeCanvasDialog.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
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

    mainLayout->addLayout(formLayout);

    m_infoLabel = new QLabel;
    m_infoLabel->setStyleSheet("color: gray;");
    mainLayout->addWidget(m_infoLabel);

    auto *btnLayout = new QHBoxLayout;
    auto *okBtn = new QPushButton(tr("OK"));
    auto *cancelBtn = new QPushButton(tr("Cancel"));
    btnLayout->addStretch();
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    mainLayout->addLayout(btnLayout);

    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void ResizeCanvasDialog::setCurrentSize(const QSizeF &pixelSize, qreal ppi, bool isMmMode)
{
    m_ppi = ppi;
    m_isMmMode = isMmMode;

    if (isMmMode) {
        qreal factor = 25.4 / ppi;
        m_widthSpin->setValue(pixelSize.width() * factor);
        m_heightSpin->setValue(pixelSize.height() * factor);
        m_widthSpin->setSuffix(tr(" mm"));
        m_heightSpin->setSuffix(tr(" mm"));
        m_infoLabel->setText(tr("Resolution: %1 PPI  |  1 mm = %2 px")
                                 .arg(ppi, 0, 'f', 0)
                                 .arg(ppi / 25.4, 0, 'f', 2));
    } else {
        m_widthSpin->setValue(pixelSize.width());
        m_heightSpin->setValue(pixelSize.height());
        m_widthSpin->setSuffix(tr(" px"));
        m_heightSpin->setSuffix(tr(" px"));
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
    if (m_isMmMode) {
        qreal factor = m_ppi / 25.4;
        return QSizeF(m_widthSpin->value() * factor, m_heightSpin->value() * factor);
    }
    return QSizeF(m_widthSpin->value(), m_heightSpin->value());
}
