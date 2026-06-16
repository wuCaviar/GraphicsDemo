# Zoom Status Bar Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the status bar zoom controls with a Figma-style −/combo/+ + log slider design.

**Architecture:** Replace QLineEdit+QLabel+linear QSlider with QToolButton(−)/editable QComboBox/QToolButton(+) / log-mapped QSlider. `StatusBarDirector` owns bidirectional sync between all zoom controls and the view. All input sources converge on `view->setZoomLevel()` which emits `zoomChanged` → director updates controls with `blockSignals`.

**Tech Stack:** Qt 5/6 C++, qmake

**Files:**
- Modify: `Src/UI/StatusBarDirector.h`
- Modify: `Src/UI/StatusBarDirector.cpp`
- Modify: `Src/UI/mainwindow.h`
- Modify: `Src/UI/mainwindow.cpp`
- Modify: `Src/UI/qatgraphicsview.h` (add fitToSelection)
- Modify: `Src/UI/qatgraphicsview.cpp` (add fitToSelection)

---

### Task 1: Add fitToSelection() to QAtGraphicsView

**Files:**
- Modify: `Src/UI/qatgraphicsview.h:48`
- Modify: `Src/UI/qatgraphicsview.cpp:143` (after fitToCanvas)

- [ ] **Step 1: Declare fitToSelection() in header**

In `qatgraphicsview.h`, after line 48 (`void fitToCanvas();`), add:

```cpp
void fitToSelection();
```

- [ ] **Step 2: Implement fitToSelection() in .cpp**

In `qatgraphicsview.cpp`, after `fitToCanvas()` ends at line 143, add:

```cpp
void QAtGraphicsView::fitToSelection()
{
    if (!scene())
        return;

    auto items = scene()->selectedItems();
    if (items.isEmpty()) {
        fitToCanvas();
        return;
    }

    QRectF rect;
    for (auto *item : items)
        rect = rect.united(item->sceneBoundingRect());

    if (rect.isEmpty())
        return;

    resetTransform();
    m_zoomLevel = 1.0;

    fitInView(rect, Qt::KeepAspectRatio);

    scale(0.9, 0.9);

    qreal actualScale = transform().m11();
    m_zoomLevel = AtMath::clamp(actualScale, 0.01, 32.0);
    resetCachedContent();

    emit zoomChanged(m_zoomLevel);
}
```

- [ ] **Step 3: Commit**

```bash
git add Src/UI/qatgraphicsview.h Src/UI/qatgraphicsview.cpp
git commit -m "feat: add fitToSelection() to QAtGraphicsView for zoom preset"
```

---

### Task 2: Rewrite StatusBarDirector.h — new zoom interface

**Files:**
- Modify: `Src/UI/StatusBarDirector.h`

- [ ] **Step 1: Replace StatusBarDirector.h zoom interface**

Replace the entire file content:

```cpp
#ifndef STATUSBARDIRECTOR_H
#define STATUSBARDIRECTOR_H

#include <QObject>
#include <QPointF>

class QLabel;
class QComboBox;
class QToolButton;
class QSlider;
enum class Tool;

class StatusBarDirector : public QObject
{
    Q_OBJECT
public:
    explicit StatusBarDirector(QObject *parent = nullptr);

    void setPositionLabel(QLabel *label);
    void setZoomControls(QComboBox *combo, QToolButton *outBtn, QToolButton *inBtn, QSlider *slider);
    void setCanvasLabel(QLabel *label);
    void setToolLabel(QLabel *label);

    void applyZoomFromCombo();
    void applyPresetFromCombo(int index);

public slots:
    void onPageSwitched(const QString &pageId, const QString &pageType);
    void onMousePositionChanged(const QPointF &scenePos);
    void onZoomChanged(qreal level);
    void onToolChanged(Tool tool);

private:
    QLabel *m_posLabel = nullptr;
    QComboBox *m_zoomCombo = nullptr;
    QToolButton *m_zoomOutBtn = nullptr;
    QToolButton *m_zoomInBtn = nullptr;
    QSlider *m_zoomSlider = nullptr;
    QLabel *m_canvasLabel = nullptr;
    QLabel *m_toolLabel = nullptr;

    static qreal _sliderToZoom(int sliderValue);
    static int _zoomToSliderValue(qreal zoom);
};

#endif // STATUSBARDIRECTOR_H
```

