# Action 重构方案

## 背景

`Src/Action/QAtActionBase.h` 定义了一套 Action 工厂 + 自注册框架，但目前是**死代码**——从未被任何地方引用。项目实际的 Action 系统是 MainWindow 中 ~30 个 inline `QAction` + 对应 slot 函数。两套 "Action" 概念并存，命名冲突。

**目标：** 将 QAtActionBase 激活为项目的中心 Action 框架，把 MainWindow 中的 inline action 逻辑迁移到独立的 Action 类中，让 MainWindow 只负责布局和信号连接。

---

## 架构设计

### ActionContext — 画布感知的依赖注入

ActionContext 作为类设计，封装所有 Action 运行时依赖，并为多画布场景预留接口。

```cpp
// Src/Action/ActionContext.h
class ActionContext : public QObject
{
    Q_OBJECT
public:
    explicit ActionContext(QObject *parent = nullptr);
    ~ActionContext();

    // ---- 画布管理（多画布扩展点） ----
    struct CanvasInfo {
        QAtGraphicsView   *view      = nullptr;
        QGraphicsScene    *scene     = nullptr;
        QUndoStack        *undoStack = nullptr;
    };

    void registerCanvas(const QString &id, const CanvasInfo &info);
    void unregisterCanvas(const QString &id);
    void setActiveCanvas(const QString &id);
    QString activeCanvasId() const;
    CanvasInfo activeCanvas() const;   // 返回当前活动画布
    bool hasCanvas() const;

    // ---- 便捷访问（委托给 activeCanvas） ----
    QAtGraphicsView*  activeView() const;
    QGraphicsScene*   activeScene() const;
    QUndoStack*       activeUndoStack() const;

    // ---- 全局服务 ----
    QWidget*           parentWidget()  const { return m_parentWidget; }
    PropertyPanel*     propertyPanel() const { return m_propertyPanel; }
    AlignLayoutDialog* alignDialog()   const { return m_alignDialog; }
    ProgressManager*   progressMgr()   const { return m_progressMgr; }
    NetWorkUtils*      networkUtils()  const { return m_networkUtils; }
    ProcessGuard*      processGuard()  const { return m_processGuard; }
    QString*           projectPath()   const { return m_projectPath; }
    bool*              projectModified() const { return m_projectModified; }

    // ---- 设置器 ----
    void setParentWidget(QWidget *w);
    void setPropertyPanel(PropertyPanel *p);
    void setAlignDialog(AlignLayoutDialog *d);
    void setProgressMgr(ProgressManager *m);
    void setNetworkUtils(NetWorkUtils *n);
    void setProcessGuard(ProcessGuard *g);
    void setProjectPath(QString *p);
    void setProjectModified(bool *m);

    // ---- 回调（深度绑定 MainWindow 的操作） ----
    using SaveCallback    = std::function<bool()>;
    using ThemeCallback   = std::function<void(const QString&)>;
    using RefreshCallback = std::function<void()>;

    void setMaybeSaveProject(SaveCallback cb);
    void setSwitchTheme(ThemeCallback cb);
    void setUpdateCanvasLabel(RefreshCallback cb);
    void setUpdateToolLabel(RefreshCallback cb);

    bool maybeSaveProject();
    void switchTheme(const QString &theme);
    void updateCanvasLabel();
    void updateToolLabel();

signals:
    void canvasSwitched(const QString &id);  // 画布切换通知
    void canvasRemoved(const QString &id);   // 画布被移除

private:
    // 画布注册表
    QMap<QString, CanvasInfo> m_canvases;
    QString                   m_activeCanvasId;

    // 全局服务
    QWidget           *m_parentWidget  = nullptr;
    PropertyPanel     *m_propertyPanel = nullptr;
    AlignLayoutDialog *m_alignDialog   = nullptr;
    ProgressManager   *m_progressMgr   = nullptr;
    NetWorkUtils      *m_networkUtils  = nullptr;
    ProcessGuard      *m_processGuard  = nullptr;
    QString           *m_projectPath   = nullptr;
    bool              *m_projectModified = nullptr;

    // 回调
    SaveCallback    m_maybeSaveProject;
    ThemeCallback   m_switchTheme;
    RefreshCallback m_updateCanvasLabel;
    RefreshCallback m_updateToolLabel;
};
```

