#ifndef IMAGEARRANGEMENTDIALOG_H
#define IMAGEARRANGEMENTDIALOG_H

#include <QDialog>
#include <QStringList>
#include <QFuture>
#include <QFutureWatcher>

class QLabel;
class QRadioButton;
class QButtonGroup;

enum ImageArrangement
{
    ArrangeHorizontal = 0,
    ArrangeVertical,
};

enum FileInfoType
{
    fitFailed = -2,
    fitExclude = -1,
    fitNone = 0,
    fitSuccess,
};

struct FileDpiInfo
{
    FileInfoType type = fitNone;
    QPair<int, int> dpi = { 0, 0 };
    QStringList paths;

    FileDpiInfo &operator+=(const FileDpiInfo &other)
    {
        if (type != other.type)
            return *this;
        if (type == fitSuccess && dpi != other.dpi)
            return *this;
        paths += other.paths;
        return *this;
    }

    bool empty() const { return paths.isEmpty(); }
};

namespace Ui {
class FileInfoWidget;
}

class FileInfoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FileInfoWidget(QWidget *parent = nullptr);
    ~FileInfoWidget();

    void setFileInfo(const FileDpiInfo &infos);
    QStringList orderedPaths() const;
    QRadioButton *radioButton() const;

private:
    Ui::FileInfoWidget *ui;
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
    void onDpiFinished();

    Ui::ImageArrangementDialog *ui;
    QFutureWatcher<FileDpiInfo> *m_dpiWatcher;
    QButtonGroup *m_radioGroup;
    QList<FileInfoWidget *> m_lstFileInfoWidget;
    QLabel *m_loadingLabel = nullptr;
};

#endif // IMAGEARRANGEMENTDIALOG_H
