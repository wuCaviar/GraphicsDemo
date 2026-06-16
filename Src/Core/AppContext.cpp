#include "AppContext.h"
#include "PageManager.h"
#include "QAtService.h"
#include "AlignWidget.h"

#include "ClipboardService.h"
#include "ThemeService.h"
#include "ProgressService.h"
#include "NetworkService.h"
#include "ProcessService.h"
#include "ProjectService.h"
#include "UndoService.h"

#include <QAction>

// ==================== AppContext ====================

AppContext::AppContext()
{
    // 转发 PageManager 的信号，使外部只需连接 AppContext
    auto &pm = PageManager::get();
    connect(&pm, &PageManager::pageSwitched, this, &AppContext::pageSwitched);
    connect(&pm, &PageManager::pageAdded, this, &AppContext::pageAdded);
    connect(&pm, &PageManager::pageRemoved, this, &AppContext::pageRemoved);
}

AppContext &AppContext::get()
{
    static AppContext instance;
    return instance;
}

// ==================== Page 管理 ====================

void AppContext::registerPage(QAtPage *page)
{
    PageManager::get().registerPage(page);
}

void AppContext::unregisterPage(const QString &pageId)
{
    PageManager::get().unregisterPage(pageId);
}

void AppContext::setActivePage(const QString &pageId)
{
    PageManager::get().setActivePage(pageId);
}

QString AppContext::activePageId() const
{
    return PageManager::get().activePageId();
}

QAtPage *AppContext::activePage() const
{
    return PageManager::get().activePage();
}

QAtCanvasPage *AppContext::activeCanvasPage() const
{
    return PageManager::get().activeCanvasPage();
}

QList<QAtPage *> AppContext::allPages() const
{
    return PageManager::get().allPages();
}

// ==================== Action 管理 ====================

void AppContext::registerAction(const QAtActionBasePtr &action)
{
    if (!action)
        return;
    const QString &token = action->token();
    m_actions[token] = action;
    m_actionCategories[token] = action->_category();
}

QAction *AppContext::getQAction(const QString &token) const
{
    // Return cached QAction if already created
    auto it = m_qActions.find(token);
    if (it != m_qActions.end())
        return it.value();

    // Lazy-create from QAtActionBase
    auto ait = m_actions.find(token);
    if (ait == m_actions.end())
        return nullptr;

    QAtActionBasePtr action = ait.value();
    // QAtActionBase::createQAction is added in P4
    QAction *qa = action->createQAction(nullptr);
    if (qa) {
        m_qActions[token] = qa;
    }
    return qa;
}

void AppContext::refreshAllActions()
{
    const QString pageType = PageManager::get().activePageType();

    for (auto it = m_actions.begin(); it != m_actions.end(); ++it) {
        const QString &token = it.key();
        QAtActionBasePtr action = it.value();

        bool enabled = true;
        bool checked = false;
        bool visible = true;

        if (!action->pageTypes().isEmpty()) {
            bool pageMatch = action->pageTypes().contains(pageType);
            enabled = pageMatch;
            visible = pageMatch;
        }

        action->onUpdateState(enabled, checked, visible);

        // Sync to QAction
        auto qit = m_qActions.find(token);
        if (qit != m_qActions.end() && qit.value()) {
            QAction *qa = qit.value();
            qa->setEnabled(enabled);
            qa->setChecked(checked);
            qa->setVisible(visible);
        }
    }
}

QList<QAtActionBasePtr> AppContext::actionsByCategory(const QString &category) const
{
    QList<QAtActionBasePtr> result;
    for (auto it = m_actionCategories.begin(); it != m_actionCategories.end(); ++it) {
        if (it.value() == category) {
            auto ait = m_actions.find(it.key());
            if (ait != m_actions.end())
                result.append(ait.value());
        }
    }
    return result;
}

// ==================== Service 注册与获取 ====================

void AppContext::registerService(QAtService *service)
{
    if (!service)
        return;
    const QString &id = service->serviceId();
    if (m_services.contains(id))
        return;
    m_services[id] = service;
    if (!service->initialize()) {
        qWarning() << "Service" << id << "failed to initialize";
    }
}

ClipboardService *AppContext::clipboard() const
{
    return service<ClipboardService>();
}

ThemeService *AppContext::theme() const
{
    return service<ThemeService>();
}

ProgressService *AppContext::progress() const
{
    return service<ProgressService>();
}

NetworkService *AppContext::network() const
{
    return service<NetworkService>();
}

ProcessService *AppContext::process() const
{
    return service<ProcessService>();
}

ProjectService *AppContext::project() const
{
    return service<ProjectService>();
}

UndoService *AppContext::undo() const
{
    return service<UndoService>();
}

// ==================== 项目状态 ====================

QString AppContext::projectPath() const
{
    return m_projectPath;
}

void AppContext::setProjectPath(const QString &path)
{
    m_projectPath = path;
}

bool AppContext::isProjectModified() const
{
    return m_projectModified;
}

void AppContext::setProjectModified(bool modified)
{
    m_projectModified = modified;
}

// ==================== AlignWidget ====================

void AppContext::setAlignWidget(AlignWidget *widget)
{
    m_alignWidget = widget;
}

AlignWidget *AppContext::alignWidget() const
{
    return m_alignWidget;
}

// ==================== 回调 ====================

void AppContext::setMaybeSaveProject(SaveCallback cb)
{
    m_maybeSaveProject = std::move(cb);
}

bool AppContext::maybeSaveProject()
{
    if (m_maybeSaveProject)
        return m_maybeSaveProject();
    return true;
}

void AppContext::setActionCallback(ActionCallback cb)
{
    m_actionCallback = std::move(cb);
}

bool AppContext::invokeActionCallback(const QString &actionToken)
{
    if (m_actionCallback)
        return m_actionCallback(actionToken);
    return false;
}
