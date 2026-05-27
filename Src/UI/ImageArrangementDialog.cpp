#include "ImageArrangementDialog.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

ImageArrangementDialog::ImageArrangementDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUI();
}

void ImageArrangementDialog::setupUI()
{
    setWindowTitle(tr("Image Arrangement"));
    setMinimumWidth(520);
    setMinimumHeight(320);

    auto *mainLayout = new QVBoxLayout(this);

    // ---- top: left (arrangement) + right (file list) ----
    auto *topLayout = new QHBoxLayout;

    // Left: arrangement group
    m_arrangementGroup = new QGroupBox(tr("Arrangement"));
    auto *groupLayout = new QVBoxLayout(m_arrangementGroup);

    m_horizontalRadio = new QRadioButton(tr("Horizontal"));
    m_verticalRadio = new QRadioButton(tr("Vertical"));
    m_verticalRadio->setChecked(true);

    groupLayout->addWidget(m_horizontalRadio);
    groupLayout->addWidget(m_verticalRadio);
    groupLayout->addStretch();

    topLayout->addWidget(m_arrangementGroup);

    // Right: file list with drag reorder
    auto *rightLayout = new QVBoxLayout;
    rightLayout->addWidget(new QLabel(tr("File order (drag to reorder):")));

    m_fileList = new QListWidget;
    m_fileList->setDragDropMode(QAbstractItemView::InternalMove);
    m_fileList->setDefaultDropAction(Qt::MoveAction);
    m_fileList->setSelectionMode(QAbstractItemView::SingleSelection);
    rightLayout->addWidget(m_fileList);

    topLayout->addLayout(rightLayout, 1);

    mainLayout->addLayout(topLayout);

    // ---- bottom: OK / Cancel ----
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

void ImageArrangementDialog::setFilePaths(const QStringList &paths)
{
    m_fileList->clear();
    for (const QString &path : paths) {
        m_fileList->addItem(path);
    }
}

ImageArrangement ImageArrangementDialog::arrangement() const
{
    return m_horizontalRadio->isChecked() ? ArrangeHorizontal : ArrangeVertical;
}

QStringList ImageArrangementDialog::orderedPaths() const
{
    QStringList paths;
    for (int i = 0; i < m_fileList->count(); ++i) {
        paths.append(m_fileList->item(i)->text());
    }
    return paths;
}
