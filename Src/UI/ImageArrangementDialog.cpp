#include "ImageArrangementDialog.h"
#include "ui_ImageArrangementDialog.h"
#include "ui_FileInfoWidget.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <QDebug>

#include <tiffio.h>

static FileDpiInfo getTiffDpi(const QString &filePath)
{
    FileDpiInfo info;
    info.paths = { filePath };

    QByteArray pathBytes = filePath.toLocal8Bit();
    TIFF *tif = TIFFOpen(pathBytes.constData(), "r");
    if (!tif) {
        info.type = fitFailed;
        return info;
    }

    float xres = 0.0f, yres = 0.0f;
    if (!TIFFGetField(tif, TIFFTAG_XRESOLUTION, &xres)
        || !TIFFGetField(tif, TIFFTAG_YRESOLUTION, &yres)) {
        TIFFClose(tif);
        info.type = fitExclude;
        return info;
    }

    uint16_t unit = 0;
    TIFFGetField(tif, TIFFTAG_RESOLUTIONUNIT, &unit);
    if (unit == 0)
        unit = RESUNIT_INCH;

    TIFFClose(tif);

    switch (unit) {
    case RESUNIT_INCH:
        info.type = fitSuccess;
        info.dpi.first = xres;
        info.dpi.second = yres;
        break;
    case RESUNIT_CENTIMETER:
        info.type = fitSuccess;
        info.dpi.first = xres * 2.54f;
        info.dpi.second = yres * 2.54f;
        break;
    default:
        info.type = fitNone;
        break;
    }

    return info;
}

// ─── ImageArrangementDialog ────────────────────────────────────────

ImageArrangementDialog::ImageArrangementDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ImageArrangementDialog)
    , m_radioGroup(new QButtonGroup(this))
{
    ui->setupUi(this);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_dpiWatcher = new QFutureWatcher<FileDpiInfo>(this);
    connect(m_dpiWatcher, &QFutureWatcher<FileDpiInfo>::finished, this,
            &ImageArrangementDialog::onDpiFinished);
}

ImageArrangementDialog::~ImageArrangementDialog()
{
    delete ui;
}

void ImageArrangementDialog::setFilePaths(const QStringList &paths)
{
    if (paths.isEmpty())
        return;

    // 清除旧的 FileInfoWidget
    qDeleteAll(m_lstFileInfoWidget);
    m_lstFileInfoWidget.clear();
    m_dpiGroups.clear();

    // 清除旧的 radio button 分组
    for (auto *btn : m_radioGroup->buttons())
        m_radioGroup->removeButton(btn);

    // 禁用 OK 按钮，等待 DPI 扫描完成
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    m_loadingLabel = new QLabel(tr("Scanning DPI..."), ui->scrollContent);
    m_loadingLabel->setAlignment(Qt::AlignCenter);
    ui->scrollContent->layout()->addWidget(m_loadingLabel);

    m_dpiWatcher->setFuture(QtConcurrent::mapped(paths, getTiffDpi));
}

void ImageArrangementDialog::setCanvasDpi(int dpi)
{
    m_canvasDpi = dpi;
}

int ImageArrangementDialog::selectedGroupDpi() const
{
    for (int i = 0; i < m_lstFileInfoWidget.size(); ++i) {
        if (m_lstFileInfoWidget[i]->radioButton()->isChecked()) {
            if (i < m_dpiGroups.size() && m_dpiGroups[i].type == fitSuccess)
                return m_dpiGroups[i].dpi.first;
            return 0;
        }
    }
    return 0;
}

