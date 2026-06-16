# Zoom Status Bar Redesign

## Problem

当前状态栏缩放控件存在以下问题：

1. **输入框太窄**（48px），三位数以上被截断
2. **"%" 是独立 QLabel**，视觉割裂，且最大宽度仅 20px
3. **线性 QSlider**（1-3200，120px）：低 zoom 区（1%-200%）过于密集难以精调，高 zoom 区（1600%-3200%）几乎无用
4. **无 zoom in/out 按钮**，只能键盘滚轮或手动输入
5. **无快捷预设**（如 Fit、100%、200%），每次需手动输入或多次滚轮
6. **无下拉预设菜单**，功能可发现性低

## Design Principle

参考 Figma 的 −/combo/+ 交互模式与 Adobe 的对数滑块，混合两者优势。所有缩放操作源（按钮、combo、滑块、快捷键、滚轮）通过 `zoomChanged` 信号统一驱动，`StatusBarDirector` 负责多控件双向同步。

## Target

```
[−] [ 100% ▾ ] [+] │ [======log-slider======]
```

- `−` / `+` QToolButton：步进缩放（×0.8 / ×1.25）
- Editable QComboBox：手动输入百分比 + 下拉预设菜单
- 对数 QSlider：80px，映射范围 1%-3200%

总宽度 ~212px（当前 188px，仅多 24px，功能提升显著）。

## Architecture

### Component Layout (StatusBar, right side)

```
...│ [−] [combo] [+] │ [log-slider] │ [resize-canvas] [canvas-label] [tool-label]
    ←── ~138px ──→     ←─ 86px ──→
    ←────────────── ~224px ──────────────→
```

### Log Slider Mapping

```
slider → zoom:  zoom = 0.01 × 3200^(v/100)       v ∈ [0, 100]
zoom → slider:  v = 100 × log₃₂₀₀(zoom / 0.01)
```

常用区域 25%-200% 映射到滑块中间 ~40% 行程，手感精确。

### Combo Dropdown Menu Items

| Item | Action |
|------|--------|
| Fit to Canvas | fitToCanvas(), shortcut Ctrl+0 |
| Fit to Selection | fitInView(selectionRect), shortcut Ctrl+2 |
| --- (separator) | |
| 100% | setZoomLevel(1.0) |
| 200% | setZoomLevel(2.0) |
| 50% | setZoomLevel(0.5) |
| 25% | setZoomLevel(0.25) |
| 400% | setZoomLevel(4.0) |
| 800% | setZoomLevel(8.0) |
| --- (separator) | |
| "Type a number + Enter" | hint text (disabled) |

### Signal Flow

```
                    ┌─────────────────────┐
                    │   QAtGraphicsView    │
                    │   setZoomLevel()     │
                    │   fitToCanvas()      │
                    └────────┬────────────┘
                             │ zoomChanged(qreal)
                             ▼
                    ┌─────────────────────┐
                    │  StatusBarDirector  │
                    │  onZoomChanged()    │
                    └────────┬────────────┘
                             │ blockSignals(true) + update
                    ┌────────┼────────────┐
                    ▼        ▼            ▼
                 combo    slider      (no −/+ state needed)
```

Input sources → Director → View:

| Source | Action |
|--------|--------|
| − button clicked | `view->setZoomLevel(current * 0.8)` |
| + button clicked | `view->setZoomLevel(current * 1.25)` |
| Combo returnPressed | parse int → `view->setZoomLevel(val/100)` |
| Combo activated (preset) | `fitToCanvas()` or `setZoomLevel(pct/100)` |
| Slider valueChanged | log formula → `view->setZoomLevel(zoom)` |

### Bidirectional Sync (onZoomChanged)

```cpp
void StatusBarDirector::onZoomChanged(qreal level) {
    int pct = qRound(level * 100.0);
    // Update combo
    m_zoomCombo->blockSignals(true);
    m_zoomCombo->setCurrentText(QString::number(pct) + "%");
    m_zoomCombo->blockSignals(false);
    // Update slider
    m_zoomSlider->blockSignals(true);
    m_zoomSlider->setValue(_zoomToSliderValue(level));
    m_zoomSlider->blockSignals(false);
}
```

## Code Changes

### mainwindow.h

Remove: `m_zoomLabel`, `m_zoomEdit` (old QLabel + QLineEdit)

Keep & modify: `m_zoomSlider` (range 0-100, log mapping)

Add:
```cpp
QComboBox *m_zoomCombo = nullptr;
QToolButton *m_zoomOutBtn = nullptr;
QToolButton *m_zoomInBtn = nullptr;
```

