#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "colormanager.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QVBoxLayout>
#include <QThread>
#include <cmath>

#include <tiffio.h>

// ================================================================
//  TiffExportWorker
// ================================================================

TiffExportWorker::TiffExportWorker(QObject *parent) : QObject(parent) {}

void TiffExportWorker::exportTiff(const QImage &img)
{
    constexpr int SIZE = 24;
    const QString path = QStringLiteral("/Volumes/Caviar/Test/color_swatch.tiff");

    TIFF *tif = TIFFOpen(path.toLocal8Bit().constData(), "w");
    if (!tif) {
        qCritical() << "无法创建TIFF文件:" << path;
        return;
    }

    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, uint32_t(SIZE));
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, uint32_t(SIZE));
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, uint32_t(SIZE));

    for (int y = 0; y < SIZE; ++y) {
        TIFFWriteScanline(tif, const_cast<uchar*>(img.constScanLine(y)), y);
    }

    TIFFClose(tif);
    qInfo() << "已导出:" << path;

    emit finished();
}

// ================================================================
//  MainWindow
// ================================================================

static QMap<int, cmsUInt32Number> mapId2Flag;

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    _initWidget();

    // 工作线程
    m_workerThread = new QThread(this);
    auto *worker = new TiffExportWorker;
    worker->moveToThread(m_workerThread);

    connect(this, &MainWindow::tiffExportRequested,
            worker, &TiffExportWorker::exportTiff);
    connect(worker, &TiffExportWorker::finished,
            this, &MainWindow::onTiffExportFinished, Qt::QueuedConnection);
    connect(m_workerThread, &QThread::finished,
            worker, &QObject::deleteLater);

    m_workerThread->start();

    ColorManager &cm = ColorManager::instance();
    if (!cm.initialize()) {
        qCritical().noquote() << cm.errorString();
        return;
    }

    connect(ui->inientComboBox, &QComboBox::currentTextChanged, this, &MainWindow::_updateColor);
    connect(group, &QButtonGroup::buttonClicked, this, &MainWindow::_updateColor);

    connect(ui->rSpB, &QSpinBox::valueChanged, this, &MainWindow::_editRGB);
    connect(ui->gSpB, &QSpinBox::valueChanged, this, &MainWindow::_editRGB);
    connect(ui->bSpB, &QSpinBox::valueChanged, this, &MainWindow::_editRGB);

    connect(ui->cSpB, &QSpinBox::valueChanged, this, &MainWindow::_editCMYK);
    connect(ui->mSpB, &QSpinBox::valueChanged, this, &MainWindow::_editCMYK);
    connect(ui->ySpB, &QSpinBox::valueChanged, this, &MainWindow::_editCMYK);
    connect(ui->kSpB, &QSpinBox::valueChanged, this, &MainWindow::_editCMYK);

    color = QColor(Qt::white);
    _updateColorLabel();
    _requestTiffExport();
}

MainWindow::~MainWindow()
{
    m_workerThread->quit();
    m_workerThread->wait();
    delete ui;
}

void MainWindow::_updateCMYKColor()
{
    ColorManager &manager = ColorManager::instance();

    cmsUInt32Number inient = ui->inientComboBox->currentData().toInt();
    cmsUInt32Number flags = 0;
    foreach (auto btn, group->buttons()) {
        QCheckBox* check = static_cast<QCheckBox*>(btn);
        if(!check) continue;

        if(check->isChecked())
        {
            cmsUInt32Number flag = mapId2Flag.value(group->id(btn));
            flags |= flag;
        }
    }

    if(manager.buildRGB2CMYKTransforms(inient, flags))
    {
        ColorManager::Cmyk cmyk = manager.toCmyk(color);

        const QSignalBlocker cBlock(ui->cSpB);
        const QSignalBlocker mBlock(ui->mSpB);
        const QSignalBlocker yBlock(ui->ySpB);
        const QSignalBlocker kBlock(ui->kSpB);

        ui->cSpB->setValue(int(std::round(cmyk.c)));
        ui->mSpB->setValue(int(std::round(cmyk.m)));
        ui->ySpB->setValue(int(std::round(cmyk.y)));
        ui->kSpB->setValue(int(std::round(cmyk.k)));
    }

    _updateColorLabel();
    _requestTiffExport();
}