- [ ] **Step 2: Commit**

```bash
git add Src/UI/StatusBarDirector.h
git commit -m "refactor: rewrite StatusBarDirector zoom interface for combo/slider design"
```

---

### Task 3: Rewrite StatusBarDirector.cpp — zoom logic

**Files:**
- Modify: `Src/UI/StatusBarDirector.cpp`

- [ ] **Step 1: Rewrite StatusBarDirector.cpp**

Replace the entire file content:

```cpp
#include "StatusBarDirector.h"
#include "atMath.h"
#include "AppContext.h"
#include "QAtCanvasPage.h"
#include "qatgraphicsview.h"

#include <QLabel>
#include <QComboBox>
#include <QToolButton>
#include <QSlider>
#include <QLineEdit>
#include <Qt>
#include <cmath>

StatusBarDirector::StatusBarDirector(QObject *parent) : QObject(parent) { }

void StatusBarDirector::setPositionLabel(QLabel *label)
{
    m_posLabel = label;
}

void StatusBarDirector::setZoomControls(QComboBox *combo, QToolButton *outBtn,
                                         QToolButton *inBtn, QSlider *slider)
{
    m_zoomCombo = combo;
    m_zoomOutBtn = outBtn;
    m_zoomInBtn = inBtn;
    m_zoomSlider = slider;
}

void StatusBarDirector::setCanvasLabel(QLabel *label)
{
    m_canvasLabel = label;
}

void StatusBarDirector::setToolLabel(QLabel *label)
{
    m_toolLabel = label;
}

// ---- Log-slider mapping ----

qreal StatusBarDirector::_sliderToZoom(int sliderValue)
{
    return 0.01 * std::pow(3200.0, sliderValue / 100.0);
}

int StatusBarDirector::_zoomToSliderValue(qreal zoom)
{
    if (zoom <= 0.01) return 0;
    return qRound(100.0 * std::log(zoom / 0.01) / std::log(3200.0));
}

// ---- Combo input ----

void StatusBarDirector::applyZoomFromCombo()
{
    if (!m_zoomCombo) return;
    auto *p = AppContext::get().activeCanvasPage();
    if (!p || !p->view()) return;

    QString text = m_zoomCombo->currentText().remove(QLatin1Char('%')).trimmed();
    bool ok = false;
    int pct = text.toInt(&ok);
    if (!ok || pct < 1) {
        onZoomChanged(p->view()->zoomLevel());
        return;
    }
    pct = qBound(1, pct, 3200);
    qreal level = pct / 100.0;
    p->view()->setZoomLevel(level);
}

void StatusBarDirector::applyPresetFromCombo(int index)
{
    if (!m_zoomCombo) return;
    QVariant data = m_zoomCombo->itemData(index);
    if (!data.isValid()) return;

    auto *p = AppContext::get().activeCanvasPage();
    if (!p || !p->view()) return;

    if (data.type() == QVariant::String) {
        QString action = data.toString();
        if (action == QStringLiteral("fit")) {
            p->view()->fitToCanvas();
        } else if (action == QStringLiteral("fit-selection")) {
            p->view()->fitToSelection();
        }
    } else {
        int pct = data.toInt();
        p->view()->setZoomLevel(pct / 100.0);
    }
}

// ---- Page switching ----

void StatusBarDirector::onPageSwitched(const QString &, const QString &pageType)
{
    // Disconnect previous page signals
    auto pages = AppContext::get().allPages();
    for (auto *p : pages) {
        auto *cp = qobject_cast<QAtCanvasPage *>(p);
        if (cp) cp->disconnect(this);
    }

    bool isCanvas = (pageType == QStringLiteral("canvas"));
    if (m_zoomCombo) m_zoomCombo->setEnabled(isCanvas);
    if (m_zoomOutBtn) m_zoomOutBtn->setEnabled(isCanvas);
    if (m_zoomInBtn) m_zoomInBtn->setEnabled(isCanvas);
    if (m_zoomSlider) m_zoomSlider->setEnabled(isCanvas);

    if (isCanvas) {
        auto *p = AppContext::get().activeCanvasPage();
        if (p) {
            connect(p, &QAtCanvasPage::mousePositionChanged, this,
                    &StatusBarDirector::onMousePositionChanged);
            connect(p, &QAtCanvasPage::zoomChanged, this,
                    &StatusBarDirector::onZoomChanged);
            connect(p, &QAtCanvasPage::toolChanged, this,
                    &StatusBarDirector::onToolChanged);
            // Sync controls to current zoom
            if (p->view()) onZoomChanged(p->view()->zoomLevel());
        }
    }
}

// ---- Signal handlers ----

void StatusBarDirector::onMousePositionChanged(const QPointF &scenePos)
{
    if (!m_posLabel) return;
    m_posLabel->setText(
        QStringLiteral("X: %1  Y: %2").arg(scenePos.x(), 0, 'f', 2).arg(scenePos.y(), 0, 'f', 2));
}

void StatusBarDirector::onZoomChanged(qreal level)
{
    int pct = qRound(level * 100.0);
    if (m_zoomCombo) {
        m_zoomCombo->blockSignals(true);
        m_zoomCombo->setCurrentText(QString::number(pct) + QStringLiteral("%"));
        m_zoomCombo->blockSignals(false);
    }
    if (m_zoomSlider) {
        m_zoomSlider->blockSignals(true);
        m_zoomSlider->setValue(_zoomToSliderValue(level));
        m_zoomSlider->blockSignals(false);
    }
}

void StatusBarDirector::onToolChanged(Tool tool)
{
    if (!m_toolLabel) return;
    static const QMap<int, QString> names = {
        { 0, tr("Select") }, { 1, tr("Hand") },   { 2, tr("Rectangle") }, { 3, tr("Ellipse") },
        { 4, tr("Line") },   { 5, tr("Bezier") }, { 6, tr("Freehand") },  { 7, tr("Text") },
    };
    m_toolLabel->setText(tr("Tool: %1").arg(names.value(static_cast<int>(tool), tr("Unknown"))));
}
```

