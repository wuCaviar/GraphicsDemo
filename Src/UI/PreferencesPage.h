#ifndef PREFERENCESPAGE_H
#define PREFERENCESPAGE_H

#include <QWidget>

class PreferencesPage : public QWidget
{
    Q_OBJECT

public:
    explicit PreferencesPage(QWidget *parent = nullptr) : QWidget(parent) {}

    virtual QString title() const = 0;
    virtual void load() = 0;
    virtual bool save() = 0;
};

#endif // PREFERENCESPAGE_H
