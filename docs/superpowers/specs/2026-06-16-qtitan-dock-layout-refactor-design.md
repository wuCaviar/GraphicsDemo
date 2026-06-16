# Qtitan Dock Layout Refactor

## Problem

当前 App 使用 `QMainWindow` + 标准 `QDockWidget` 管理画布和面板。"混合 Auto-hide" 布局（方案 C）需要高级停靠能力——面板 Auto-hide/Pin、画布 Tab 管理、面板状态持久化——标准 QDockWidget 无法原生支持。与此同时，`Libs/Qtitan` 的 Docking 模块已集成但未使用，提供 `DockMainWindow`、`DockPanelManager`、`DockDocumentPanel`、`DockWidgetPanel`、`DockToolBar` 等完整停靠系统。

## Design Decisions

- **只使用 Qtitan Docking**，不使用 Qtitan Style 系统（保留现有 QSS light/dark 主题切换）
- **方案 C：混合 Auto-hide 模式** — 画布为中央 DockDocumentPanel（多 Tab），外围面板默认展开可见且支持 Auto-hide
- **ToolBar 保持 ToolBar** — 使用 `DockToolBar` 替代 `QToolBar`，不改为 Dock 面板
- **一次性迁移** — `MainWindow` 直接从 `QMainWindow` 切换到 `DockMainWindow`
- **面板默认展开** — 右侧 PropertyPanel 和 AlignPanel 启动时可见，用户手动 Pin 切换 Auto-hide

## Architecture

```
MainWindow : Qtitan::DockMainWindow
│
├── MenuBar (setMenuBar, 不变)
├── StatusBar (setStatusBar, 不变)
│
├── DockBarManager
│   ├── DockToolBar "File & Edit"    ← Top
│   ├── DockToolBar "Drawing Tools"  ← Left
│   └── DockToolBar "Align"          ← Top
│
├── DockPanelManager
│   ├── DockDocumentPanel[] (中央 Tab 区)
│   │   └── QAtCanvasPage (画布)
│   ├── DockWidgetPanel "Properties" ← Right, 默认展开
│   │   └── PropertyPanel
│   └── DockWidgetPanel "Align"      ← Right, 默认展开
│       └── AlignWidget
│
└── 无 CentralWidget (DockDocumentPanel 自动填充中央)
```

## Component Mapping

### MainWindow 继承链

```
QMainWindow → Qtitan::DockMainWindow
```

`DockMainWindow` 是 `QWidget` 子类（非 `QMainWindow` 子类），通过 setter 注入 MenuBar/StatusBar。

### ToolBar

| QMainWindow API | Qtitan 等价 |
|----------------|-------------|
| `QToolBar` | `Qtitan::DockToolBar` |
| `addToolBar(Qt::TopToolBarArea, bar)` | `dockBarManager()->addToolBar(name, DockBarArea_Top)` |
| `addAction(QAction*)` | `addAction(QAction*)` (兼容) |
| `addSeparator()` | `addSeparator()` (兼容) |
| `setIconSize(QSize)` | `setIconSize(QSize)` (兼容) |

### Canvas

| 当前 | Qtitan |
|------|--------|
| `QDockWidget[]` (m_canvasDocks) | `DockDocumentPanel[]` |
| `_addCanvasDockInternal()` | `dockPanelManager()->addDocumentPanel(title)` |
| Tab 切换信号 (自定义) | `DockPanelManager::dockPanelActivated(DockWidgetPanel*)` |

### 属性/对齐面板

| 当前 | Qtitan |
|------|--------|
| `QDockWidget` | `Qtitan::DockWidgetPanel` |
| `addDockWidget(Qt::RightDockWidgetArea, dw)` | `dockPanelManager()->addDockPanel(title, DockPanelArea_Right)` |
| `setWidget(w)` | `dockWidgetPanel->setWidget(w)` |
| `setAllowedAreas()` | `setAllowedAreas()` (兼容) |
| `setFeatures()` | `setFeatures()` (兼容) |
| `hide()` / `show()` | `closePanel()` / `showPanel()` |
| (无对应) | `setAutoHide(bool)` |

### 面板行为规格

| 面板 | 默认位置 | 默认状态 | Features | AllowedAreas |
|------|---------|---------|----------|-------------|
| Properties | Right | 展开可见 | Closable \| Hideable \| Floatable | Left \| Right |
| Align | Right | 展开可见 | Closable \| Hideable \| Floatable | Left \| Right |

两个面板可独立 Tab 组合或分离，仅允许停靠左侧或右侧。View 菜单通过 `visibleAction()` 切换可见性。

## Signal Flow

### 画布切换

```
用户点击 Tab / 新建画布
  → DockPanelManager::dockPanelActivated(DockWidgetPanel*)
  → MainWindow::_onDocumentPanelActivated()
    ├── 从 DockDocumentPanel 中取 QAtCanvasPage
    ├── 更新 m_pView / m_undoStack 指针
    ├── _bindViewConnections() 重新绑定
    ├── m_pPropertyPanel->setItem(currentItem)
    └── StatusBarDirector::onPageSwitched()
```

### View 菜单面板切换

```
View → Properties checkbox  → dockWidgetPanel->showPanel() / closePanel()
View → Align checkbox       → dockWidgetPanel->showPanel() / closePanel()
```

### 状态持久化

