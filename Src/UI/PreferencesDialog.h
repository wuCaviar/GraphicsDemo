#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>

class PreferencesPage;
class QTabWidget;

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget *parent = nullptr);

    void addPage(PreferencesPage *page);

private:
    void setupUI();

    QTabWidget *m_tabWidget = nullptr;
    QList<PreferencesPage *> m_pages;
};

#endif // PREFERENCESDIALOG_H
