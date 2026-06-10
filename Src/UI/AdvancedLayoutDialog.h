#ifndef ADVANCEDLAYOUTDIALOG_H
#define ADVANCEDLAYOUTDIALOG_H

#include <QDialog>

class QLineEdit;
class QComboBox;
class QCheckBox;

class AdvancedLayoutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AdvancedLayoutDialog(QWidget *parent = nullptr);

private slots:
    void onBoolSelest4Toggled(bool checked);

private:
    void loadSettings();
    void saveSettings();
    void addColorItems(QComboBox *comboBox);

    // Resolution
    QLineEdit *m_lessResRatioEdit;
    QLineEdit *m_maxResRatioEdit;
    QLineEdit *m_otherResRatioEdit;
    QComboBox *m_otherResModeCombo;

    // Toggles
    QCheckBox *m_boolSelest1;
    QCheckBox *m_boolSelest2;
    QCheckBox *m_boolSelest3;
    QCheckBox *m_boolSelest4;
    QCheckBox *m_boolSelest5;

    // Colors
    QComboBox *m_colorCombo[10];
};

#endif // ADVANCEDLAYOUTDIALOG_H
