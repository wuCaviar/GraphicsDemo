# GraphicsDemo 架构重构设计文档

## 目标

1. **多画布支持** — 支持多个画布 Tab，每个画布有独立的 scene、undoStack、工具状态
2. **Action 统一管理** — 基于现有 QAtActionBase 工厂框架，所有 Action 独立成类，全局注册
3. **MainWindow 职能细化** — 从当前 ~230 行声明瘦身为纯组装器
4. **Page 体系** — Tab 不仅可展示画布，也可嵌入数据表格、图层管理等自定义页面
5. **Service 抽象** — 全局服务统一注册、统一获取、统一生命周期管理

---

## 1. 整体架构

```bash
┌──────────────────────────────────────────────────────────────────┐
│                      MainWindow（纯组装器）                        │
│  只做：创建各组件实例、信号连接、窗口状态持久化、closeEvent          │
│  不含：任何菜单/工具栏/状态栏的创建逻辑，不含任何业务逻辑              │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────────────── 全局单例层 ──────────────────────────┐ │
│  │                         AppContext                          │ │
│  │  统一入口：Page 管理 + Action 管理 + Service 注册/获取         │ │
│  │  信号转发：pageSwitched → 通知所有监听者                       │ │
│  └───────────────────────────────────────────────────────────┘ │
│                                                                  │
│  ┌─────────────────────── UI 组装层 ──────────────────────────┐ │
│  │  MenuBarBuilder     ToolBarDirector    StatusBarDirector   │ │
│  │  声明式构建菜单栏     按 Page 类型显示/    状态栏控件与数据     │ │
│  │  从 AppContext        隐藏工具栏组        源绑定              │ │
│  │  获取 Action         从 AppContext        从 AppContext +     │ │
│  │  获取 Action          Page 获取数据       │ │
│  └───────────────────────────────────────────────────────────┘ │
│                                                                  │
│  ┌─────────────────────── Page 层 ────────────────────────────┐ │
│  │  QAtPage 抽象基类 → CanvasPage / DataTablePage / ...       │ │
│  │  每个 Page 封装自己的 View/Model/UndoStack                    │ │
│  └───────────────────────────────────────────────────────────┘ │
│                                                                  │
│  ┌─────────────────────── Action 层 ──────────────────────────┐ │
│  │  QAtActionBase → File/Edit/Draw/Arrange/View/Dialog Actions│ │
│  │  pageTypes() 声明能力边界，框架自动管理可见性/可用性           │ │
│  └───────────────────────────────────────────────────────────┘ │
│                                                                  │
│  ┌─────────────────────── Service 层 ─────────────────────────┐ │
│  │  QAtService 基类 → Clipboard/Theme/Progress/Network/       │ │
│  │  Process/Project/Undo Service                              │ │
│  │  每个 Service 独立，通过 AppContext 注册和获取                 │ │
│  └───────────────────────────────────────────────────────────┘ │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

### 依赖原则

```bash
所有层只依赖 AppContext，不依赖 MainWindow

  MenuBarBuilder ──→ AppContext.getQAction(token)
  ToolBarDirector ──→ AppContext.actionsByCategory("Draw")
  StatusBarDirector ──→ AppContext.activePage()
  QAtActionBase ──→ AppContext.activeCanvasPage() / AppContext.clipboard()
  QAtPage ──→ AppContext (pageSwitched 信号)
```

---

## 2. Core 层

### 2.1 AppContext — 应用全局上下文

```cpp
// Src/Core/AppContext.h
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
    void     registerAction(QAtActionBasePtr action);
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

private:
    AppContext();
    QMap<QString, QAtService*> m_services;
    QString m_projectPath;
    bool    m_projectModified = false;
    SaveCallback m_maybeSaveProject;
};
```

### 2.2 PageManager — Page 生命周期管理

```cpp
// Src/Core/PageManager.h
// 管理所有 Page 的注册/注销/切换，发信号通知 AppContext

class PageManager : public QObject
{
    Q_OBJECT
public:
    static PageManager& get();

    void registerPage(QAtPage *page);
    void unregisterPage(const QString &pageId);
    void setActivePage(const QString &pageId);

