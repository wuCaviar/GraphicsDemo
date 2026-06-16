# Qtitan Dock Layout Refactor — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Migrate MainWindow from QMainWindow+QDockWidget to Qtitan::DockMainWindow with DockDocumentPanel (canvas tabs) and DockWidgetPanel (properties/align) while keeping existing QSS themes.

**Architecture:** One-time class hierarchy switch: `QMainWindow` → `Qtitan::DockMainWindow`. QToolBar → DockToolBar (95% API-compatible). QDockWidget canvas tabs → DockDocumentPanel. QDockWidget side panels → DockWidgetPanel (default expanded, auto-hide capable via DockPanelHideable feature). QSS themes untouched.

**Tech Stack:** Qt 6.11, C++17, Qtitan 4.0 Docking (static lib), qmake

---

## File Structure

| File | Action | Responsibility |
|------|--------|---------------|
| `Libs/Libs.pro` | Modify | Enable Qtitan subdir in build |
| `Src/Src.pro` | Modify | Link QtitanDocking library |
| `Src/UI/mainwindow.h` | Major modify | QMainWindow → DockMainWindow, replace canvas dock API |
| `Src/UI/mainwindow.cpp` | Major modify | All _init*, canvas mgmt, state persistence |
| `Src/UI/ToolBarDirector.h` | Modify | QToolBar* → DockToolBar* |
| `Src/UI/ToolBarDirector.cpp` | Modify | QToolBar → DockToolBar types |
| `Src/UI/StatusBarDirector.h` | No change | Interface unchanged |
| `Src/UI/StatusBarDirector.cpp` | No change | Interface unchanged |
| `Src/UI/PropertyPanel.h` | No change | Inner widget unchanged (only outer container changes) |
| `Src/UI/AlignWidget.h` | No change | Inner widget unchanged |
| `Src/Core/AppContext.h` | No change | Unchanged |
| `Src/Core/AppContext.cpp` | No change | Unchanged |

---

### Task 1: Enable QtitanDocking in the build system

**Files:**
- Modify: `Libs/Libs.pro:9-12`
- Modify: `Src/Src.pro:20-21`

- [ ] **Step 1: Uncomment Qtitan in Libs.pro**

Read `Libs/Libs.pro`. Find the commented-out `# Qtitan` in SUBDIRS (line 9) and the `# Qtitan.subdir = Qtitan` line (line 16). Uncomment both.

```qmake
# In Libs/Libs.pro, change:
#     Qtitan \
# to:
    Qtitan \

# And change:
# Qtitan.subdir = Qtitan
# to:
Qtitan.subdir = Qtitan
```

- [ ] **Step 2: Uncomment QtitanDocking link in Src.pro**

Read `Src/Src.pro`. Find the commented-out `# LIBS += -L$$DESTDIR -lQtitanDocking` (near line 20). Uncomment it.

```qmake
# In Src/Src.pro, change:
# LIBS += -L$$DESTDIR -lQtitanDocking
# to:
LIBS += -L$$DESTDIR -lQtitanDocking
```

The `INCLUDEPATH += $$PWD/../Libs/Qtitan/include` is already present — no change needed there.

- [ ] **Step 3: Build to verify linking works**

Run: `cd /Volumes/Caviar/Test/GraphicsDemo && qmake && make -j$(sysctl -n hw.ncpu) 2>&1 | tail -20`

