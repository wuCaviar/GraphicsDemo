#include "PageManager.h"

PageManager &PageManager::get()
{
    static PageManager instance;
    return instance;
}

void PageManager::registerPage(QAtPage *page)
{
    if (!page)
        return;
    const QString &id = page->pageId();
    if (m_pages.contains(id))
        return;
    m_pages[id] = page;
    emit pageAdded(id);
}

void PageManager::unregisterPage(const QString &pageId)
{
    if (!m_pages.contains(pageId))
        return;
    m_pages.remove(pageId);
    if (m_activePageId == pageId) {
        m_activePageId.clear();
    }
    emit pageRemoved(pageId);
}

void PageManager::setActivePage(const QString &pageId)
{
    if (m_activePageId == pageId)
        return;
    if (!m_pages.contains(pageId))
        return;

    QAtPage *oldPage = m_pages.value(m_activePageId);
    if (oldPage) {
        oldPage->onDeactivated();
    }

    m_activePageId = pageId;

    QAtPage *newPage = m_pages.value(pageId);
    if (newPage) {
        newPage->onActivated();
        emit pageSwitched(pageId, newPage->pageType());
    }
}

QAtPage *PageManager::activePage() const
{
    return m_pages.value(m_activePageId);
}

QAtPage *PageManager::page(const QString &pageId) const
{
    return m_pages.value(pageId);
}

QList<QAtPage *> PageManager::allPages() const
{
    return m_pages.values();
}

QString PageManager::activePageId() const
{
    return m_activePageId;
}

QString PageManager::activePageType() const
{
    QAtPage *p = activePage();
    return p ? p->pageType() : QString();
}

QAtCanvasPage *PageManager::activeCanvasPage() const
{
    QAtPage *p = activePage();
    if (p && p->pageType() == PageType::Canvas)
        return static_cast<QAtCanvasPage *>(p);
    return nullptr;
}

QList<QAtPage *> PageManager::pagesByType(const QString &type) const
{
    QList<QAtPage *> result;
    for (auto *p : m_pages) {
        if (p->pageType() == type)
            result.append(p);
    }
    return result;
}