void MainWindow::_editRGB()
{
    color = QColor(ui->rSpB->value(),
                   ui->gSpB->value(),
                   ui->bSpB->value());
    flag = 0;
    _updateCMYKColor();
}

void MainWindow::_updateRGBColor()
{
    ColorManager::Cmyk cmyk {
        double(ui->cSpB->value()),
        double(ui->mSpB->value()),
        double(ui->ySpB->value()),
        double(ui->kSpB->value())
    };

    ColorManager &manager = ColorManager::instance();

    cmsUInt32Number inient = ui->inientComboBox->currentData().toInt();
    cmsUInt32Number flags = 0;
    foreach (auto btn, group->buttons()) {
        QCheckBox* check = static_cast<QCheckBox*>(btn);
        if(!check) continue;

        if(check->isChecked())
        {
            cmsUInt32Number flag = mapId2Flag.value(group->id(btn));
            flags |= flag;
        }
    }

    if(manager.buildCMYK2RGBTransforms(inient, flags))
    {
        QColor rgb = manager.toRgb(cmyk);

        const QSignalBlocker rBlock(ui->rSpB);
        const QSignalBlocker gBlock(ui->gSpB);
        const QSignalBlocker bBlock(ui->bSpB);

        ui->rSpB->setValue(rgb.red());
        ui->gSpB->setValue(rgb.green());
        ui->bSpB->setValue(rgb.blue());

        color = rgb;
    }

    _updateColorLabel();
    _requestTiffExport();
}

void MainWindow::_editCMYK()
{
    flag = 1;
    _updateRGBColor();
}

void MainWindow::_updateColorLabel()
{
    ui->labelColor->setStyleSheet(
        QStringLiteral("background-color: %1").arg(color.name()));
}

void MainWindow::_requestTiffExport()
{
    if (m_exportPending.exchange(true))
        return;

    QImage img(24, 24, QImage::Format_RGB888);
    img.fill(color);

    emit tiffExportRequested(img);
}

void MainWindow::onTiffExportFinished()
{
    m_exportPending.store(false);
}

void MainWindow::_initWidget()
{
    ui->inientComboBox->addItem("感知", 0);
    ui->inientComboBox->addItem("相对色度", 1);
    ui->inientComboBox->addItem("饱和度", 2);
    ui->inientComboBox->addItem("绝对色度", 3);

    QVBoxLayout *layout = new QVBoxLayout(ui->flagsWidget);
    group = new QButtonGroup(ui->flagsWidget);
    group->setExclusive(false);

    auto func = [&](cmsUInt32Number flag) {
        auto check = new QCheckBox(QString::number(flag), ui->flagsWidget);
        group->addButton(check);
        mapId2Flag.insert(group->id(check), flag);
        layout->addWidget(check);
    };

    func(cmsFLAGS_NOCACHE);
    func(cmsFLAGS_NOOPTIMIZE);
    func(cmsFLAGS_NULLTRANSFORM);

    func(cmsFLAGS_GAMUTCHECK);
    func(cmsFLAGS_SOFTPROOFING);

    func(cmsFLAGS_BLACKPOINTCOMPENSATION);
    func(cmsFLAGS_NOWHITEONWHITEFIXUP);
    func(cmsFLAGS_HIGHRESPRECALC);
    func(cmsFLAGS_LOWRESPRECALC);

    func(cmsFLAGS_8BITS_DEVICELINK);
    func(cmsFLAGS_GUESSDEVICECLASS);
    func(cmsFLAGS_KEEP_SEQUENCE);

    func(cmsFLAGS_FORCE_CLUT);
    func(cmsFLAGS_CLUT_POST_LINEARIZATION);
    func(cmsFLAGS_CLUT_PRE_LINEARIZATION);

    func(cmsFLAGS_NONEGATIVES);

    func(cmsFLAGS_COPY_ALPHA);

    func(cmsFLAGS_NODEFAULTRESOURCEDEF);

    ui->flagsWidget->setLayout(layout);

    // ui->rSpB->setEnabled(false);
    // ui->gSpB->setEnabled(false);
    // ui->bSpB->setEnabled(false);
    // ui->cSpB->setEnabled(false);
    // ui->mSpB->setEnabled(false);
    // ui->ySpB->setEnabled(false);
    // ui->kSpB->setEnabled(false);
}

void MainWindow::_updateColor()
{
    if(flag == 0)
        _updateCMYKColor();
    else
        _updateRGBColor();
}
