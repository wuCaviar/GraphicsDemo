#ifndef CANVASPAGE_H
#define CANVASPAGE_H

#include "PreferencesPage.h"

class QDoubleSpinBox;

class CanvasPage : public PreferencesPage
{
    Q_OBJECT

public:
    explicit CanvasPage(QWidget *parent = nullptr);

    QString title() const override;
    void load() override;
    bool save() override;

private:
    void setupUI();

    QDoubleSpinBox *m_marginLeftSpin = nullptr;
    QDoubleSpinBox *m_marginRightSpin = nullptr;
    QDoubleSpinBox *m_marginTopSpin = nullptr;
    QDoubleSpinBox *m_marginBottomSpin = nullptr;
};

#endif // CANVASPAGE_H