    QAtPage*        activePage() const;
    QAtPage*        page(const QString &pageId) const;
    QList<QAtPage*> allPages() const;
    QString         activePageId() const;

    QAtCanvasPage*  activeCanvasPage() const;
    QList<QAtPage*> pagesByType(const QString &type) const;

signals:
    void pageSwitched(const QString &pageId, const QString &pageType);
    void pageAdded(const QString &pageId);
    void pageRemoved(const QString &pageId);

private:
    QMap<QString, QAtPage*> m_pages;
    QString                 m_activePageId;
};
```

---

## 3. Service 层

### 3.1 QAtService — 服务抽象基类

```cpp
// Src/Core/QAtService.h
// 所有全局服务的抽象基类

class QAtService : public QObject
{
    Q_OBJECT

public:
    explicit QAtService(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~QAtService() = default;

    virtual QString serviceId() const = 0;       // 唯一标识
    virtual QString description() const;          // 功能描述

    virtual bool initialize();                    // 初始化，返回 false 表示失败
    virtual void shutdown();                      // 清理资源

    virtual QStringList dependencies() const;     // 依赖的其他 service ID
};
```

### 3.2 服务清单

```bash
QAtService（抽象基类）
│
├── ClipboardService          serviceId="clipboard"
│   · 图元 ↔ QMimeData 序列化/反序列化
│   · copy(items) → MimeData to clipboard
│   · paste(scene) → items from clipboard
│   · cut(items, scene) → copy + remove
│   · 管理 MIME format 常量
│
├── ThemeService              serviceId="theme"
│   · 主题加载/切换/持久化
│   · currentTheme() / setTheme(name) / availableThemes()
│   · 信号: themeChanged(name)
│   · 内部操作: setStyleSheet(qApp)
│
├── ProgressService           serviceId="progress"
│   · 多任务进度追踪与 UI 展示（原 ProgressManager）
│   · createTask(name) → taskId
│   · updateTask(id, pct) / completeTask(id)
│   · 信号: taskProgressChanged(id, pct), taskCompleted(id)
│
├── NetworkService            serviceId="network"
│   · HTTP 请求发送与响应解析（原 NetWorkUtils）
│   · sendRequest(json, type) / cancelRequest(id)
│   · 信号: requestFinished(json, type), requestFailed(error)
│
├── ProcessService            serviceId="process"
│   · 子进程生命周期管理（原 ProcessGuard）
│   · launch(program, args) / stopAll() / isRunning(id)
│   · 信号: processStarted(id), processExited(id, code)
│
├── ProjectService            serviceId="project"
│   · 项目文件状态管理
│   · projectPath() / setProjectPath() / isModified() / setModified()
│   · 信号: projectPathChanged(path), projectModifiedChanged(modified)
│
└── UndoService               serviceId="undo"
    · 活动画布的 undo/redo 代理
    · undo() / redo() / canUndo() / canRedo()
    · 监听 pageSwitched → 自动绑定新画布的 undoStack
    · 信号: undoStateChanged(canUndo, canRedo)
```

### 3.3 AppContext 服务注册实现

```cpp
template<typename T>
void AppContext::registerService(T *service)
{
    static_assert(std::is_base_of_v<QAtService, T>,
                  "Service must inherit from QAtService");
    m_services[service->serviceId()] = service;
    if (!service->initialize()) {
        qWarning() << "Service" << service->serviceId() << "failed to initialize";
    }
}

template<typename T>
T* AppContext::service() const
{
    return static_cast<T*>(m_services.value(T().serviceId()));
}
```

---

## 4. Action 体系

### 4.1 QAtActionBase 增强

```cpp
// Src/Action/QAtActionBase.h
// 每个 Action 声明自己支持的 Page 类型
// pageTypes() 为空 = 全局 Action，始终可用

using PageTypeSet = QSet<QString>;

class QAtActionBase
{
public:
    virtual ~QAtActionBase() = default;

    // ---- 纯虚：子类必须实现 ----
    virtual QString     token() const = 0;
    virtual PageTypeSet pageTypes() const = 0;
    virtual void        _execute() = 0;

