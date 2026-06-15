#ifndef PAGEMANAGER_H
#define PAGEMANAGER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QList>

#include "QAtPage.h"
#include "QAtCanvasPage.h"

// Page 生命周期管理 — 单例
// 管理所有 Page 的注册/注销/切换，发信号通知 AppContext
class PageManager : public QObject
{
    Q_OBJECT

public:
    static PageManager &get();

    void registerPage(QAtPage *page);
    void unregisterPage(const QString &pageId);
    void setActivePage(const QString &pageId);

    QAtPage *activePage() const;
    QAtPage *page(const QString &pageId) const;
    QList<QAtPage *> allPages() const;
    QString activePageId() const;

    QAtCanvasPage *activeCanvasPage() const;
    QString activePageType() const;
    QList<QAtPage *> pagesByType(const QString &type) const;

signals:
    void pageSwitched(const QString &pageId, const QString &pageType);
    void pageAdded(const QString &pageId);
    void pageRemoved(const QString &pageId);

private:
    PageManager() = default;
    Q_DISABLE_COPY(PageManager)

    QMap<QString, QAtPage *> m_pages;
    QString m_activePageId;
};

#endif // PAGEMANAGER_H
