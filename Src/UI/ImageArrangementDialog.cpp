#include "ImageArrangementDialog.h"
#include "ui_ImageArrangementDialog.h"

#include <QDialogButtonBox>
#include <QListWidget>
#include <QVBoxLayout>

ImageArrangementDialog::ImageArrangementDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::ImageArrangementDialog)
{
    ui->setupUi(this);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // 在 scrollContent 中创建文件列表
    m_listWidget = new QListWidget(ui->scrollContent);
    m_listWidget->setDragDropMode(QAbstractItemView::InternalMove);
    m_listWidget->setDefaultDropAction(Qt::MoveAction);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);

    QVBoxLayout *scrollLayout = qobject_cast<QVBoxLayout *>(ui->scrollContent->layout());
    scrollLayout->addWidget(m_listWidget);
}

ImageArrangementDialog::~ImageArrangementDialog()
{
    delete ui;
}

void ImageArrangementDialog::setFilePaths(const QStringList &paths)
{
    m_listWidget->clear();
    for (const auto &path : paths)
        m_listWidget->addItem(path);
}

ImageArrangement ImageArrangementDialog::arrangement() const
{
    return ui->horizontalRadio->isChecked() ? ArrangeHorizontal : ArrangeVertical;
}

QStringList ImageArrangementDialog::orderedPaths() const
{
    QStringList paths;
    paths.reserve(m_listWidget->count());
    for (int i = 0; i < m_listWidget->count(); ++i)
        paths.append(m_listWidget->item(i)->text());
    return paths;
}