    // ---- 可选覆盖 ----
    virtual QIcon        _icon()            { return {}; }
    virtual QString      _text()            { return {}; }
    virtual QString      _tooltip()         { return {}; }
    virtual QKeySequence _shortcut()        { return {}; }
    virtual bool         _isCheckable()     { return false; }
    virtual QString      _category()        { return {}; }
    virtual void         _onUpdateState(bool &enabled, bool &checked, bool &visible);

    // ---- 框架注入 ----
    void setContext(AppContext *ctx);
    AppContext* context() const;

    // ---- 桥接到 Qt ----
    QAction* createQAction(QObject *parent);
    void     updateQAction(QAction *act);

protected:
    QString     m_token;
    PageTypeSet m_pageTypes;
    AppContext *m_context = nullptr;
};

// 工厂 + 自注册宏保持不变
#define ENABLE_REGISTER_ACTION(class_name) \
class _Action##class_name \
{ \
    public: \
        static QAtActionBasePtr instance() \
        { return QAtActionBasePtr(new class_name); } \
    private: \
        static const QAtActionFactory::_Register m_stRegister; \
};

#define REGISTER_ACTION(action_token, class_name) \
const QAtActionFactory::_Register class_name::_Action##class_name::m_stRegister( \
    #action_token, class_name::_Action##class_name::instance);
```

### 4.2 ActionContext — 运行时上下文

```cpp
// Src/Action/ActionContext.h
// Action 专属的轻量运行时上下文
// 提供 Action 执行所需的 Page 和服务访问

class ActionContext : public QObject
{
    Q_OBJECT
public:
    explicit ActionContext(QObject *parent = nullptr);

    // ---- Page 访问 ----
    QAtPage*        activePage() const;
    QAtCanvasPage*  activeCanvasPage() const;

    // ---- 服务访问 ----
    ClipboardService* clipboard() const;
    ThemeService*     theme() const;
    ProgressService*  progress() const;
    NetworkService*   network() const;
    ProcessService*   process() const;
    ProjectService*   project() const;
    UndoService*      undo() const;

    // 回调
    void setMaybeSaveProject(std::function<bool()> cb);
    bool maybeSaveProject();
};
```

### 4.3 类层级

```bash
QAtActionBase
│
├── QAtFileActionBase       category="File"   pageTypes={}（全局）
│   ├── NewAction            → 新建 CanvasPage
│   ├── OpenProjectAction    → 打开项目文件
│   ├── SaveProjectAction    → 保存项目文件
│   ├── ImportImageAction    → 导入图片到当前 CanvasPage
│   ├── ExportImageAction    → 导出当前 CanvasPage
│   └── ExitAction           → 退出应用
│
├── QAtEditActionBase        category="Edit"   pageTypes={"canvas"}
│   ├── UndoAction            → 调用 UndoService.undo()
│   ├── RedoAction            → 调用 UndoService.redo()
│   ├── CutAction             → ClipboardService.cut()
│   ├── CopyAction            → ClipboardService.copy()
│   ├── PasteAction           → ClipboardService.paste()
│   ├── DeleteAction          → RemoveItemsCommand
│   └── SelectAllAction       → 选中画布所有图元
│
├── QAtDrawActionBase        category="Draw"   pageTypes={"canvas"}  checkable=true
│   ├── SelectToolAction      → view.setTool(Tool::Select)
│   ├── RectToolAction        → view.setTool(Tool::Rect)
│   ├── EllipseToolAction     → view.setTool(Tool::Ellipse)
│   ├── LineToolAction        → view.setTool(Tool::Line)
│   ├── BezierCurveToolAction → view.setTool(Tool::BezierCurve)
│   ├── FreehandToolAction    → view.setTool(Tool::Freehand)
│   └── TextToolAction        → view.setTool(Tool::Text)
│
├── QAtArrangeActionBase     category="Arrange" pageTypes={"canvas"}
│   ├── BringToFrontAction    → ZValueChangeCommand
│   ├── SendToBackAction      → ZValueChangeCommand
│   ├── GroupAction           → GroupItemsCommand
│   ├── UngroupAction         → UngroupItemsCommand
│   ├── FitCanvasToItemsAction→ MoveItemsCommand + CanvasResizeCommand
│   ├── RotateCWAction        → RotationChangeCommand(90°)
│   ├── RotateCCWAction       → RotationChangeCommand(-90°)
│   ├── Rotate180Action       → RotationChangeCommand(180°)
│   ├── AlignActions          → AlignItemsCommand
│   ├── DistributeActions     → MoveItemsCommand
│   └── AutoLayoutAction      → LayoutEngine
│
├── QAtViewActionBase        category="View"   pageTypes={}（全局）
│   ├── ToggleGridAction      → view.setGridVisible()
│   ├── FitToCanvasAction     → view.fitToCanvas()
│   ├── ResetZoomAction       → view.setZoomLevel(1.0)
│   └── ThemeAction           → ThemeService.setTheme()
│
└── QAtDialogActionBase      category="Settings" pageTypes={}（全局）
    ├── SettingsAction         → SettingsDialog
    ├── PreferencesAction      → PreferencesDialog
    ├── AboutAction            → QMessageBox::about()
    └── AlignLayoutDialogAction → 显示/置顶 AlignLayoutDialog
```

### 4.4 状态刷新机制

```bash
PageManager::pageSwitched(id, type)
        │
        ▼
AppContext::pageSwitched(id, type)  // 转发信号
        │
        ▼
ActionManager::refreshAllActions()
        │
        ├── 遍历所有已注册的 QAtActionBase
        ├── action.pageTypes() 为空        → 始终启用，调用 _onUpdateState
        ├── action.pageTypes() 包含当前 type → 启用，调用 _onUpdateState
        └── action.pageTypes() 不包含       → 禁用 + 隐藏
        │
        ▼
ActionManager::updateAllQActions()
        → 批量刷新关联的 Qt QAction 状态
```

---

## 5. Page 体系

### 5.1 QAtPage — 抽象基类

```cpp
// Src/Page/QAtPage.h
// 所有 Tab 页面的抽象基类，继承 QWidget

class QAtPage : public QWidget
{
    Q_OBJECT

public:
    explicit QAtPage(QWidget *parent = nullptr);
    virtual ~QAtPage();

