#include "PreferencesDialog.h"
#include "PreferencesPage.h"
#include "GeneralPage.h"
#include "CanvasPage.h"

#include <QDialogButtonBox>
#include <QTabWidget>
#include <QVBoxLayout>

PreferencesDialog::PreferencesDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("首选项"));
    setMinimumSize(600, 400);
    setupUI();

    addPage(new GeneralPage(this));
    addPage(new CanvasPage(this));

    for (auto *page : m_pages)
        page->load();
}

void PreferencesDialog::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabPosition(QTabWidget::North);
    mainLayout->addWidget(m_tabWidget);

    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        for (auto *page : m_pages) {
            if (!page->save())
                return;
        }
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

void PreferencesDialog::addPage(PreferencesPage *page)
{
    m_pages.append(page);
    m_tabWidget->addTab(page, page->title());
}
