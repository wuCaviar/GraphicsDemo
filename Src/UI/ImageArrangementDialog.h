#ifndef IMAGEARRANGEMENTDIALOG_H
#define IMAGEARRANGEMENTDIALOG_H

#include <QDialog>
#include <QStringList>

class QListWidget;

enum ImageArrangement
{
    ArrangeHorizontal = 0,
    ArrangeVertical,
};

namespace Ui {
class ImageArrangementDialog;
}

class ImageArrangementDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ImageArrangementDialog(QWidget *parent = nullptr);
    ~ImageArrangementDialog();

    void setFilePaths(const QStringList &paths);
    ImageArrangement arrangement() const;
    QStringList orderedPaths() const;

private:
    Ui::ImageArrangementDialog *ui;
    QListWidget *m_listWidget = nullptr;
};

#endif // IMAGEARRANGEMENTDIALOG_H
