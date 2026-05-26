#ifndef IMAGEARRANGEMENTDIALOG_H
#define IMAGEARRANGEMENTDIALOG_H

#include <QDialog>
#include <QStringList>

class QGroupBox;
class QRadioButton;
class QListWidget;

enum ImageArrangement
{
    ArrangeHorizontal = 0,
    ArrangeVertical,
};

class ImageArrangementDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ImageArrangementDialog(QWidget *parent = nullptr);

    void setFilePaths(const QStringList &paths);
    ImageArrangement arrangement() const;
    QStringList orderedPaths() const;

private:
    void setupUI();

    QGroupBox *m_arrangementGroup = nullptr;
    QRadioButton *m_horizontalRadio = nullptr;
    QRadioButton *m_verticalRadio = nullptr;
    QListWidget *m_fileList = nullptr;
};

#endif // IMAGEARRANGEMENTDIALOG_H
