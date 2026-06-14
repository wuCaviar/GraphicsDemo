#include "QAtDataTablePage.h"

#include <QTableView>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QVBoxLayout>

QAtDataTablePage::QAtDataTablePage(const QString &id, QWidget *parent)
    : QAtPage(parent), m_pageId(id)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_model = new QStandardItemModel(0, 4, this);
    m_model->setHorizontalHeaderLabels({tr("Name"), tr("Type"), tr("Position"), tr("Size")});

    m_tableView = new QTableView(this);
    m_tableView->setModel(m_model);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->setAlternatingRowColors(true);

    layout->addWidget(m_tableView);
}

void QAtDataTablePage::onActivated()
{
    // Refresh data from active canvas (P8 will connect to AppContext signals)
}