    virtual QString  pageId() const = 0;       // 全局唯一 ID
    virtual QString  pageType() const = 0;      // "canvas" / "datatable" / ...
    virtual QString  title() const = 0;         // Tab 标签标题
    virtual QIcon    icon() const;              // Tab 图标，可选

    virtual void onActivated();                 // 切换到此页面
    virtual void onDeactivated();               // 切出此页面

    // 序列化
    virtual QJsonObject saveState() const;
    virtual bool restoreState(const QJsonObject &state);

signals:
    void titleChanged(const QString &newTitle);
};

// PageType 常量
namespace PageType {
    const QString Canvas     = "canvas";
    const QString DataTable  = "datatable";
    const QString LayerPanel = "layerpanel";
}
```

### 5.2 QAtCanvasPage — 画布页面

```cpp
// Src/Page/QAtCanvasPage.h
// 封装一个完整画布：QAtGraphicsView + QGraphicsScene + QUndoStack

class QAtCanvasPage : public QAtPage
{
    Q_OBJECT

public:
    QAtCanvasPage(const QString &id, QWidget *parent = nullptr);

    QString  pageId() const override   { return m_pageId; }
    QString  pageType() const override { return PageType::Canvas; }
    QString  title() const override;

    QAtGraphicsView*  view() const;
    QGraphicsScene*   scene() const;
    QUndoStack*       undoStack() const;
    CanvasItem*       canvasItem() const;
    Tool              currentTool() const;
    void              setTool(Tool tool);
    qreal             zoomLevel() const;

    void              setCanvasSize(const QSizeF &size);
    void              clearCanvas(const QSizeF &size);
    QList<QGraphicsItem*> selectedItems() const;

signals:
    void itemAdded(QGraphicsItem *item);
    void selectionChanged();
    void toolChanged(Tool tool);
    void zoomChanged(qreal level);
    void mousePositionChanged(const QPointF &scenePos);