```cpp
// 替换 loadWindowState/saveWindowState
dockBarManager()->saveStateToFile(barStatePath);
dockPanelManager()->saveStateToFile(panelStatePath);
dockBarManager()->loadStateFromFile(barStatePath);
dockPanelManager()->loadStateFromFile(panelStatePath);
```

## Changes Summary

### mainwindow.h

- `QMainWindow` → `Qtitan::DockMainWindow`
- 移除: `m_canvasDocks` (`QList<QDockWidget *>`)、`m_activeCanvasDock`
- 移除: `_addCanvasDockInternal`、`_removeCanvasDockInternal`、`_onCanvasDockActivated`、`_canvasPageAt`、`_canvasCount`、`_currentCanvasIndex`、`_indexOfCanvasPage`、`_setCurrentCanvasPage`、`_setCurrentCanvasIndex`、`_addCanvasDock`、`_updateCanvasDockTitle` 等旧 Canvas Dock 管理 API
- 新增: `_onDocumentPanelActivated(DockDocumentPanel *panel)` — 画布 Tab 切换槽
- 新增: DockPanelManager / DockBarManager 访问便捷方法
- ToolBar 成员变量类型: `QToolBar *` → `Qtitan::DockToolBar *`

### mainwindow.cpp

- `_initPages()`: 移除 QDockWidget tab 逻辑，改用 `addDocumentPanel(title)` 创建画布
- `_initPropertyPanel()`: `QDockWidget` → `DockWidgetPanel`，`addDockWidget()` → `addDockPanel()`
- `_initToolBar()`: `QToolBar` → `DockToolBar`，`addToolBar()` → `dockBarManager()->addToolBar()`
- `_initMenuBar()`: View 菜单 checkbox 绑定 `DockWidgetPanel::visibleAction()`
- 移除所有 `m_canvasDocks` 遍历逻辑，改用 `dockPanelManager()->documentPanelList()`
- `loadWindowState` / `saveWindowState`: 替换为 Qtitan 序列化 API
- `eventFilter`: 移除 Canvas Dock 激活检测（由 `dockPanelActivated` 信号替代）

### 新增 include

```cpp
#include "QtitanDocking.h"
using namespace Qtitan;
```

### 不变的部分

- `PropertyPanel` / `AlignWidget` 内部实现完全不变（只是外层容器从 `QDockWidget` 变为 `DockWidgetPanel`）
- `QAtCanvasPage` / `QAtGraphicsView` 不变
- `StatusBarDirector` / `ToolBarDirector` 接口不变
- `AppContext` / 各 Service 不变
- QSS 主题系统不变

## Edge Cases

| 场景 | 处理 |
|------|------|
| 无画布启动 | 中央无 DocumentPanel，PropertyPanel/AlignPanel 禁用 |
| 最后画布关闭 | DocumentPanel 销毁，面板禁用，ToolBar 仅保留 New/Import/Export |
| 新建画布（无其他画布时） | 创建首个 DocumentPanel，面板重新激活 |
| 面板被关闭后重新打开 | View 菜单 → `showPanel()`，恢复到原停靠位置 |
| 窗口最小尺寸 | 不低于 1024×700 |
| 主题切换 | QSS 切换不变，Dock 组件无样式冲突 |
| 状态加载失败 | 回退默认布局（右侧展开面板 + 无画布） |
| Ruler 同步 | Ruler 属于 QAtCanvasPage 内部，不受影响 |

## Adaptation Considerations

**DockMainWindow 非 QMainWindow 子类**。需要适配：
- `addDockWidget()` → 不再可用（改用 `DockPanelManager` API）
- `addToolBar()` → `dockBarManager()->addToolBar()`
- `setCentralWidget()` → `dockPanelManager()->setCentralWidget()` 或 `dockBarManager()->setCentralWidget()`
- `tabifyDockWidget()` → DockPanelManager 自动处理 Tab 组合（拖拽面板到同一区域即可）
- `setTabPosition()` / `setDockNestingEnabled()` → DockPanelManager 内部控制

**DockToolBar 非 QToolBar 子类**。需要适配：
- `addAction(QAction*)` / `addSeparator()` / `setIconSize()` 接口兼容
- `setMovable()` → DockToolBar 始终可拖拽
- `setObjectName()` → 保持不变
- `toggleViewAction()` → 无直接等价，需手动创建或使用 DockBarManager API

## Verification

- [ ] App 启动，DockMainWindow 正常显示
- [ ] 新建画布 → DockDocumentPanel Tab 出现在中央区域
- [ ] 多个画布 Tab 切换正常，PropertyPanel 和 AlignPanel 同步更新
- [ ] PropertyPanel 和 AlignPanel 默认在右侧展开可见
- [ ] 面板可拖拽到左侧/右侧，不可拖拽到 Top/Bottom
- [ ] 面板 Pin 按钮切换到 Auto-hide 状态（悬停展开）
- [ ] View 菜单可切换面板可见性
- [ ] 关闭所有画布后面板自动禁用
- [ ] 工具栏正常工作（File & Edit、Drawing Tools、Align）
- [ ] 工具栏可拖拽重组位置
- [ ] 状态栏缩放控件正常
- [ ] 标尺与画布同步正常
- [ ] 主题切换（light/dark）正常
- [ ] 窗口状态保存/恢复正常（含面板位置、Auto-hide 状态）
- [ ] 最小窗口尺寸 ≥ 1024×700
- [ ] Undo/Redo 与 Tab 切换联动正常
- [ ] 无 Qtitan Style 泄露（外观仍由 QSS 控制）
