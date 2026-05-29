#include "FitCanvasDlg.h"
#include "ui_FitCanvasDlg.h"

FitCanvasDlg::FitCanvasDlg(QWidget *parent)
    : QDialog(parent), ui(new Ui::FitCanvasDlg)
{
    ui->setupUi(this);

    connect(ui->comboBox, &QComboBox::currentIndexChanged, this,
            &FitCanvasDlg::_slotIndexChanged);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &FitCanvasDlg::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &FitCanvasDlg::reject);

    _initWidget();
}

FitCanvasDlg::~FitCanvasDlg()
{
    delete ui;
}

void FitCanvasDlg::setParam(const QSizeF &canvas, const QSizeF &image)
{
    m_sDefaultSize = canvas;
    m_sImageSize = image;
}

QSizeF FitCanvasDlg::getResult() const
{
    int index = ui->comboBox->currentData().toInt();
    if (index == fctAdapt) {
        return m_sImageSize.isValid() ? m_sImageSize : m_sDefaultSize;
    }
    if (index == fctWidth) {
        double w = ui->doubleSpinBox->value();
        return QSizeF(w, m_sDefaultSize.height());
    }
    if (index == fctHeight) {
        double h = ui->doubleSpinBox->value();
        return QSizeF(m_sDefaultSize.width(), h);
    }

    return m_sDefaultSize;
}

FitCanvasType FitCanvasDlg::fitType() const
{
    return static_cast<FitCanvasType>(ui->comboBox->currentData().toInt());
}

double FitCanvasDlg::fitValue() const
{
    return ui->doubleSpinBox->value();
}

void FitCanvasDlg::_initWidget()
{
    // 初始化
    ui->comboBox->addItem(tr("No need"), fctNone);
    ui->comboBox->addItem(tr("Adaptation"), fctAdapt);
    ui->comboBox->addItem(tr("Custom width"), fctWidth);
    ui->comboBox->addItem(tr("Custom height"), fctHeight);

    ui->comboBox->setCurrentIndex(fctNone);

    ui->widget->setVisible(false);
    ui->label->setText("");
}

void FitCanvasDlg::_slotIndexChanged(int index)
{
    ui->widget->setVisible(index == fctWidth || index == fctHeight);

    QString text = "";
    if (index == fctWidth) {
        text = tr("Width (mm):");
        ui->doubleSpinBox->setMaximum(m_sDefaultSize.width() * 10.0);
        ui->doubleSpinBox->setValue(m_sDefaultSize.width());
        ui->doubleSpinBox->setSuffix(QStringLiteral(" mm"));
    } else if (index == fctHeight) {
        text = tr("Height (mm):");
        ui->doubleSpinBox->setMaximum(m_sDefaultSize.height() * 10.0);
        ui->doubleSpinBox->setValue(m_sDefaultSize.height());
        ui->doubleSpinBox->setSuffix(QStringLiteral(" mm"));
    }

    ui->label->setText(text);
}