**多画布扩展设计要点：**
- `registerCanvas(id, info)` / `unregisterCanvas(id)` — 注册/注销画布，每个画布有唯一 ID
- `setActiveCanvas(id)` — 切换活动画布，发射 `canvasSwitched` 信号
- `activeView()` / `activeScene()` / `activeUndoStack()` — 便捷方法，委托给当前活动画布
- 当前单画布场景下，MainWindow 启动时注册一个默认画布 `"main"`
- 未来多画布时，tab 切换调用 `setActiveCanvas(tabId)`，所有通过 context 访问画布的 Action 自动切到正确的画布
- Action 基类提供的 `view()` / `scene()` / `undoStack()` 等便捷方法统一调用 `context()->activeXxx()`

### 类层次

```
QAtActionBase (增强: +shortcut, +tooltip, +checkable, +category, +context, +createQAction())
├── QAtFileActionBase      → NewAction, OpenProjectAction, SaveProjectAction, ImportImageAction, ExportImageAction
├── QAtEditActionBase      → UndoAction, RedoAction, CutAction, CopyAction, PasteAction, DeleteAction, SelectAllAction
├── QAtDrawActionBase (增强) → SelectToolAction, RectToolAction, EllipseToolAction, LineToolAction, BezierCurveToolAction, FreehandToolAction, TextToolAction
├── QAtArrangeActionBase   → BringToFrontAction, SendToBackAction, GroupAction, UngroupAction, FitCanvasToItemsAction, RotateCW/CCW/180Action, AutoLayoutAction
├── QAtViewActionBase      → ToggleGridAction, FitToCanvasAction, ResetZoomAction
└── QAtDialogActionBase    → AlignLayoutDialogAction, SettingsAction, PreferencesAction, AboutAction
```

### 核心机制

- `createQAction(parent)` — 桥接到 Qt QAction，自动配置 icon/text/shortcut/checkable
- `updateQAction(QAction*)` — 刷新 enabled/checked/visible 状态
- 注册宏 `ENABLE_REGISTER_ACTION` + `REGISTER_ACTION` 已有，直接复用
- 每个具体 Action 类通过 `REGISTER_ACTION(token, ClassName)` 自注册到工厂

---

## 分阶段实施

### Phase 1: 增强 QAtActionBase 基础（仅新增，无运行时变更）

**新建 `Src/Action/ActionContext.h` + `ActionContext.cpp`：**
- `ActionContext` 类实现，含画布注册/切换、全局服务、回调管理
- `CanvasInfo` 结构体：view + scene + undoStack 三元组
- `registerCanvas()` / `setActiveCanvas()` / `activeCanvas()` 等画布管理 API
- `canvasSwitched` / `canvasRemoved` 信号

**修改 `Src/Action/QAtActionBase.h`：**
- 新增成员：`m_shortcut`, `m_tooltip`, `m_checkable`, `m_category`, `m_context`
- 新增虚方法：`shortcut()`, `tooltip()`, `isCheckable()`, `category()`
- 新增：`setContext(ActionContext*)`, `context()`, `createQAction(QObject*)`, `updateQAction(QAction*)`
- 新增便捷方法：`view()`, `scene()`, `undoStack()` — 委托给 `m_context->activeXxx()`
- 修复 `setAttchViewScene` 拼写错误

**修改 `Src/Action/QAtActionBase.cpp`：**
- 实现 `createQAction()` 和 `updateQAction()`
- 实现 `view()` / `scene()` / `undoStack()` 便捷方法

### Phase 2: 创建中间基类（仅新增，无运行时变更）

中间基类通过 `context()` 访问 `ActionContext`，获取画布和服务：

```cpp
// 示例：QAtArrangeActionBase 中的便捷方法
inline QUndoStack* QAtArrangeActionBase::undoStack() const {
    return context() ? context()->activeUndoStack() : nullptr;
}
inline QGraphicsScene* QAtArrangeActionBase::scene() const {
    return context() ? context()->activeScene() : nullptr;
}
inline QAtGraphicsView* QAtArrangeActionBase::view() const {
    return context() ? context()->activeView() : nullptr;
}
```

新建文件（每组 .h + .cpp）：

| 文件 | 设置 category | 便捷方法 |
|------|--------------|---------|
| `QAtFileAction.h/.cpp` | "File" | `parentWidget()` |
| `QAtEditAction.h/.cpp` | "Edit" | `undoStack()`, `scene()`, `view()` |
| `QAtArrangeAction.h/.cpp` | "Arrange" | `undoStack()`, `scene()`, `view()`, `selectedItems()`, `filterSelectableItems()` |
| `QAtViewAction.h/.cpp` | "View" | `view()` |
| `QAtDialogAction.h/.cpp` | "Settings" | `parentWidget()` |

