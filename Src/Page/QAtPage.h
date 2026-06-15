#ifndef QATPAGE_H
#define QATPAGE_H

#include <QWidget>
#include <QIcon>
#include <QJsonObject>
#include <QString>

// Abstract base for all tab pages
class QAtPage : public QWidget
{
    Q_OBJECT

public:
    explicit QAtPage(QWidget *parent = nullptr) : QWidget(parent) { }
    ~QAtPage() override = default;

    virtual QString pageId() const = 0;
    virtual QString pageType() const = 0;
    virtual QString title() const = 0;
    virtual QIcon icon() const { return { }; }

    virtual void onActivated() { }
    virtual void onDeactivated() { }

    virtual QJsonObject saveState() const { return { }; }
    virtual bool restoreState(const QJsonObject &) { return false; }

signals:
    void titleChanged(const QString &newTitle);
};

namespace PageType {
const QString Canvas = QStringLiteral("canvas");
const QString DataTable = QStringLiteral("datatable");
const QString LayerPanel = QStringLiteral("layerpanel");
} // namespace PageType

#endif // QATPAGE_H