    // 属性变更
    void penChanged(QGraphicsItem *item, const QPen &oldPen, const QPen &newPen);
    void brushChanged(QGraphicsItem *item, const QBrush &oldBrush, const QBrush &newBrush);
    void geometryChanged(QGraphicsItem *item, const QRectF &oldRect, const QRectF &newRect);
    // ...

private:
    QString          m_pageId;
    QAtGraphicsView *m_view;
    QUndoStack      *m_undoStack;
};
```

### 5.3 QAtDataTablePage — 数据表格（示例新 Page 类型）

```cpp
// Src/Page/QAtDataTablePage.h
// 以表格形式展示所有图元的属性数据

class QAtDataTablePage : public QAtPage
{
    Q_OBJECT
public:
    QAtDataTablePage(const QString &id, QWidget *parent = nullptr);

    QString  pageId() const override   { return m_pageId; }
    QString  pageType() const override { return PageType::DataTable; }
    QString  title() const override    { return tr("Item Data"); }

    void onActivated() override;

private:
    QString     m_pageId;
    QTableView *m_tableView;
};
```

### 5.5 Page 与 Action 协作流程

```bash
用户点击 "Import Image"
     │
     ▼
ImportImageAction::_execute()
     → AppContext::get().activeCanvasPage()
     → if (page == nullptr) 弹出提示："请先创建或切换到画布"
     → page->scene() → 添加图片

用户切换 Tab 到 DataTablePage
     │
     ▼
PageManager::setActivePage("datatable-1")
     → 发射 pageSwitched("datatable-1", "datatable")
         │
         ├── AppContext 转发信号
         ├── ActionManager::refreshAllActions()
         │   → GroupAction.pageTypes() = {"canvas"} ≠ "datatable"
         │   → 禁用 + 隐藏 GroupAction 的 QAction
         │   → ImportImageAction.pageTypes() = {"canvas"}
         │   → 禁用 ImportImageAction
         ├── ToolBarDirector → 隐藏 DrawBar、AlignBar
         ├── StatusBarDirector → 更新标题标签
         ├── UndoService → 重新绑定 CanvasPage 的 undoStack
         └── PropertyPanel → 清空选择
```

---

## 6. UI 组装层

### 6.1 MenuBarBuilder

```cpp
// Src/UI/MenuBarBuilder.h
// 声明式构建菜单栏，从 AppContext 获取 Action

class MenuBarBuilder
{
public:
    explicit MenuBarBuilder(QMenuBar *menuBar);

    void build();

private:
    void buildFileMenu();
    void buildEditMenu();
    void buildArrangeMenu();
    void buildViewMenu();
    void buildSettingsMenu();
    void buildHelpMenu();

    QMenu* buildMenuFromCategory(const QString &title, const QString &category);
    QMenuBar *m_menuBar;
};

void MenuBarBuilder::buildEditMenu()
{
    auto *menu = buildMenuFromCategory(tr("&Edit"), "Edit");
    connect(menu, &QMenu::aboutToShow, []() {
        AppContext::get().refreshAllActions();
    });
}
```

### 6.2 ToolBarDirector

```cpp
// Src/UI/ToolBarDirector.h
// 管理多个 QToolBar 的创建、布局和 Page 感知的显示/隐藏

class ToolBarDirector : public QObject
{
    Q_OBJECT

public:
    explicit ToolBarDirector(QMainWindow *mainWindow);
    void build();

private slots:
    void onPageSwitched(const QString &pageId, const QString &pageType);

private:
    void buildFileEditBar();
    void buildDrawBar();
    void buildAlignBar();

    QMainWindow  *m_mainWindow;
    QToolBar     *m_fileEditBar  = nullptr;
    QToolBar     *m_drawBar      = nullptr;
    QToolBar     *m_alignBar     = nullptr;

