#ifndef ACTIONCONTEXT_H
#define ACTIONCONTEXT_H

#include <QObject>
#include <functional>

class QAtPage;
class QAtCanvasPage;
class QWidget;
class ClipboardService;
class ThemeService;
class ProgressService;
class NetworkService;
class ProcessService;
class ProjectService;
class UndoService;

// Lightweight runtime context for Action classes
// Delegates to AppContext — no dependency on MainWindow
class ActionContext : public QObject
{
    Q_OBJECT
public:
    explicit ActionContext(QObject *parent = nullptr);

    QAtPage*        activePage() const;
    QAtCanvasPage*  activeCanvasPage() const;

    // Service access
    ClipboardService* clipboard() const;
    ThemeService*     theme() const;
    ProgressService*  progress() const;
    NetworkService*   network() const;
    ProcessService*   process() const;
    ProjectService*   project() const;
    UndoService*      undo() const;

    // Callbacks
    using SaveCallback = std::function<bool()>;
    void setMaybeSaveProject(SaveCallback cb);
    bool maybeSaveProject();
};

#endif // ACTIONCONTEXT_H