**修改 `Src/Action/QAtDrawAction.h/.cpp`：**
- 移除 `setAttchViewScene()`，改用 `context()->activeView()` / `activeScene()`
- 新增 `tool()` 纯虚方法、`switchToTool()` 辅助方法（`context()->activeView()->setTool(tool())`）
- 构造函数设置 `m_checkable = true`
- 当 `canvasSwitched` 信号触发时，自动更新工具栏选中状态

### Phase 3: 迁移简单 Action（Settings / View / Tools）

**新建 `Src/Action/DialogActions.h/.cpp`：**
- `SettingsAction`, `PreferencesAction`, `AboutAction`

**新建 `Src/Action/ViewActions.h/.cpp`：**
- `ToggleGridAction`, `FitToCanvasAction`, `ResetZoomAction`

**修改 `Src/Action/QAtDrawAction.h/.cpp`，替换 QAtDrawRectAction 并新增：**
- `SelectToolAction`, `RectToolAction`, `EllipseToolAction`, `LineToolAction`, `BezierCurveToolAction`, `FreehandToolAction`, `TextToolAction`
- 每个实现 `tool()` 返回对应 Tool enum，`_execute()` 调用 `switchToTool()`

**临时集成 MainWindow：** 在 `_initToolBar()` 中用工厂创建 drawing tools，旧代码保留做 fallback。

### Phase 4: 迁移 Edit Action

**新建 `Src/Action/EditActions.h/.cpp`：**
- `UndoAction` / `RedoAction` — 调用 undoStack，`_onUpdateState` 检查 canUndo/canRedo
- `CutAction` — 组合 copy + delete
- `CopyAction` / `PasteAction` — 提取 MainWindow 中的 `copyItemsToClipboard` / `pasteItemsFromClipboard` 逻辑
- `DeleteAction` — 推送 RemoveItemsCommand
- `SelectAllAction`

**提取工具函数：** 将 clipboard 序列化/反序列化代码提取到 `Src/Utils/ClipboardUtils.h/.cpp`（可选，也可直接放在 Action 类中）。

### Phase 5: 迁移 Arrange Action

**新建 `Src/Action/ArrangeActions.h/.cpp`：**
- `BringToFrontAction` / `SendToBackAction` — ZValueChangeCommand
- `GroupAction` / `UngroupAction` — GroupItemsCommand / UngroupItemsCommand
- `FitCanvasToItemsAction` — MoveItemsCommand + CanvasResizeCommand
- `RotateCWAction` / `RotateCCWAction` / `Rotate180Action` — RotationChangeCommand
- `AlignLayoutDialogAction` — 显示/置顶 dock widget
- `AutoLayoutAction` — 懒创建 LayoutEngine，自行管理

### Phase 6: 迁移 File Action（最高风险）

**新建 `Src/Action/FileActions.h/.cpp`：**
- `NewAction` — NewFileDialog + 重置画布 + 清空 undo stack
- `OpenProjectAction` — 文件对话框 + 反序列化 + `_maybeSaveProject()` 回调
- `SaveProjectAction` — 文件对话框 + 序列化
- `ImportImageAction` — 文件对话框 + 异步导入
- `ExportImageAction` — DPI 对话框 + RIP 设置 + SceneToJson + 异步导出

使用 `std::function` 回调处理 `maybeSaveProject()`、`switchTheme()` 等深度绑定 MainWindow 的操作。

### Phase 7: MainWindow 清理

**修改 `Src/UI/mainwindow.h`：**
- 删除 ~30 个已迁移的 slot 声明
- 删除已迁移的 helper 方法（`copyItemsToClipboard`, `rotateSelectedItems`, `applyAlign` 等）
- 新增 `QMap<QString, QAtActionBasePtr> m_actions`
- 新增 `ActionContext *m_actionContext`（MainWindow 拥有，生命周期与 MainWindow 一致）
- 新增 `void _initActions()`
- 保留：`onSelectionChanged()`, `onItemAdded()`, `onPenChanged()` 等属性面板信号处理（它们不是 Action）

**修改 `Src/UI/mainwindow.cpp`：**
- `_initActions()` 构造 ActionContext、注册默认画布、并通过工厂创建所有 Action：

```cpp
void MainWindow::_initActions()
{
    // 构造 context
    m_actionContext = new ActionContext(this);
    m_actionContext->setParentWidget(this);
    m_actionContext->setPropertyPanel(m_pPropertyPanel);
    // ... 设置其他服务 ...

    // 注册默认画布（单画布场景）
    m_actionContext->registerCanvas("main", {
        m_pView, m_pView->scene(), m_undoStack
    });
    m_actionContext->setActiveCanvas("main");

    // 设置回调
    m_actionContext->setMaybeSaveProject([this]() { return _maybeSaveProject(); });
    m_actionContext->setSwitchTheme([this](const QString &t) { switchTheme(t); });
    // ...

    // 通过工厂创建所有 Action
    QStringList tokens = { "New", "OpenProject", "SaveProject", ... };
    for (const auto& token : tokens) {
        auto action = QAtActionFactory::get().createAction(token);
        if (action) {
            action->setContext(m_actionContext);
            m_actions[token] = action;
        }
    }
}
```
- `_initMenuBar()` / `_initToolBar()` 改为声明式布局，从 `m_actions` 获取 QAction
- 删除所有已迁移 slot 的实现

