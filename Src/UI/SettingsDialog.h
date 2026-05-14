#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

class QLineEdit;
class QSpinBox;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    int resolutionX() const;
    int resolutionY() const;
    QString dotCurvePath() const;
    QString colorCurvePath() const;
    QString outputPath() const;

private:
    void setupUI();
    void loadConfig();
    void saveConfig();

    QSpinBox *m_resolutionXSpin = nullptr;
    QSpinBox *m_resolutionYSpin = nullptr;
    QLineEdit *m_dotCurveEdit = nullptr;
    QLineEdit *m_colorCurveEdit = nullptr;
    QLineEdit *m_outputPathEdit = nullptr;
};

#endif // SETTINGSDIALOG_H
