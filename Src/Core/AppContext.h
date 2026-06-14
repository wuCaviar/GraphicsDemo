#ifndef APPCONTEXT_H
#define APPCONTEXT_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QList>
#include <QSet>
#include <functional>

#include "QAtActionBase.h"

class QAction;
class QAtPage;
class QAtCanvasPage;
class QAtService;

class ClipboardService;
class ThemeService;
class ProgressService;
class NetworkService;
class ProcessService;
class ProjectService;
class UndoService;

// 应用全局状态与服务的统一访问入口
// 任何组件通过 AppContext::get() 获取所需服务，无需依赖 MainWindow
class AppContext : public QObject
{
    Q_OBJECT

public:
    static AppContext& get();

    // ==================== Page 管理 ====================
    void    registerPage(QAtPage *page);
    void    unregisterPage(const QString &pageId);
    void    setActivePage(const QString &pageId);
    QString activePageId() const;

    QAtPage*        activePage() const;
    QAtCanvasPage*  activeCanvasPage() const;
    QList<QAtPage*> allPages() const;

signals:
    void pageSwitched(const QString &pageId, const QString &pageType);
    void pageAdded(const QString &pageId);
    void pageRemoved(const QString &pageId);

    // ==================== Action 管理 ====================
public:
    void     registerAction(const QAtActionBasePtr &action);
    QAction* getQAction(const QString &token) const;
    void     refreshAllActions();
    QList<QAtActionBasePtr> actionsByCategory(const QString &category) const;

    // ==================== Service 注册与获取 ====================
    void registerService(QAtService *service);

    template<typename T>
    T* service() const;

    // 内置服务的便捷访问器
    ClipboardService*  clipboard() const;
    ThemeService*      theme() const;
    ProgressService*   progress() const;
    NetworkService*    network() const;
    ProcessService*    process() const;
    ProjectService*    project() const;
    UndoService*       undo() const;

    // ==================== 项目状态 ====================
    QString projectPath() const;
    void    setProjectPath(const QString &path);
    bool    isProjectModified() const;
    void    setProjectModified(bool modified);

    // ==================== 回调注入 ====================
    using SaveCallback = std::function<bool()>;
    void setMaybeSaveProject(SaveCallback cb);
    bool maybeSaveProject();

    // Action callbacks for operations that need MainWindow-specific logic
    using ActionCallback = std::function<bool(const QString &actionToken)>;
    void setActionCallback(ActionCallback cb);
    bool invokeActionCallback(const QString &actionToken);

private:
    AppContext();
    ~AppContext() override = default;
    Q_DISABLE_COPY(AppContext)

    // Service storage
    QMap<QString, QAtService*> m_services;

    // Action storage: token → QAtActionBasePtr
    QMap<QString, QAtActionBasePtr> m_actions;
    // token → QAction* (Qt widget bridge, lazy-created)
    mutable QMap<QString, QAction*> m_qActions;
    // token → category (populated at registration)
    QMap<QString, QString> m_actionCategories;

    // Project state
    QString m_projectPath;
    bool    m_projectModified = false;

    // Callbacks
    SaveCallback m_maybeSaveProject;
    ActionCallback m_actionCallback;
};

// Template implementation — matches design doc Section 3.3
template<typename T>
T* AppContext::service() const
{
    return static_cast<T*>(m_services.value(T().serviceId()));
}

#endif // APPCONTEXT_H