void ImageArrangementDialog::onDpiFinished()
{
    // 移除加载提示，启用 OK 按钮
    if (m_loadingLabel) {
        m_loadingLabel->deleteLater();
        m_loadingLabel = nullptr;
    }
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);

    const auto results = m_dpiWatcher->future().results();

    // 按类型和 DPI 合并
    m_dpiGroups.clear();
    for (const auto &info : results) {
        if (info.empty())
            continue;

        bool merged = false;
        for (auto &existing : m_dpiGroups) {
            int oldSize = existing.paths.size();
            existing += info;
            if (existing.paths.size() > oldSize) {
                merged = true;
                break;
            }
        }
        if (!merged)
            m_dpiGroups.append(info);
    }

    // 创建 FileInfoWidget 并加入滚动区域
    QVBoxLayout *scrollLayout =
        qobject_cast<QVBoxLayout *>(ui->scrollContent->layout());
    int firstValidIndex = -1;
    for (int i = 0; i < m_dpiGroups.size(); ++i) {
        const auto &info = m_dpiGroups[i];
        if (info.empty())
            continue;

        auto *widget = new FileInfoWidget(ui->scrollContent);
        widget->setFileInfo(info);
        scrollLayout->addWidget(widget);
        m_lstFileInfoWidget.append(widget);
        m_radioGroup->addButton(widget->radioButton());

        // 如果画布已有 DPI，禁用不匹配的组
        if (m_canvasDpi > 0 && info.type == fitSuccess &&
            info.dpi.first != m_canvasDpi) {
            widget->radioButton()->setEnabled(false);
            widget->radioButton()->setToolTip(
                tr("Canvas DPI is %1, this group is %2 DPI")
                    .arg(m_canvasDpi).arg(info.dpi.first));
        } else if (firstValidIndex < 0) {
            firstValidIndex = m_lstFileInfoWidget.size() - 1;
        }
    }

    // 默认选中第一个匹配的组
    if (firstValidIndex >= 0)
        m_lstFileInfoWidget[firstValidIndex]->radioButton()->setChecked(true);
    else if (!m_lstFileInfoWidget.isEmpty())
        m_lstFileInfoWidget.first()->radioButton()->setChecked(true);
}

ImageArrangement ImageArrangementDialog::arrangement() const
{
    return ui->horizontalRadio->isChecked() ? ArrangeHorizontal
                                            : ArrangeVertical;
}

QStringList ImageArrangementDialog::orderedPaths() const
{
    for (auto *widget : m_lstFileInfoWidget) {
        if (widget->radioButton()->isChecked())
            return widget->orderedPaths();
    }
    return {};
}

// ─── FileInfoWidget ────────────────────────────────────────────────

FileInfoWidget::FileInfoWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::FileInfoWidget)
{
    ui->setupUi(this);

    ui->listWidget->setDragDropMode(QAbstractItemView::InternalMove);
    ui->listWidget->setDefaultDropAction(Qt::MoveAction);
    ui->listWidget->setSelectionMode(QAbstractItemView::SingleSelection);

    ui->radioButton->setText(tr("Select and import"));
}

FileInfoWidget::~FileInfoWidget()
{
    delete ui;
}

void FileInfoWidget::setFileInfo(const FileDpiInfo &infos)
{
    if (infos.empty())
        return;

    QString title;
    switch (infos.type) {
    case fitSuccess:
        title = QString("X:%1 Y:%2").arg(infos.dpi.first).arg(infos.dpi.second);
        break;
    case fitFailed:
        title = tr("Cannot be opened.");
        break;
    case fitExclude:
        title = tr("Not included");
        break;
    case fitNone:
        title = tr("None");
        break;
    }

    ui->groupBox->setTitle(title);

    ui->listWidget->clear();
    for (const auto &str : infos.paths)
        ui->listWidget->addItem(str);
}

QStringList FileInfoWidget::orderedPaths() const
{
    QStringList paths;
    paths.reserve(ui->listWidget->count());
    for (int i = 0; i < ui->listWidget->count(); ++i)
        paths.append(ui->listWidget->item(i)->text());
    return paths;
}

QRadioButton *FileInfoWidget::radioButton() const
{
    return ui->radioButton;
}