    // 工具栏可见性规则
    QMap<QToolBar*, QSet<QString>> m_visibilityRules;
};

void ToolBarDirector::buildDrawBar()
{
    m_drawBar = new QToolBar(tr("Drawing Tools"), m_mainWindow);
    auto *group = new QActionGroup(this);
    group->setExclusive(true);

    auto actions = AppContext::get().actionsByCategory("Draw");
    for (auto &a : actions) {
        QAction *qa = AppContext::get().getQAction(a->token());
        qa->setCheckable(true);
        group->addAction(qa);
        m_drawBar->addAction(qa);
    }

    m_mainWindow->addToolBar(Qt::LeftToolBarArea, m_drawBar);
    m_visibilityRules[m_drawBar] = { PageType::Canvas };
}

void ToolBarDirector::onPageSwitched(const QString &, const QString &pageType)
{
    for (auto it = m_visibilityRules.begin(); it != m_visibilityRules.end(); ++it) {
        it.key()->setVisible(it.value().contains(pageType));
    }
}
```

### 6.3 StatusBarDirector

```cpp
// Src/UI/StatusBarDirector.h
// 管理状态栏控件及其数据源绑定

class StatusBarDirector : public QObject
{
    Q_OBJECT

public:
    explicit StatusBarDirector(QStatusBar *statusBar);
    void build();

private slots:
    void onPageSwitched(const QString &pageId, const QString &pageType);
    void onMousePositionChanged(const QPointF &scenePos);
    void onZoomChanged(qreal level);
    void onToolChanged(Tool tool);

private:
    void buildPositionLabel();
    void buildZoomSlider();
    void buildZoomLabel();
    void buildCanvasLabel();
    void buildToolLabel();
    void buildProgressWidget();
    void buildTaskHistoryButton();

    QStatusBar *m_statusBar;
    QLabel     *m_posLabel      = nullptr;
    QLabel     *m_zoomLabel     = nullptr;
    QLabel     *m_canvasLabel   = nullptr;
    QLabel     *m_toolLabel     = nullptr;
    QSlider    *m_zoomSlider    = nullptr;
    QToolButton *m_taskHistoryBtn = nullptr;
};
```

### 6.4 MainWindow 最终形态

```cpp
// MainWindow — 纯组装器，~80 行
class MainWindow : public QMainWindow
{
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    bool _maybeSaveProject();

    void _initServices();
    void _initPages();
    void _initActions();
    void _initUI();
    void _initConnections();
    void loadWindowState();
    void saveWindowState();

    QTabWidget        *m_tabWidget;
    PropertyPanel     *m_propertyPanel;
    AlignLayoutDialog *m_alignLayoutDlg;
    ProgressManager   *m_progressMgr;
    RulerBar          *m_hRuler;
    RulerBar          *m_vRuler;
};

void MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    _initServices();
    _initPages();
    _initActions();
    _initUI();
    _initConnections();
    loadWindowState();
}

void MainWindow::_initUI()
{
    new MenuBarBuilder(ui->menubar)->build();

    auto *toolBars = new ToolBarDirector(this);
    toolBars->build();
    connect(&AppContext::get(), &AppContext::pageSwitched,
            toolBars, &ToolBarDirector::onPageSwitched);

    auto *statusBar = new StatusBarDirector(statusBar());
    statusBar->build();
    connect(&AppContext::get(), &AppContext::pageSwitched,
            statusBar, &StatusBarDirector::onPageSwitched);

    // PropertyPanel dock
    m_propertyPanel = new PropertyPanel(this);
    addDockWidget(Qt::RightDockWidgetArea, m_propertyPanel);

    // AlignLayout dock
    m_alignLayoutDlg = new AlignLayoutDialog(this);
    splitDockWidget(m_propertyPanel, m_alignLayoutDlg, Qt::Vertical);
    m_alignLayoutDlg->hide();

    // Rulers
    m_hRuler = new RulerBar(Qt::Horizontal, this);
    m_vRuler = new RulerBar(Qt::Vertical, this);
}
```

---

## 7. 信号连接全景图

```bash
                        AppContext (信号转发中心)
                        ┌──────────────────────────────────┐
                        │                                  │
  PageManager ──────────→ pageSwitched(id, type)           │
                        │                                  │
                        │   监听者：                        │
                        │   ├── ActionManager              │
                        │   │   refreshAllActions()         │
                        │   │   按 pageType 更新状态         │
                        │   │                               │
                        │   ├── ToolBarDirector             │
                        │   │   按 pageType 显示/隐藏工具栏   │
                        │   │                               │
                        │   ├── StatusBarDirector           │
                        │   │   更新坐标/画布/工具标签        │
                        │   │                               │
                        │   ├── UndoService                │
                        │   │   重新绑定 undoStack           │
                        │   │                               │
                        │   └── PropertyPanel              │
                        │       清空选择 + 绑定新画布信号     │
                        │                                  │
                        └──────────────────────────────────┘