**修改 `Src/Src.pro`：**
- 添加所有新文件到 HEADERS / SOURCES

---

## 关键设计决策

1. **QAtActionBase 保持非 QObject** — 实例轻量，无 moc 依赖，工厂模式自然运作。`createQAction()` 桥接 Qt widget 系统。
2. **ActionContext 是 QObject 类** — 需要发射 `canvasSwitched` 等信号通知 Action 画布变更，同时提供画布注册/切换的完整 API。
3. **属性面板信号处理留在 MainWindow** — `onPenChanged()` 等是响应式的，不是用户触发的 Action，不属于框架范畴。
4. **File action 使用 `std::function` 回调** — `maybeSaveProject()` 等深度绑定 MainWindow 状态，通过回调委托避免 Action 依赖 MainWindow 类。
5. **每个分类 Action 放在独立文件中** — 按菜单/工具栏分组，每个文件保持 200 行以内。

---

## 新增文件清单

| 文件 | 用途 |
|------|------|
| `Src/Action/ActionContext.h/.cpp` | 画布感知的依赖注入类 |
| `Src/Action/QAtFileAction.h/.cpp` | 文件操作基类 |
| `Src/Action/QAtEditAction.h/.cpp` | 编辑操作基类 |
| `Src/Action/QAtArrangeAction.h/.cpp` | 排列操作基类 |
| `Src/Action/QAtViewAction.h/.cpp` | 视图操作基类 |
| `Src/Action/QAtDialogAction.h/.cpp` | 对话框操作基类 |
| `Src/Action/FileActions.h/.cpp` | 具体文件 Action |
| `Src/Action/EditActions.h/.cpp` | 具体编辑 Action |
| `Src/Action/ToolActions.h/.cpp` | 具体工具 Action（或放在 QAtDrawAction 中） |
| `Src/Action/ArrangeActions.h/.cpp` | 具体排列 Action |
| `Src/Action/ViewActions.h/.cpp` | 具体视图 Action |
| `Src/Action/DialogActions.h/.cpp` | 具体对话框 Action |
| `Src/Utils/ClipboardUtils.h/.cpp` | 剪贴板序列化工具（可选） |

## 风险评估

| 阶段 | 风险 | 回退策略 |
|------|------|---------|
| Phase 1-2 | 极低 | 纯新增代码，无运行时影响 |
| Phase 3 | 低 | 旧代码并行保留，可随时切回 |
| Phase 4 | 中 | 剪贴板逻辑提取需仔细测试 |
| Phase 5 | 中 | 复杂 undo 宏需验证 |
| Phase 6 | 高 | 异步操作 + 项目状态管理最脆弱 |
| Phase 7 | 中 | 仅在全部验证后才删除旧代码 |

**关键原则：** 每个阶段迁移时保留旧代码并行运行，验证通过后再删除。各阶段间存在临时代码重复。

---

## 多画布扩展场景

当前为单画布（`"main"`），ActionContext 已为多画布做好准备：

```
当前：MainWindow → 1 个 QAtGraphicsView → 1 个 scene + 1 个 undoStack
未来：MainWindow → TabWidget → N 个 QAtGraphicsView（每个 tab 一个画布）
```

多画布时的操作流程：
1. 用户切换 tab → 调用 `m_actionContext->setActiveCanvas(tabId)` → 发射 `canvasSwitched` 信号
2. Drawing tool action 监听 `canvasSwitched`，将工具状态同步到新画布
3. Edit/Arrange action 通过 `context()->activeView()` 自动操作当前画布
4. File action（New/Open/Save）在指定画布上操作，或创建新画布并注册

---

## 验证方案

1. **编译验证：** 每个 Phase 完成后 qmake + make 通过
2. **功能验证：** 每个 Phase 完成后手动测试对应功能
   - Phase 3: 绘图工具切换、快捷键、工具栏状态
   - Phase 4: Undo/Redo、剪贴板操作、Delete
   - Phase 5: 成组/解组、排列、旋转、自动排版
   - Phase 6: 新建/打开/保存项目、导入/导出图片
3. **回归验证：** Phase 7 完成后全功能回归测试
