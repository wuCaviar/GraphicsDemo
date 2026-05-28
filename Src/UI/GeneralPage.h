#ifndef GENERALPAGE_H
#define GENERALPAGE_H

#include "PreferencesPage.h"

class QLineEdit;

class GeneralPage : public PreferencesPage
{
    Q_OBJECT

public:
    explicit GeneralPage(QWidget *parent = nullptr);

    QString title() const override;
    void load() override;
    bool save() override;

private:
    void setupUI();

    QLineEdit *m_ripExePathEdit = nullptr;
    QLineEdit *m_ripConfigPathEdit = nullptr;
    QLineEdit *m_iccProfilePathEdit = nullptr;
};

#endif // GENERALPAGE_H
