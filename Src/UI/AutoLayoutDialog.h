#ifndef AUTOLAYOUTDIALOG_H
#define AUTOLAYOUTDIALOG_H

#include <QDialog>

class QSpinBox;
class QCheckBox;
class QPushButton;

class AutoLayoutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AutoLayoutDialog(QWidget *parent = nullptr);

    /// Returns 0 if "使用纸张宽度" is unchecked (use DLL default).
    int paperWidth() const;
    /// Returns the raw interval value (5–10 mm). Caller divides by 2 for DLL.
    int elementInterval() const;
    int loopCount() const;
    int operateCount() const;
    /// Whether the user checked "使用纸张宽度".
    bool usePaperWidth() const;

private slots:
    void openAdvancedSettings();

private:
    QSpinBox *m_paperWidthSpin;
    QSpinBox *m_intervalSpin;
    QSpinBox *m_loopCountSpin;
    QSpinBox *m_operateCountSpin;
    QCheckBox *m_paperWidthCheck;
    QPushButton *m_advancedBtn;
};

#endif // AUTOLAYOUTDIALOG_H