Expected: Build completes without errors (MainWindow still compiles as QMainWindow — this task only enables the library, doesn't use it yet).

- [ ] **Step 4: Commit**

```bash
git add Libs/Libs.pro Src/Src.pro
git commit -m "build: enable QtitanDocking library in build system

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 2: Switch mainwindow.h to Qtitan::DockMainWindow

**Files:**
- Modify: `Src/UI/mainwindow.h`

- [ ] **Step 1: Replace include and base class**

In `mainwindow.h`:
- Add `#include "QtitanDocking.h"` after the existing Qt includes (before the `class QLabel;` forward declarations)
- Change `class MainWindow : public QMainWindow` to `class MainWindow : public Qtitan::DockMainWindow`

```cpp
// At top of mainwindow.h, add after existing includes:
#include "QtitanDocking.h"

// Change line 47:
// from: class MainWindow : public QMainWindow
// to:
class MainWindow : public Qtitan::DockMainWindow
```

- [ ] **Step 2: Replace QDockWidget canvas tracking members**

Remove:
```cpp
// Canvas dock management (replaces QTabWidget — each canvas is a QDockWidget)
QList<QDockWidget *> m_canvasDocks; // all canvas dock widgets in insertion order
QDockWidget *m_activeCanvasDock = nullptr; // currently active/focused canvas dock
```

Add:
```cpp
// Canvas document panels (Qtitan DockDocumentPanel replaces QDockWidget for canvases)
Qtitan::DockDocumentPanel *m_activeDocumentPanel = nullptr;
```

- [ ] **Step 3: Remove canvas dock helper method declarations**

Remove these method declarations from the header:
- `QAtCanvasPage *_currentCanvasPage() const;`
- `QAtCanvasPage *_canvasPageAt(int index) const;`
- `int _canvasCount() const;`
- `int _currentCanvasIndex() const;`
- `int _indexOfCanvasPage(QAtCanvasPage *page) const;`
- `void _setCurrentCanvasPage(QAtCanvasPage *page);`
- `void _setCurrentCanvasIndex(int index);`
- `QDockWidget *_addCanvasDockInternal(QAtCanvasPage *page, const QString &title);`
- `void _removeCanvasDockInternal(int index);`
- `void _onCanvasDockActivated(QDockWidget *dock);`
- `void _updateCanvasDockTitle(QAtCanvasPage *page);`

Add these new method declarations:
```cpp
// Qtitan Dock canvas management
QAtCanvasPage *_currentCanvasPage() const;
int _canvasCount() const;
void _addCanvasPage(const QString &title, const QString &pageId);
void _removeCanvasPage(Qtitan::DockDocumentPanel *docPanel);
void _onDocumentPanelActivated(Qtitan::DockDocumentPanel *panel);
void _updateDocumentPanelTitle(QAtCanvasPage *page);
```

- [ ] **Step 4: Replace PropertyPanel/AlignWidget member declarations**

The existing `m_pPropertyPanel` and `m_alignLayoutDlg` were originally `PropertyPanel *` and `AlignWidget *` (subclassing `QDockWidget`). Now they are wrapped inside `DockWidgetPanel`. Add wrapper members:

```cpp
// DockWidgetPanel wrappers (contain PropertyPanel and AlignWidget as inner widgets)
Qtitan::DockWidgetPanel *m_propsDockPanel = nullptr;
Qtitan::DockWidgetPanel *m_alignDockPanel = nullptr;
```

The inner `m_pPropertyPanel` and `m_alignLayoutDlg` remain (they become child widgets of the DockWidgetPanels).

- [ ] **Step 5: Remove `bool eventFilter` override declaration**

Remove: `bool eventFilter(QObject *obj, QEvent *event) override;`
Qtitan DockPanelManager handles dock activation/focus internally.

- [ ] **Step 6: Remove old toolbar findChild declarations**

No member variables of type `QToolBar *` exist in the header (the toolbars were created locally in `_initToolBar()` and found via `findChild`). No change needed here.

- [ ] **Step 7: Build to verify header compiles**

Run: `cd /Volumes/Caviar/Test/GraphicsDemo && make -j$(sysctl -n hw.ncpu) 2>&1 | tail -30`

Expected: Compile errors (mainwindow.cpp hasn't been updated yet — but the header should be syntactically valid). The errors should be in mainwindow.cpp, not mainwindow.h.

- [ ] **Step 8: Commit**

```bash
git add Src/UI/mainwindow.h
git commit -m "refactor: switch MainWindow base class to Qtitan::DockMainWindow

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 3: Rewrite _initPages() — Canvas as DockDocumentPanel

**Files:**
- Modify: `Src/UI/mainwindow.cpp` (specifically `_initPages()`)

- [ ] **Step 1: Rewrite `_initPages()`**

Replace the entire `_initPages()` method (currently at line 1022) and the old helper methods below it (lines 1132-1386) with the new implementation.

First, replace `_initPages()`:

```cpp
void MainWindow::_initPages()
{
    // Canvas pages are now Qtitan DockDocumentPanels — each gets its own
    // QAtCanvasPage widget embedded via setWidget().
    // No canvases created on startup — user creates via File -> New.
    // The DockPanelManager automatically fills the central area with document panels.
}
```

- [ ] **Step 2: Replace helper methods with Qtitan equivalents**

Remove all old helper methods from `_currentCanvasPage()` through `eventFilter()` (lines 1132-1386).

Replace with new implementations:

```cpp
QAtCanvasPage *MainWindow::_currentCanvasPage() const
{
    if (!m_activeDocumentPanel)
        return nullptr;
    return qobject_cast<QAtCanvasPage *>(m_activeDocumentPanel->widget());
}

int MainWindow::_canvasCount() const
{
    return dockPanelManager()->documentPanelList().size();
}

void MainWindow::_addCanvasPage(const QString &title, const QString &pageId)
{
    static constexpr qreal kDefaultPpi = 150.0;
    qreal mmToPx = AtMath::Units::DPIContext(kDefaultPpi).mmToPx(1.0);

    NewFileDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    QSizeF sizeMM = dlg.selectedSizeMM();
    QSizeF canvasSize(std::ceil(sizeMM.width() * mmToPx), std::ceil(sizeMM.height() * mmToPx));

    auto *canvasPage = new QAtCanvasPage(pageId, canvasSize, kDefaultPpi, this);
    AppContext::get().registerPage(canvasPage);

    auto *docPanel = dockPanelManager()->addDocumentPanel(title);
    docPanel->setWidget(canvasPage);
    // DocPanel acts as the identifier for this canvas page
    docPanel->setProperty("pageId", pageId);

    // Set as active
    m_activeDocumentPanel = docPanel;
    m_tabStates[canvasPage].modified = false;

    AppContext::get().setActivePage(pageId);
    QAtGraphicsView *newView = canvasPage->view();
    QUndoStack *newStack = canvasPage->undoStack();
    m_pView = newView;
    m_undoStack = newStack;
    if (m_undoStack)
        AppContext::get().undo()->bindUndoStack(m_undoStack);
    _bindViewConnections();

    m_pPropertyPanel->setItem(nullptr);
    m_resizeCanvasBtn->setVisible(true);
    m_pPropertyPanel->setDisplayPpi(kDefaultPpi);

    m_pProgressMgr->resetAll();

    // Sync labels
    _updateCanvasLabel();
    _updatePosLabel(m_lastScenePos);
    AppContext::get().refreshAllActions();
    m_statusBarDirector->onPageSwitched(pageId, QStringLiteral("canvas"));

    // Enable side panels now that we have a canvas
    if (m_propsDockPanel)
        m_propsDockPanel->showPanel();
    if (m_alignDockPanel)
        m_alignDockPanel->closePanel(); // default hidden; user opens via View or align action

    m_projectModified = true;
}

void MainWindow::_removeCanvasPage(Qtitan::DockDocumentPanel *docPanel)
{
    if (!docPanel)
        return;
    QAtCanvasPage *page = qobject_cast<QAtCanvasPage *>(docPanel->widget());
    if (page) {
        page->disconnect(this);
        if (m_statusBarDirector)
            page->disconnect(m_statusBarDirector);
        AppContext::get().unregisterPage(page->pageId());
        m_tabStates.remove(page);
    }

    // If removing active panel, pick new active
    auto docList = dockPanelManager()->documentPanelList();
    if (m_activeDocumentPanel == docPanel) {
        int idx = docList.indexOf(docPanel);
        if (docList.size() > 1) {
            int newIdx = (idx > 0) ? idx - 1 : 1;
            // The panel manager will fire dockPanelActivated for the next active panel
        }
        m_activeDocumentPanel = nullptr;
    }

    // Last canvas removed — reset all state
    if (docList.size() <= 1) {
        m_pView = nullptr;
        m_undoStack = nullptr;
        m_projectPath.clear();
        m_projectModified = false;
        // Disable side panels
        if (m_propsDockPanel)
            m_propsDockPanel->closePanel();
        if (m_alignDockPanel)
            m_alignDockPanel->closePanel();
    }

    dockPanelManager()->removeDockPanel(docPanel);
    // docPanel is managed by Qtitan, don't deleteLater manually
}

void MainWindow::_onDocumentPanelActivated(Qtitan::DockDocumentPanel *panel)
{
    if (!panel || m_activeDocumentPanel == panel)
        return;

    QAtCanvasPage *page = qobject_cast<QAtCanvasPage *>(panel->widget());
    if (!page)
        return;

    _updateDocumentPanelTitle(page);

    Qtitan::DockDocumentPanel *oldPanel = m_activeDocumentPanel;
    m_activeDocumentPanel = panel;

    AppContext::get().setActivePage(page->pageId());
    QAtGraphicsView *newView = page->view();
    QUndoStack *newStack = page->undoStack();

    if (m_pView != newView) {
        if (m_pView) {
            m_pView->disconnect(this);
            if (m_undoStack)
                m_undoStack->disconnect(this);
        }
        m_pView = newView;
        m_undoStack = newStack;
        if (m_undoStack)
            AppContext::get().undo()->bindUndoStack(m_undoStack);
        _bindViewConnections();

        if (m_alignLayoutDlg && newView) {
            m_alignLayoutDlg->setScene(newView->scene());
            m_alignLayoutDlg->setUndoStack(newStack);
        }

        if (m_pView && m_pView->currentTool() != m_currentTool)
            m_pView->setTool(m_currentTool);

        _updateCanvasLabel();
        _updatePosLabel(m_lastScenePos);

        AppContext::get().refreshAllActions();

        if (m_pView && m_pView->canvasItem()) {
            qreal ppi = m_pView->canvasItem()->ppi();
            if (m_pPropertyPanel)
                m_pPropertyPanel->setDisplayPpi(ppi);
        }
    }

    // Update old panel title too
    if (oldPanel) {
        auto *oldPage = qobject_cast<QAtCanvasPage *>(oldPanel->widget());
        if (oldPage)
            _updateDocumentPanelTitle(oldPage);
    }

    // Restore property panel from newly active canvas's selection
    if (m_pView && m_pPropertyPanel) {
        auto items = ::filterSelectableItems(m_pView->scene()->selectedItems());
        m_pPropertyPanel->setItem(items.size() == 1 ? items.first() : nullptr);
    }
}

void MainWindow::_updateDocumentPanelTitle(QAtCanvasPage *page)
{
    if (!page)
        return;
    QString title;
    if (!m_projectPath.isEmpty()) {
        if (_canvasCount() == 1)
            title = QFileInfo(m_projectPath).completeBaseName();
        else {
            auto docList = dockPanelManager()->documentPanelList();
            for (int i = 0; i < docList.size(); ++i) {
                if (qobject_cast<QAtCanvasPage *>(docList[i]->widget()) == page) {
                    title = tr("%1 — Canvas %2")
                                .arg(QFileInfo(m_projectPath).completeBaseName())
                                .arg(i + 1);
                    break;
                }
            }
        }
    } else {
        title = page->title();
    }
    auto &state = m_tabStates[page];
    if (state.modified && !title.endsWith(QStringLiteral(" *")))
        title += QStringLiteral(" *");
    // Update the DockDocumentPanel caption
    auto docList = dockPanelManager()->documentPanelList();
    for (auto *dp : docList) {
        if (qobject_cast<QAtCanvasPage *>(dp->widget()) == page) {
            dp->setCaption(title);
            break;
        }
    }
}
```

- [ ] **Step 3: Build to verify new methods compile**

Run: `cd /Volumes/Caviar/Test/GraphicsDemo && make -j$(sysctl -n hw.ncpu) 2>&1 | tail -30`

Expected: Compile errors from remaining code still referencing old QDockWidget APIs (these will be fixed in subsequent tasks).

- [ ] **Step 4: Commit**

```bash
git add Src/UI/mainwindow.cpp
git commit -m "refactor: replace canvas QDockWidget management with DockDocumentPanel

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 4: Rewrite _initPropertyPanel() — Side panels as DockWidgetPanel

**Files:**
- Modify: `Src/UI/mainwindow.cpp` (`_initPropertyPanel()`)

- [ ] **Step 1: Rewrite `_initPropertyPanel()`**

Replace the entire `_initPropertyPanel()` method (lines 718-733):

```cpp
void MainWindow::_initPropertyPanel()
{
    // Property Panel — wrapped in a Qtitan DockWidgetPanel
    m_pPropertyPanel = new PropertyPanel(this);
    m_pPropertyPanel->setObjectName("PropertyPanel");

    m_propsDockPanel = dockPanelManager()->addDockPanel(
        tr("Properties"),
        QSize(300, -1),
        Qtitan::RightDockPanelArea);
    m_propsDockPanel->setObjectName("PropertiesPanel");
    m_propsDockPanel->setWidget(m_pPropertyPanel);
    m_propsDockPanel->setFeatures(Qtitan::DockWidgetPanel::DockPanelClosable
                                  | Qtitan::DockWidgetPanel::DockPanelHideable
                                  | Qtitan::DockWidgetPanel::DockPanelFloatable);
    m_propsDockPanel->setAllowedAreas(Qtitan::LeftDockPanelArea | Qtitan::RightDockPanelArea);

    // Align Widget — wrapped in another DockWidgetPanel
    m_alignLayoutDlg = new AlignWidget(nullptr, nullptr, this);
    m_alignLayoutDlg->setObjectName("AlignLayoutDock");

    m_alignDockPanel = dockPanelManager()->addDockPanel(
        tr("Align"),
        QSize(300, -1),
        Qtitan::RightDockPanelArea,
        m_propsDockPanel); // tab together with Properties by default
    m_alignDockPanel->setObjectName("AlignPanel");
    m_alignDockPanel->setWidget(m_alignLayoutDlg);
    m_alignDockPanel->setFeatures(Qtitan::DockWidgetPanel::DockPanelClosable
                                  | Qtitan::DockWidgetPanel::DockPanelHideable
                                  | Qtitan::DockWidgetPanel::DockPanelFloatable);
    m_alignDockPanel->setAllowedAreas(Qtitan::LeftDockPanelArea | Qtitan::RightDockPanelArea);
    m_alignDockPanel->closePanel(); // hidden by default

    AppContext::get().setAlignWidget(m_alignLayoutDlg);
}
```

Note: `m_pPropertyPanel->setMinimumWidth(300)` and `m_pPropertyPanel->setAllowedAreas()` are removed — the DockWidgetPanel controls sizing and allowed areas. `m_alignLayoutDlg->hide()` is replaced with `m_alignDockPanel->closePanel()`.

- [ ] **Step 2: Build**

Run: `cd /Volumes/Caviar/Test/GraphicsDemo && make -j$(sysctl -n hw.ncpu) 2>&1 | tail -30`

Expected: Errors decreasing — remaining issues are old QToolBar and eventFilter/canvas dock references.

- [ ] **Step 3: Commit**

```bash
git add Src/UI/mainwindow.cpp
git commit -m "refactor: replace QDockWidget side panels with DockWidgetPanel

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 5: Rewrite _initToolBar() — QToolBar to DockToolBar

**Files:**
- Modify: `Src/UI/mainwindow.cpp` (`_initToolBar()`)
- Modify: `Src/UI/mainwindow.cpp` (ToolBarDirector wiring in constructor)
- Modify: `Src/UI/ToolBarDirector.h`
- Modify: `Src/UI/ToolBarDirector.cpp`

- [ ] **Step 1: Rewrite `_initToolBar()`**

Replace the entire `_initToolBar()` method (lines 596-716):

```cpp
void MainWindow::_initToolBar()
{
    // File & Edit toolbar — top
    auto *fileEditBar = dockBarManager()->addToolBar(tr("File & Edit"), Qtitan::DockBarTop);
    fileEditBar->setObjectName("FileEditToolBar");
    fileEditBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    fileEditBar->setIconSize(QSize(20, 20));

    QAction *newAct = new QAction(QIcon(":/icons/icons/file-new.svg"), tr("New"), this);
    newAct->setToolTip(tr("Create a new canvas"));
    connect(newAct, &QAction::triggered, this, &MainWindow::onNew);
    fileEditBar->addAction(newAct);

    QAction *importAct =
        new QAction(QIcon(":/icons/icons/file-import.svg"), tr("Import Image"), this);
    importAct->setToolTip(tr("Import an image onto the canvas"));
    connect(importAct, &QAction::triggered, this, &MainWindow::onImportImage);
    fileEditBar->addAction(importAct);

    QAction *exportAct =
        new QAction(QIcon(":/icons/icons/file-export.svg"), tr("Export Image"), this);
    exportAct->setToolTip(tr("Export the canvas to an image file"));
    connect(exportAct, &QAction::triggered, this, &MainWindow::onExportImage);
    fileEditBar->addAction(exportAct);

    fileEditBar->addSeparator();

    fileEditBar->addAction(AppContext::get().getQAction(QStringLiteral("Undo")));
    fileEditBar->addAction(AppContext::get().getQAction(QStringLiteral("Redo")));

    fileEditBar->addSeparator();

    fileEditBar->addAction(AppContext::get().getQAction(QStringLiteral("Cut")));
    fileEditBar->addAction(AppContext::get().getQAction(QStringLiteral("Copy")));
    fileEditBar->addAction(AppContext::get().getQAction(QStringLiteral("Paste")));

    // Drawing toolbar — left
    auto *drawBar = dockBarManager()->addToolBar(tr("Drawing Tools"), Qtitan::DockBarLeft);
    drawBar->setObjectName("DrawingToolBar");
    drawBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    drawBar->setIconSize(QSize(20, 20));

    auto *actionGroup = new QActionGroup(this);
    actionGroup->setExclusive(true);

    static const QStringList drawToolTokens = {
        QStringLiteral("SelectTool"),      QStringLiteral("RectTool"),
        QStringLiteral("EllipseTool"),     QStringLiteral("LineTool"),
        QStringLiteral("BezierCurveTool"), QStringLiteral("FreehandTool"),
        QStringLiteral("TextTool")
    };
    for (const auto &token : drawToolTokens) {
        QAction *act = AppContext::get().getQAction(token);
        if (act) {
            actionGroup->addAction(act);
            drawBar->addAction(act);
        }
    }

    // Align toolbar — top
    auto *alignToolBar = dockBarManager()->addToolBar(tr("Align"), Qtitan::DockBarTop);
    alignToolBar->setObjectName("AlignToolBar");
    alignToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    alignToolBar->setIconSize(QSize(20, 20));

    {
        QAction *act = AppContext::get().getQAction(QStringLiteral("AlignLayoutDialog"));
        if (act)
            alignToolBar->addAction(act);
    }

    alignToolBar->addSeparator();

    QAction *groupAct = AppContext::get().getQAction(QStringLiteral("Group"));
    if (groupAct)
        alignToolBar->addAction(groupAct);
    QAction *ungroupAct = AppContext::get().getQAction(QStringLiteral("Ungroup"));
    if (ungroupAct)
        alignToolBar->addAction(ungroupAct);

    alignToolBar->addSeparator();

    {
        QAction *act = AppContext::get().getQAction(QStringLiteral("RotateCW"));
        if (act)
            alignToolBar->addAction(act);
    }
    {
        QAction *act = AppContext::get().getQAction(QStringLiteral("RotateCCW"));
        if (act)
            alignToolBar->addAction(act);
    }

    alignToolBar->addSeparator();

    QAction *fitCanvasAct =
        alignToolBar->addAction(QIcon(":/icons/icons/view-fit.svg"), tr("Fit Canvas to Selection"));
    fitCanvasAct->setToolTip(tr("Resize the canvas to fit the selected items"));
    connect(fitCanvasAct, &QAction::triggered, this, &MainWindow::onFitCanvasToItems);

    alignToolBar->addSeparator();

    QAction *autoLayoutAct =
        alignToolBar->addAction(QIcon(":/icons/icons/auto-layout.svg"), tr("Auto Layout"));
    autoLayoutAct->setToolTip(tr("Auto arrange image items on the canvas"));
    connect(autoLayoutAct, &QAction::triggered, this, &MainWindow::onAutoLayout);
}
```

Key changes: `new QToolBar(...)` → `dockBarManager()->addToolBar(name, area)`, `addToolBar(Qt::TopToolBarArea, bar)` removed (DockBarManager takes care of placement), `setMovable()` removed (DockToolBar always movable), action/setIconSize/setToolButtonStyle calls unchanged.

- [ ] **Step 2: Update ToolBarDirector wiring in constructor**

In the constructor (around lines 174-190), update the `findChild` calls to use `Qtitan::DockToolBar *`:

```cpp
// P6: Wire ToolBarDirector for page-type-aware toolbar visibility
m_toolBarDirector = new ToolBarDirector(this);
{
    auto *fileEditBar = findChild<Qtitan::DockToolBar *>("FileEditToolBar");
    auto *drawBar = findChild<Qtitan::DockToolBar *>("DrawingToolBar");
    auto *alignBar = findChild<Qtitan::DockToolBar *>("AlignToolBar");
    if (fileEditBar)
        m_toolBarDirector->addToolBar(fileEditBar);
    if (drawBar)
        m_toolBarDirector->addToolBar(drawBar, { QStringLiteral("canvas") });
    if (alignBar)
        m_toolBarDirector->addToolBar(alignBar, { QStringLiteral("canvas") });
}
```

- [ ] **Step 3: Update ToolBarDirector.h**

Change all `QToolBar *` to `Qtitan::DockToolBar *`:

```cpp
// In ToolBarDirector.h:
// Change: #include <QToolBar>
// To:     #include "QtitanDocking.h"
//
// In the class declaration:
class ToolBarDirector : public QObject
{
    Q_OBJECT
public:
    explicit ToolBarDirector(QObject *parent = nullptr);

    void addToolBar(Qtitan::DockToolBar *bar, const QStringList &visiblePageTypes = {});
    void setToolBarVisibility(Qtitan::DockToolBar *bar, bool visible);

public slots:
    void onPageSwitched(const QString &pageId, const QString &pageType);
    void applyVisibility(const QString &pageType);

private:
    struct ToolBarEntry {
        Qtitan::DockToolBar *bar;
        QStringList pageTypes; // empty = always visible
    };
    QList<ToolBarEntry> m_entries;
};
```

- [ ] **Step 4: Update ToolBarDirector.cpp**

Replace `QToolBar` → `Qtitan::DockToolBar` throughout. The `setVisible()` call is unchanged (DockToolBar inherits from QWidget).

```cpp
// In ToolBarDirector.cpp, change:
// #include <QToolBar>
// to:
// #include "QtitanDocking.h"
//
// And replace all QToolBar * → Qtitan::DockToolBar *
```

- [ ] **Step 5: Build**

Run: `cd /Volumes/Caviar/Test/GraphicsDemo && make -j$(sysctl -n hw.ncpu) 2>&1 | tail -30`

Expected: Errors decreasing. Remaining issues may be eventFilter/loadWindowState/saveWindowState and scattered old canvas dock references.

- [ ] **Step 6: Commit**

```bash
git add Src/UI/mainwindow.cpp Src/UI/ToolBarDirector.h Src/UI/ToolBarDirector.cpp
git commit -m "refactor: replace QToolBar with Qtitan::DockToolBar

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 6: Rewrite state persistence — loadWindowState/saveWindowState

**Files:**
- Modify: `Src/UI/mainwindow.cpp` (`loadWindowState()`, `saveWindowState()`, `closeEvent()`)

- [ ] **Step 1: Rewrite `loadWindowState()`**

Replace `loadWindowState()` (lines 3299-3338):

```cpp
void MainWindow::loadWindowState()
{
    QSettings settings;

    // Restore window geometry
    if (settings.contains("window/geometry")) {
        restoreGeometry(settings.value("window/geometry").toByteArray());
    }

    // Restore Qtitan dock bar (toolbar) state
    QString barStatePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                           + "/dockBars.state";
    if (QFile::exists(barStatePath))
        dockBarManager()->loadStateFromFile(barStatePath);

    // Restore Qtitan dock panel (side panels) state
    QString panelStatePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                             + "/dockPanels.state";
    if (QFile::exists(panelStatePath))
        dockPanelManager()->loadStateFromFile(panelStatePath);

    // Restore toolbar visibility
    auto *fileEditBar = findChild<Qtitan::DockToolBar *>("FileEditToolBar");
    auto *drawBar = findChild<Qtitan::DockToolBar *>("DrawingToolBar");
    auto *alignToolBar = findChild<Qtitan::DockToolBar *>("AlignToolBar");

    if (fileEditBar && settings.contains("toolbar/FileEditToolBar_visible"))
        fileEditBar->setVisible(settings.value("toolbar/FileEditToolBar_visible").toBool());
    if (drawBar && settings.contains("toolbar/DrawingToolBar_visible"))
        drawBar->setVisible(settings.value("toolbar/DrawingToolBar_visible").toBool());
    if (alignToolBar && settings.contains("toolbar/AlignToolBar_visible"))
        alignToolBar->setVisible(settings.value("toolbar/AlignToolBar_visible").toBool());

    // Grid visibility (only if no session tabs were restored)
    if (_canvasCount() == 0 && settings.contains("view/gridVisible")) {
        bool gridVisible = settings.value("view/gridVisible").toBool();
        if (m_pView)
            m_pView->setGridVisible(gridVisible);
        AppContext::get().refreshAllActions();
    }
}
```

- [ ] **Step 2: Rewrite `saveWindowState()`**

Replace `saveWindowState()` (lines 3340-3366):

```cpp
void MainWindow::saveWindowState()
{
    QSettings settings;

    // Save window geometry
    settings.setValue("window/geometry", saveGeometry());

    // Save Qtitan dock bar (toolbar) state
    QString barStatePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                           + "/dockBars.state";
    QDir().mkpath(QFileInfo(barStatePath).absolutePath());
    dockBarManager()->saveStateToFile(barStatePath);

    // Save Qtitan dock panel state
    QString panelStatePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                             + "/dockPanels.state";
    dockPanelManager()->saveStateToFile(panelStatePath);

    // Save toolbar visibility
    auto *fileEditBar = findChild<Qtitan::DockToolBar *>("FileEditToolBar");
    auto *drawBar = findChild<Qtitan::DockToolBar *>("DrawingToolBar");
    auto *alignToolBar = findChild<Qtitan::DockToolBar *>("AlignToolBar");

    if (fileEditBar)
        settings.setValue("toolbar/FileEditToolBar_visible", fileEditBar->isVisible());
    if (drawBar)
        settings.setValue("toolbar/DrawingToolBar_visible", drawBar->isVisible());
    if (alignToolBar)
        settings.setValue("toolbar/AlignToolBar_visible", alignToolBar->isVisible());
}
```

- [ ] **Step 3: Update `closeEvent()`**

In `closeEvent()`, replace the old `saveWindowState()` call (line 3392) — the new implementation is already compatible since it uses the same function name. No code change needed here; the function signature is the same.

- [ ] **Step 4: Add `#include <QStandardPaths>` and `#include <QDir>` to includes if not already present**

Check if QStandardPaths and QDir are included. If not, add them.

```cpp
#include <QStandardPaths>  // Already likely present for other uses
```

- [ ] **Step 5: Build**

Run: `cd /Volumes/Caviar/Test/GraphicsDemo && make -j$(sysctl -n hw.ncpu) 2>&1 | tail -30`

Expected: Errors decreasing.

- [ ] **Step 6: Commit**

```bash
git add Src/UI/mainwindow.cpp
git commit -m "refactor: replace QMainWindow state persistence with Qtitan serialization

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 7: Fix all remaining references — onNew, onNewProject, loadSession, eventFilter, etc.

**Files:**
- Modify: `Src/UI/mainwindow.cpp` (scattered references)

- [ ] **Step 1: Update `onNew()` to use `_addCanvasPage()`**

Find `onNew()` (around line 1900). Replace:

```cpp
void MainWindow::onNew()
{
    _addCanvasPage(tr("Canvas %1").arg(_canvasCount() + 1),
                   QStringLiteral("canvas-%1").arg(_canvasCount() + 1));
}
```

- [ ] **Step 2: Update `onNewProject()` — replace `_addCanvasDock` call**

Find `onNewProject()` (around line 1944). Replace `_addCanvasDock(fi.completeBaseName(), QStringLiteral("canvas-1"));` with `_addCanvasPage(fi.completeBaseName(), QStringLiteral("canvas-1"));`.

- [ ] **Step 3: Update `loadSession()` — replace `_addCanvasDockInternal` calls**

Find `loadSession()` (around line 1569). Replace `_addCanvasDockInternal(canvasPage, canvasPage->title());` with the new equivalent:

```cpp
// Replace:
// _addCanvasDockInternal(canvasPage, canvasPage->title());
// With:
auto *docPanel = dockPanelManager()->addDocumentPanel(canvasPage->title());
docPanel->setWidget(canvasPage);
docPanel->setProperty("pageId", canvasPage->pageId());
// Set the first canvas as active
if (!m_activeDocumentPanel) {
    m_activeDocumentPanel = docPanel;
    _onDocumentPanelActivated(docPanel);
}

// Replace:
// _addCanvasDockInternal(canvasPage, tabTitle);
// With:
auto *docPanel = dockPanelManager()->addDocumentPanel(tabTitle);
docPanel->setWidget(canvasPage);
docPanel->setProperty("pageId", canvasPage->pageId());
if (!m_activeDocumentPanel) {
    m_activeDocumentPanel = docPanel;
    _onDocumentPanelActivated(docPanel);
}
```

Also in `loadSession()`, replace `_setCurrentCanvasIndex()` and similar calls — remove the set current logic since DockPanelManager will fire `dockPanelActivated` automatically when the user clicks a tab.

- [ ] **Step 4: Replace `_currentCanvasIndex()` in `closeEvent()`**

In `closeEvent()` (around line 3377), replace:

```cpp
si.activeTabIndex = qMax(0, _currentCanvasIndex());
```

With:
```cpp
auto docList = dockPanelManager()->documentPanelList();
si.activeTabIndex = docList.indexOf(m_activeDocumentPanel);
if (si.activeTabIndex < 0) si.activeTabIndex = 0;
```

- [ ] **Step 5: Replace all remaining `_canvasPageAt(i)` iteration patterns**

Replace patterns in `_collectCanvasBundles()` and `saveSession()` where:
```cpp
for (int i = 0; i < _canvasCount(); ++i) {
    auto *page = qobject_cast<QAtCanvasPage *>(_canvasPageAt(i));
```
With:
```cpp
auto docList = dockPanelManager()->documentPanelList();
for (auto *docPanel : docList) {
    auto *page = qobject_cast<QAtCanvasPage *>(docPanel->widget());
```

- [ ] **Step 6: Remove `eventFilter()` entirely**

Delete the entire `eventFilter()` method (lines 1358-1386). Qtitan DockPanelManager handles focus/activation internally.

Also remove the `qApp->installEventFilter(this)` call from the constructor (line 155).

- [ ] **Step 7: Wire `dockPanelActivated` signal for canvas tab switching**

In the constructor (after `_initPages()` call, around line 153), add:

```cpp
connect(dockPanelManager(), &Qtitan::DockPanelManager::dockPanelActivated,
        this, [this](Qtitan::DockWidgetPanel *panel) {
            auto *docPanel = qobject_cast<Qtitan::DockDocumentPanel *>(panel);
            if (docPanel)
                _onDocumentPanelActivated(docPanel);
        });
```

- [ ] **Step 8: Update View menu — panel visibility toggles**

In `_initMenuBar()`, add View menu checkboxes for the panels. After the existing View menu block (around line 534), add:

```cpp
viewMenu->addSeparator();
// Panel visibility toggles via View menu
if (m_propsDockPanel && m_propsDockPanel->visibleAction()) {
    QAction *propsVis = m_propsDockPanel->visibleAction();
    propsVis->setText(tr("Properties Panel"));
    propsVis->setShortcut(QKeySequence()); // no default shortcut
    viewMenu->addAction(propsVis);
}
if (m_alignDockPanel && m_alignDockPanel->visibleAction()) {
    QAction *alignVis = m_alignDockPanel->visibleAction();
    alignVis->setText(tr("Align Panel"));
    alignVis->setShortcut(QKeySequence());
    viewMenu->addAction(alignVis);
}
```

- [ ] **Step 9: Update recovery snapshot code in constructor**

In the constructor (lines 244-280), replace `_canvasCount()`, `_canvasPageAt(ci)`, and `_addCanvasDockInternal(page, page->title())` with the new DockDocumentPanel equivalents:

```cpp
auto docList = dockPanelManager()->documentPanelList();
if (ci < docList.size()) {
    page = qobject_cast<QAtCanvasPage *>(docList[ci]->widget());
} else {
    // create new doc panel
    QSizeF sz(bundle.info.width > 0 ? bundle.info.width : 1920,
              bundle.info.height > 0 ? bundle.info.height : 1080);
    qreal ppi = bundle.info.dpi > 0 ? bundle.info.dpi : 150.0;
    page = new QAtCanvasPage(QStringLiteral("recovery-%1").arg(ci + 1), sz, ppi, this);
    AppContext::get().registerPage(page);
    auto *dp = dockPanelManager()->addDocumentPanel(page->title());
    dp->setWidget(page);
    dp->setProperty("pageId", page->pageId());
    m_tabStates[page].modified = false;
    if (!m_activeDocumentPanel) {
        m_activeDocumentPanel = dp;
    }
}
```

And replace `_updateCanvasDockTitle(page)` with `_updateDocumentPanelTitle(page)`.

- [ ] **Step 10: Update `_syncViewState()` and any remaining callers**

In `_syncViewState()` and other methods that reference `m_canvasDocks` or old canvas API — replace with equivalents. Common pattern:

```cpp
// Old:
if (_currentCanvasPage()) { ... }

// New: same — _currentCanvasPage() still exists and works via m_activeDocumentPanel->widget()
```

- [ ] **Step 11: Update `_initConnections()` — replace `_currentCanvasPage()` usage**

In `_bindViewConnections()` around line 798, replace:
```cpp
auto *page = qobject_cast<QAtCanvasPage *>(_currentCanvasPage());
if (page)
    _updateCanvasDockTitle(page);
```
With:
```cpp
auto *page = _currentCanvasPage();
if (page)
    _updateDocumentPanelTitle(page);
```

- [ ] **Step 12: Remove `_maybeCloseCanvas` and any QDockWidget close handling**

The old `_maybeCloseCanvas` uses QDockWidget close detection via eventFilter. With DockDocumentPanel, closing is handled by DockPanelManager's close button. We still need to intercept close. Add connection in constructor:

```cpp
connect(dockPanelManager(), &Qtitan::DockPanelManager::aboutToClose,
        this, [this](Qtitan::DockPanelBase *panel, bool &handled) {
            auto *docPanel = qobject_cast<Qtitan::DockDocumentPanel *>(panel);
            if (!docPanel)
                return;
            QAtCanvasPage *page = qobject_cast<QAtCanvasPage *>(docPanel->widget());
            if (page && !_maybeCloseCanvas(page))
                handled = true; // cancel close
        });
```

- [ ] **Step 13: Build and fix any remaining compile errors**

Run: `cd /Volumes/Caviar/Test/GraphicsDemo && make -j$(sysctl -n hw.ncpu) 2>&1`

Expected: Clean build.

Go through all remaining compile errors one by one — search for any remaining references to:
- `m_canvasDocks` → replace with `dockPanelManager()->documentPanelList()`
- `m_activeCanvasDock` → replace with `m_activeDocumentPanel`
- `_addCanvasDockInternal` → replace with DockDocumentPanel creation
- `_removeCanvasDockInternal` → replace with `dockPanelManager()->removeDockPanel()`
- `_onCanvasDockActivated` → handled by dockPanelActivated signal
- `_updateCanvasDockTitle` → `_updateDocumentPanelTitle`
- `QToolBar` in findChild → `Qtitan::DockToolBar`
- `addDockWidget` → removed (was in _initPropertyPanel)
- `tabifyDockWidget` → removed (DockPanelManager handles this via addDockPanel targetPanel param)
- `removeDockWidget` → removed (DockPanelManager handles this)
- `setTabPosition` → removed
- `setDockNestingEnabled` → removed
- `eventFilter` reference in header → removed

- [ ] **Step 14: Commit**

```bash
git add Src/UI/mainwindow.cpp Src/UI/mainwindow.h
git commit -m "refactor: fix all remaining QDockWidget/QToolBar references for Qtitan migration

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 8: Build, run, and fix runtime issues

**Files:**
- Modify: `Src/UI/mainwindow.cpp` (any runtime fixes)

- [ ] **Step 1: Full clean build**

Run: `cd /Volumes/Caviar/Test/GraphicsDemo && make clean && qmake && make -j$(sysctl -n hw.ncpu) 2>&1 | tail -30`

Expected: Clean build, no errors.

- [ ] **Step 2: Launch app and verify startup**

Run: `open /Volumes/Caviar/Test/GraphicsDemo/Bin/Debug/ATGraphics.app`

Expected:
- App launches without crash
- DockMainWindow renders (may not have a window title bar — check)
- Status bar visible at bottom
- Menu bar visible at top
- Drawing toolbar on left, File & Edit and Align toolbars on top

- [ ] **Step 3: Verify canvas creation**

- File → New → Create a new canvas
- Expected: DockDocumentPanel Tab appears in central area, canvas viewport renders

- [ ] **Step 4: Verify side panels**

- Properties panel should be visible on the right (default expanded)
- Align panel starts hidden (closePanel called in init)

- [ ] **Step 5: Verify auto-hide and pin**

- Click the pin icon on the Properties panel title bar
- Expected: Panel collapses to a side tab (auto-hide state)
- Hover over side tab → panel slides out
- Click pin again → panel stays expanded

- [ ] **Step 6: Verify panel drag and tab grouping**

- Drag Align panel (after opening via View menu) onto Properties panel area
- Expected: They tab together
- Drag a tab out → it becomes a separate panel

- [ ] **Step 7: Verify theme switching**

- View → Theme → Dark
- Expected: QSS dark theme applies, dock chrome unchanged (no Qtitan Style leak)

- [ ] **Step 8: Verify close and reopen**

- Close the app
- Reopen
- Expected: Window position restored, toolbar positions restored, panel states restored

- [ ] **Step 9: Fix any runtime issues found**

Common issues to watch for:
- **No window decorations**: DockMainWindow is a QWidget subclass (not QMainWindow), so platform window decorations may differ. If needed, call `setWindowFlags()` to set appropriate window flags.
- **Menu bar not showing**: Ensure `setMenuBar(ui->menubar)` is called. Since the .ui file creates the menubar, we need to pass it to DockMainWindow. Add to constructor after `ui->setupUi(this)`: `setMenuBar(ui->menubar);`
- **Status bar not showing**: Add `setStatusBar(statusBar());` after creating status bar elements.
- **Central widget conflict**: If DockDocumentPanel doesn't fill the center, check that no central widget was set via `setCentralWidget()`.

Fix any issues and re-test.

- [ ] **Step 10: Commit**

```bash
git add Src/UI/mainwindow.cpp Src/UI/mainwindow.h
git commit -m "fix: runtime issues from Qtitan Dock migration

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 9: Final verification — full regression test

- [ ] **Step 1: Run verification checklist from spec**

Manually verify each item:
- [ ] App launches, DockMainWindow displays
- [ ] New canvas → DockDocumentPanel Tab in central area
- [ ] Multiple canvas tabs switch correctly, Properties/Align sync
- [ ] Properties panel default expanded on right, Align hidden
- [ ] Panels draggable to left/right, not top/bottom
- [ ] Pin button toggles auto-hide (hover to expand)
- [ ] View menu toggles panel visibility
- [ ] Close all canvases → panels disabled
- [ ] ToolBars work (File & Edit, Drawing, Align)
- [ ] ToolBars draggable to rearrange
- [ ] Status bar zoom controls functional
- [ ] Rulers sync with canvas
- [ ] Theme switching (light/dark) works
- [ ] Window state save/restore (position, panels, auto-hide)
- [ ] Minimum window size ≥ 1024×700
- [ ] Undo/Redo with tab switching works
- [ ] No Qtitan Style leak (QSS controls appearance)

- [ ] **Step 2: Commit final polish**

```bash
git add -A
git commit -m "chore: final cleanup and verification for Qtitan Dock migration

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```