- [ ] **Step 2: Commit**

```bash
git add Src/UI/StatusBarDirector.cpp
git commit -m "refactor: rewrite StatusBarDirector zoom logic with combo/slider sync"
```

---

### Task 4: Update mainwindow.h — member variable changes

**Files:**
- Modify: `Src/UI/mainwindow.h:24-26` (forward declarations)
- Modify: `Src/UI/mainwindow.h:240-247` (member variables)

- [ ] **Step 1: Add QComboBox forward declaration**

In `mainwindow.h`, add `class QComboBox;` to the forward declarations block (around line 26):

```cpp
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QToolButton;
class QComboBox;          // <-- add this line
class QToolBar;
```

- [ ] **Step 2: Replace member variables**

In `mainwindow.h`, replace lines 241-246:

Remove:
```cpp
    QLabel *m_zoomLabel = nullptr;
    QLineEdit *m_zoomEdit = nullptr;
```

Add after `m_zoomSlider`:
```cpp
    QComboBox *m_zoomCombo = nullptr;
    QToolButton *m_zoomOutBtn = nullptr;
    QToolButton *m_zoomInBtn = nullptr;
```

The zoom member block should become:
```cpp
    // 状态栏控件
    QLabel *m_posLabel = nullptr;
    QLabel *m_canvasLabel = nullptr;
    QLabel *m_toolLabel = nullptr;
    QSlider *m_zoomSlider = nullptr;
    QComboBox *m_zoomCombo = nullptr;
    QToolButton *m_zoomOutBtn = nullptr;
    QToolButton *m_zoomInBtn = nullptr;
    QToolButton *m_resizeCanvasBtn = nullptr;
```

- [ ] **Step 3: Commit**

```bash
git add Src/UI/mainwindow.h
git commit -m "refactor: update mainwindow.h zoom members for combo/slider design"
```

---

### Task 5: Update mainwindow.cpp — _initStatusBar() zoom controls

**Files:**
- Modify: `Src/UI/mainwindow.cpp:835-861`

- [ ] **Step 1: Add QComboBox include**