### mainwindow.cpp — _initStatusBar()

Replace the zoom control block (lines 835-861):

1. Create `m_zoomOutBtn` (QToolButton, text "−", 26×24, autoRaise)
2. Create `m_zoomCombo` (QComboBox, editable, NoInsert policy)
   - lineEdit: QIntValidator(1, 3200), right-aligned, placeholder "100%"
   - Add preset items via `addItem()` with userData storing zoom percent or special token ("fit", "fit-selection")
   - Insert separators between groups
3. Create `m_zoomInBtn` (QToolButton, text "+", 26×24, autoRaise)
4. Modify `m_zoomSlider`: range 0→100, width 80px, no ticks
5. Connect signals:
   - `m_zoomOutBtn->clicked` → lambda: step ×0.8
   - `m_zoomInBtn->clicked` → lambda: step ×1.25
   - `m_zoomCombo->lineEdit()->returnPressed` → `StatusBarDirector::applyZoomFromCombo`
   - `m_zoomCombo->activated(int)` → `StatusBarDirector::applyPresetFromCombo`
   - `m_zoomSlider->valueChanged` → lambda: log formula → `setZoomLevel`
6. Layout: `bar->addPermanentWidget(m_zoomOutBtn)` → `addPermanentWidget(m_zoomCombo)` → `addPermanentWidget(m_zoomInBtn)` → separator → `addPermanentWidget(m_zoomSlider)`

### StatusBarDirector.h

New interface:
```cpp
void setZoomControls(QComboBox *combo, QToolButton *outBtn, QToolButton *inBtn, QSlider *slider);
void applyZoomFromCombo();
void applyPresetFromCombo(int index);

private:
    QComboBox *m_zoomCombo = nullptr;
    QToolButton *m_zoomOutBtn = nullptr;
    QToolButton *m_zoomInBtn = nullptr;
    QSlider *m_zoomSlider = nullptr;

    static qreal _sliderToZoom(int sliderValue);
    static int _zoomToSliderValue(qreal zoom);
```

### StatusBarDirector.cpp

Implement:
- `_sliderToZoom(v)`: `return 0.01 * std::pow(3200.0, v / 100.0);`
- `_zoomToSliderValue(z)`: `return qRound(100.0 * std::log(z / 0.01) / std::log(3200.0));`
- `applyZoomFromCombo()`: parse combo text (strip "%"), validate, clamp, call `view->setZoomLevel(pct/100.0)`
- `applyPresetFromCombo(index)`: read itemData to determine action (fit/fit-selection/pct), execute
- `onZoomChanged(level)`: update combo text and slider value with blockSignals
- Remove all old `m_zoomEdit` / `m_zoomLabel` logic

### ViewActions.cpp (minor)

`FitToCanvasAction` and `ResetZoomAction` already emit `zoomChanged` via view — no changes needed. The StatusBarDirector will pick up the signal and sync combo + slider.

## Edge Cases

1. **Invalid input**: non-numeric text → revert to current zoom; value < 1 → clamp to 1; value > 3200 → clamp to 3200
2. **Slider at extremes**: slider=0 → zoom=1%, slider=100 → zoom=3200%. Rounding may cause slight mismatch at edges — acceptable
3. **Combo dropdown open while zoom changes externally** (e.g. Ctrl+scroll): `setCurrentText` while popup is visible resets the highlighted item — acceptable, Figma does the same
4. **Page switch with no canvas**: disable −/+/combo/slider when pageType != "canvas"
5. **Rapid slider drag**: log formula computation is cheap (std::pow), but throttle `setZoomLevel` calls if performance issues arise (unlikely — view already early-returns on `isEqual`)

## Verification

- [ ] Type "150" + Enter → zoom jumps to 150%, combo shows "150%", slider at correct log position
- [ ] Click − from 100% → zoom 80%, all controls sync
- [ ] Click + from 100% → zoom 125%, all controls sync
- [ ] Select "Fit to Canvas" from dropdown → view fits canvas, combo updates
- [ ] Ctrl+0 → zoom resets to 100%, combo shows "100%", slider mid-point
- [ ] Ctrl+scroll → zoom changes, combo and slider sync in real-time
- [ ] Drag slider to ~50% position → zoom ~56.6%, combo shows "57%"
- [ ] Type "abc" + Enter → reverts to current zoom, no crash
- [ ] Type "0" + Enter → clamps to 1%
- [ ] Switch to non-canvas page → zoom controls disabled
- [ ] Switch back to canvas page → zoom controls re-enabled and synced
- [ ] Status bar still fits at minimum window width (1200px)
