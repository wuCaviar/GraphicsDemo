#ifndef QATDATATABLEPAGE_H
#define QATDATATABLEPAGE_H

#include "QAtPage.h"

class QTableView;
class QStandardItemModel;

// Data table page — displays item properties in a table (example Page type)
class QAtDataTablePage : public QAtPage
{
    Q_OBJECT
public:
    explicit QAtDataTablePage(const QString &id, QWidget *parent = nullptr);

    QString pageId() const override   { return m_pageId; }
    QString pageType() const override { return PageType::DataTable; }
    QString title() const override    { return tr("Item Data"); }

    void onActivated() override;

private:
    QString             m_pageId;
    QTableView         *m_tableView = nullptr;
    QStandardItemModel *m_model = nullptr;
};

#endif // QATDATATABLEPAGE_H