At the top of `mainwindow.cpp`, verify `<QComboBox>` is included. If not present after the other Qt includes, add:

```cpp
#include <QComboBox>
```

- [ ] **Step 2: Replace zoom control block in _initStatusBar()**

Replace lines 835-861 (from `// 缩放编辑框` through the end of the slider connection) with:

```cpp
    // ---- Zoom controls: − / combo / + / log-slider ----
    m_zoomOutBtn = new QToolButton;
    m_zoomOutBtn->setText(QStringLiteral("−")); // minus sign
    m_zoomOutBtn->setFixedSize(26, 24);
    m_zoomOutBtn->setAutoRaise(true);
    m_zoomOutBtn->setToolTip(tr("Zoom out"));
    connect(m_zoomOutBtn, &QToolButton::clicked, this, [this]() {
        if (m_pView) m_pView->setZoomLevel(m_pView->zoomLevel() * 0.8);
    });

    m_zoomCombo = new QComboBox;
    m_zoomCombo->setEditable(true);
    m_zoomCombo->setInsertPolicy(QComboBox::NoInsert);
    m_zoomCombo->setFixedWidth(72);
    m_zoomCombo->setToolTip(tr("Zoom percentage (type number + Enter, or pick preset)"));
    m_zoomCombo->lineEdit()->setAlignment(Qt::AlignCenter);
    m_zoomCombo->lineEdit()->setValidator(new QIntValidator(1, 3200, m_zoomCombo));
    // Preset items
    m_zoomCombo->addItem(tr("Fit to Canvas"), QStringLiteral("fit"));
    m_zoomCombo->addItem(tr("Fit to Selection"), QStringLiteral("fit-selection"));
    m_zoomCombo->insertSeparator(m_zoomCombo->count());
    m_zoomCombo->addItem(QStringLiteral("100%"), 100);
    m_zoomCombo->addItem(QStringLiteral("200%"), 200);
    m_zoomCombo->addItem(QStringLiteral("50%"), 50);
    m_zoomCombo->addItem(QStringLiteral("25%"), 25);
    m_zoomCombo->addItem(QStringLiteral("400%"), 400);
    m_zoomCombo->addItem(QStringLiteral("800%"), 800);
    m_zoomCombo->setCurrentText(QStringLiteral("100%"));
    // Signals
    connect(m_zoomCombo->lineEdit(), &QLineEdit::returnPressed,
            m_statusBarDirector, &StatusBarDirector::applyZoomFromCombo);
    connect(m_zoomCombo, QOverload<int>::of(&QComboBox::activated),
            m_statusBarDirector, &StatusBarDirector::applyPresetFromCombo);

    m_zoomInBtn = new QToolButton;
    m_zoomInBtn->setText(QStringLiteral("+"));
    m_zoomInBtn->setFixedSize(26, 24);
    m_zoomInBtn->setAutoRaise(true);
    m_zoomInBtn->setToolTip(tr("Zoom in"));
    connect(m_zoomInBtn, &QToolButton::clicked, this, [this]() {
        if (m_pView) m_pView->setZoomLevel(m_pView->zoomLevel() * 1.25);
    });

    // Log-mapped slider
    m_zoomSlider = new QSlider(Qt::Horizontal);
    m_zoomSlider->setRange(0, 100);
    m_zoomSlider->setValue(50); // 50 -> ~56.6% default; corrected on first sync
    m_zoomSlider->setFixedWidth(80);
    m_zoomSlider->setToolTip(tr("Adjust zoom level"));
    connect(m_zoomSlider, &QSlider::valueChanged, this, [this](int v) {
        if (m_pView)
            m_pView->setZoomLevel(0.01 * std::pow(3200.0, v / 100.0));
    });
```

- [ ] **Step 3: Update layout lines**

Replace lines 923-925:
```cpp
    bar->addPermanentWidget(m_zoomEdit);
    bar->addPermanentWidget(m_zoomLabel);
    bar->addPermanentWidget(m_zoomSlider);
```

With:
```cpp
    bar->addPermanentWidget(m_zoomOutBtn);
    bar->addPermanentWidget(m_zoomCombo);
    bar->addPermanentWidget(m_zoomInBtn);
    bar->addPermanentWidget(createStatusSeparator(bar));
    bar->addPermanentWidget(m_zoomSlider);
```