跨组件信号（不走 AppContext）：
  QAtCanvasPage::toolChanged       → StatusBarDirector::onToolChanged
  QAtCanvasPage::zoomChanged       → StatusBarDirector::onZoomChanged + RulerBar
  QAtCanvasPage::mousePositionChanged → StatusBarDirector + RulerBar
  ThemeService::themeChanged       → setStyleSheet(qApp)
  ProjectService::projectModifiedChanged → MainWindow.updateTitle
```

---

## 8. 文件结构规划

```bash
Src/
├── Core/                          # 新建：内核层
│   ├── QAtService.h / .cpp        #   服务抽象基类
│   ├── AppContext.h / .cpp        #   全局上下文
│   ├── PageManager.h / .cpp       #   Page 生命周期管理
│   ├── ThemeService.h / .cpp       #   主题管理服务
│   └── ClipboardService.h / .cpp  #   剪贴板服务
│
├── Page/                          # 新建：Page 体系
│   ├── QAtPage.h / .cpp           #   抽象基类
│   ├── QAtCanvasPage.h / .cpp     #   画布页面
│   └── QAtDataTablePage.h / .cpp  #   数据表格页面（示例）
│
├── Action/                        # 重构：扩展现有目录
│   ├── QAtActionBase.h / .cpp     #   增强基类 + 工厂
│   ├── ActionContext.h / .cpp     #   Action 运行时上下文
│   ├── QAtDrawAction.h / .cpp     #   绘图工具基类 + 具体工具
│   ├── FileActions.h / .cpp       #   文件操作 Action
│   ├── EditActions.h / .cpp       #   编辑操作 Action
│   ├── ArrangeActions.h / .cpp    #   排列操作 Action
│   ├── ViewActions.h / .cpp       #   视图操作 Action
│   └── DialogActions.h / .cpp     #   对话框 Action
│
├── UI/                            # 重构：拆分 MainWindow
│   ├── MenuBarBuilder.h / .cpp    #   菜单栏构建器（新建）
│   ├── ToolBarDirector.h / .cpp   #   工具栏管理器（新建）
│   ├── StatusBarDirector.h / .cpp #   状态栏管理器（新建）
│   ├── mainwindow.h / .cpp        #   精简后 ~80 行
│   ├── qatgraphicsview.h / .cpp   #   （保留）
│   ├── GraphicsScene.h / .cpp     #   （保留）
│   ├── PropertyPanel.h / .cpp     #   （保留）
│   └── ...                        #   其他 UI 文件不变
│
├── Utils/                         # 扩展
│   └── ...                        #   现有文件不变
│
├── Service/                       # 新建：服务实现
│   ├── ProgressService.h / .cpp   #   进度服务
│   ├── NetworkService.h / .cpp    #   网络服务
│   ├── ProcessService.h / .cpp    #   进程服务
│   ├── ProjectService.h / .cpp    #   项目状态服务
│   └── UndoService.h / .cpp       #   Undo/Redo 代理服务
│
├── Commands/                      # 不变
├── Items/                         # 不变
├── App/                           # 不变
└── NetWork/                       # 不变
```

### 新建 vs 修改文件汇总

| 操作 | 文件                             | 说明                     |
| ---- | -------------------------------- | ------------------------ |
| 新建 | `Core/QAtService.h/.cpp`         | 服务抽象基类             |
| 新建 | `Core/AppContext.h/.cpp`         | 全局上下文               |
| 新建 | `Core/PageManager.h/.cpp`        | Page 管理                |
| 新建 | `Core/ThemeService.h/.cpp`       | 主题服务                 |
| 新建 | `Core/ClipboardService.h/.cpp`   | 剪贴板服务               |
| 新建 | `Page/QAtPage.h/.cpp`            | Page 基类                |
| 新建 | `Page/QAtCanvasPage.h/.cpp`      | 画布 Page                |
| 新建 | `Page/QAtDataTablePage.h/.cpp`   | 数据表格 Page            |
| 新建 | `Service/ProgressService.h/.cpp` | 进度服务                 |
| 新建 | `Service/NetworkService.h/.cpp`  | 网络服务                 |
| 新建 | `Service/ProcessService.h/.cpp`  | 进程服务                 |
| 新建 | `Service/ProjectService.h/.cpp`  | 项目状态服务             |
| 新建 | `Service/UndoService.h/.cpp`     | Undo 代理服务            |
| 新建 | `Action/ActionContext.h/.cpp`    | Action 上下文            |
| 新建 | `Action/FileActions.h/.cpp`      | 文件 Action              |
| 新建 | `Action/EditActions.h/.cpp`      | 编辑 Action              |
| 新建 | `Action/ArrangeActions.h/.cpp`   | 排列 Action              |
| 新建 | `Action/ViewActions.h/.cpp`      | 视图 Action              |
| 新建 | `Action/DialogActions.h/.cpp`    | 对话框 Action            |
| 新建 | `UI/MenuBarBuilder.h/.cpp`       | 菜单构建                 |
| 新建 | `UI/ToolBarDirector.h/.cpp`      | 工具栏管理               |
| 新建 | `UI/StatusBarDirector.h/.cpp`    | 状态栏管理               |
| 修改 | `Action/QAtActionBase.h/.cpp`    | 增强基类                 |
| 修改 | `Action/QAtDrawAction.h/.cpp`    | 改用 AppContext 获取画布 |
| 修改 | `UI/mainwindow.h/.cpp`           | 精简为组装器             |
| 修改 | `Src.pro`                        | 添加新文件               |

---

## 9. 分阶段实施

| 阶段   | 内容                                                                   | 风险 | 产出                       |
| ------ | ---------------------------------------------------------------------- | ---- | -------------------------- |
| **P1** | Core 层：QAtService + AppContext + PageManager                         | 低   | 纯新增                     |
| **P2** | Service 提取：ThemeService + ClipboardService + ProgressService + 其他 | 低   | 从现有类重构               |
| **P3** | Page 体系：QAtPage + QAtCanvasPage                                     | 低   | 纯新增                     |
| **P4** | Action 增强：基类增强 + 中间基类 + createQAction 桥接 + ActionContext  | 低   | 修改现有 Action 文件       |
| **P5** | Action 迁移：逐个分类创建具体 Action 类，旧 slot 并行保留              | 中   | 6 个 Action 文件           |
| **P6** | UI 拆解：MenuBarBuilder + ToolBarDirector + StatusBarDirector          | 中   | 从 MainWindow 剥离 UI 构建 |
| **P7** | MainWindow 瘦身：切换新组件，删除旧代码                                | 中   | MainWindow ~80 行          |
| **P8** | DataTablePage 范例                                                     | 低   | 验证 Page 体系可扩展性     |
| **P9** | 多画布集成：TabWidget + PageManager 完整多画布切换                     | 高   | 多个 CanvasPage + 切换逻辑 |

**核心原则：**

- 每阶段纯新增优先，旧代码并行保留
- 每个阶段可编译可运行，不会出现大面积不可用
- 各阶段间临时代码重复是可接受的
- 验证通过后再删除旧代码

---

## 10. 关键设计决策

| 决策                                                      | 理由                                       |
| --------------------------------------------------------- | ------------------------------------------ |
| QAtActionBase 保持非 QObject                              | 实例轻量，无 moc 依赖，工厂模式自然运作    |
| AppContext 是 QObject 单例                                | 需要发射信号通知全局状态变更               |
| Service 统一基类 QAtService                               | 统一生命周期管理，统一注册/获取接口        |
| Action 通过 pageTypes() 声明能力边界                      | 框架自动管理可见性，Page 不需要关心 Action |
| MenuBarBuilder / ToolBarDirector / StatusBarDirector 独立 | 互相不依赖，只与 AppContext 通信           |
| MainWindow 只保留组装和生命周期                           | 职责单一，易于测试和维护                   |
| 旧代码并行保留直至验证通过                                | 降低迁移风险，支持随时回退                 |
