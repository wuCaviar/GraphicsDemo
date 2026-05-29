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

void ResizeCanvasDialog::setCurrentSize(const QSizeF &sizeMm, qreal currentDpi)
{
    m_widthSpin->setValue(sizeMm.width());
    m_heightSpin->setValue(sizeMm.height());

    if (currentDpi > 0) {
        m_infoLabel->setText(tr("Current DPI: %1").arg(qRound(currentDpi)));
    } else {
        m_infoLabel->setText(tr("No DPI set (will be determined by imported images)"));
    }
}

QSizeF ResizeCanvasDialog::newSizeMm() const
{
    return QSizeF(m_widthSpin->value(), m_heightSpin->value());
}