- [ ] **Step 4: Update StatusBarDirector wiring (line 194)**

Replace line 194:
```cpp
    m_statusBarDirector->setZoomControls(m_zoomLabel, m_zoomEdit, m_zoomSlider);
```

With:
```cpp
    m_statusBarDirector->setZoomControls(m_zoomCombo, m_zoomOutBtn, m_zoomInBtn, m_zoomSlider);
```

- [ ] **Step 5: Commit**

```bash
git add Src/UI/mainwindow.cpp
git commit -m "feat: replace zoom controls with combo/buttons/log-slider in status bar"
```

---

### Task 6: Clean up old zoom sync code in mainwindow.cpp

**Files:**
- Modify: `Src/UI/mainwindow.cpp:804-815` (_bindViewConnections zoom lambda)
- Modify: `Src/UI/mainwindow.cpp:1294-1304` (_syncViewState zoom sync)

- [ ] **Step 1: Remove old zoomChanged lambda in _bindViewConnections()**

Remove lines 804-815 (the zoomChanged lambda connection in `_bindViewConnections`). The block from `// Status bar: mouse position & zoom` should only keep the mouse position connection.

Replace lines 804-815:
```cpp
    // Status bar: mouse position & zoom
    connect(m_pView, &QAtGraphicsView::mousePositionChanged, this,
            [this](const QPointF &pos) { _updatePosLabel(pos); });
    connect(m_pView, &QAtGraphicsView::zoomChanged, this, [this](qreal level) {
        int pct = qRound(level * 100);
        m_zoomEdit->blockSignals(true);
        m_zoomEdit->setText(QString::number(pct));
        m_zoomEdit->blockSignals(false);
        m_zoomSlider->blockSignals(true);
        m_zoomSlider->setValue(pct);
        m_zoomSlider->blockSignals(false);
    });
```

With:
```cpp
    // Status bar: mouse position (zoom is handled by StatusBarDirector)
    connect(m_pView, &QAtGraphicsView::mousePositionChanged, this,
            [this](const QPointF &pos) { _updatePosLabel(pos); });
```

- [ ] **Step 2: Remove old zoom sync in _syncViewState()**

Remove lines 1294-1304 (the zoom sync block in `_syncViewState`):

```cpp
        // Sync zoom
        if (m_pView) {
            qreal zoom = m_pView->zoomLevel();
            int pct = qRound(zoom * 100);
            m_zoomEdit->blockSignals(true);
            m_zoomEdit->setText(QString::number(pct));
            m_zoomEdit->blockSignals(false);
            m_zoomSlider->blockSignals(true);
            m_zoomSlider->setValue(pct);
            m_zoomSlider->blockSignals(false);
        }
```

Keep only what follows after that block (`_updateCanvasLabel();` etc.).

- [ ] **Step 3: Commit**

```bash
git add Src/UI/mainwindow.cpp
git commit -m "refactor: remove old zoom sync code, StatusBarDirector handles it now"
```

---

### Task 7: Build and verify

- [ ] **Step 1: Build the project**

```bash
cd /Volumes/Caviar/Test/GraphicsDemo && qmake && make -j$(sysctl -n hw.ncpu) 2>&1 | tail -30
```

Expected: clean build, no errors.

- [ ] **Step 2: Verify controls work manually**

Launch the app and verify:
- − button zooms out (100% → 80%)
- + button zooms in (100% → 125%)
- Type "150" + Enter → zoom to 150%, combo shows "150%"
- Select "Fit to Canvas" from dropdown → view fits canvas
- Select "50%" from dropdown → zoom to 50%
- Ctrl+0 → zoom resets to 100%, combo shows "100%"
- Ctrl+scroll → zoom changes, combo and slider sync
- Drag slider → zoom updates smoothly
- Type "abc" + Enter → reverts to current zoom (no crash)
- Switch to non-canvas page → zoom controls disabled
- Switch back → controls re-enabled and synced

- [ ] **Step 3: Commit any fixes if needed**

```bash
git add -A && git commit -m "fix: zoom controls edge case fixes from testing"
```
