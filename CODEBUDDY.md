# CODEBUDDY.md This file provides guidance to CodeBuddy when working with code in this repository.

## Build Commands

**Build (qmake + MinGW):**
```bash
qmake GraphicsDemo.pro
mingw32-make
```

**Clean:**
```bash
mingw32-make clean
mingw32-make distclean
```

**Run:**
```bash
# Executable output to Build-*/debug/ or Build-*/release/
./GraphicsDemo
```

**Format code:**
```bash
clang-format -i <file>
```
Uses `.clang-format` based on Qt coding style (4-space indent, pointer `*` aligns right, 100-column limit, C++17).

## Architecture

Qt C++ graphics editor application built on the **Qt Graphics View Framework**. Supports importing TIF images (lossless via libtiff), drawing shapes/lines/text as interactive items, editing properties (border color, fill, text style, size), and exporting to TIF/PNG/JPG. Full undo/redo, copy/paste/cut, and alignment/distribution tools.

### Project Structure

- **`GraphicsDemo.pro`** — Top-level qmake subdirs template; delegates to `Src/`.
- **`Src/Src.pro`** — Application target. Lists all headers, sources, and forms. Includes `Common/Common.pri`.
- **`Src/App/main.cpp`** — Entry point; creates `QApplication` and `MainWindow`.

### Module Breakdown

**`Src/Items/`** — Custom graphics items implementing the `IGraphicsItem` interface.

All custom items use **multiple inheritance**: a Qt base class (e.g., `QGraphicsRectItem`) + `IGraphicsItem` interface. This gives each item both standard Graphics View behavior and a uniform property API for the property panel and serialization.

- `IGraphicsItem` — Pure virtual interface defining: `itemType()`, `propertyFlags()` (HasPen/HasBrush/HasFont/HasText/HasImage), `cloneItem()`, `itemPen()`/`setItemPen()`, `itemBrush()`/`setItemBrush()`, `serialize()`/`deserialize()`.
- `RectItem` — Rectangle with optional `cornerRadius` (rounded rect). Inherits `QGraphicsRectItem`.
- `EllipseItem` — Ellipse/circle. Inherits `QGraphicsEllipseItem`.
- `LineItem` — Straight line. Inherits `QGraphicsLineItem`. Only HasPen.
- `BezierCurveItem` — Cubic Bézier curve. Inherits `QGraphicsPathItem`. Only HasPen.
- `TextItem` — Editable text. Inherits `QGraphicsTextItem`. Supports background brush, inline editing via double-click.
- `ImageItem` — Raster image. Inherits `QGraphicsPixmapItem`. Stores `m_rawTiffData` (QByteArray) for lossless TIFF re-export.

`IGraphicsItem.cpp` provides `createItemByType()` factory for deserializing items from clipboard data.

**`Src/Commands/`** — Undo/redo commands for `QUndoStack`.

- `AddItemCommand` — Adds an item to scene on redo, removes on undo. Owns item pointer until first redo.
- `RemoveItemsCommand` — Removes items on redo, re-adds on undo.
- `MoveItemsCommand` — Stores old/new positions for each item.
- `PropertyChangeCommand` — Generic property change (Pen/Brush/Font/Text/Geometry/CornerRadius). Uses `QVariant` for old/new values.
- `PasteItemsCommand` — Adds pasted items on redo, removes on undo.
- `ZValueChangeCommand` — Changes z-order for bring-to-front/send-to-back.

**`Src/UI/`** — UI components.

- `MainWindow` — Central controller. Owns `QUndoStack`, creates menus/toolbars/dock widgets. Handles file import/export (TIFF via libtiff, other via QImage), clipboard serialization (custom MIME type `application/x-graphicsdemo-items`), alignment/distribution operations, and tool switching. Connects `PropertyPanel` signals to create `PropertyChangeCommand` entries.
- `QAtGraphicsView` — Custom `QGraphicsView` with drawing tool support. Manages `Tool` enum state (Select/Rect/Ellipse/Line/BezierCurve/Text/Image). Handles mouse press/move/release for drawing interaction. In Select mode uses `RubberBandDrag`; in drawing modes uses `NoDrag` + `CrossCursor`.
- `PropertyPanel` — `QDockWidget` on the right. Displays and edits properties of the selected item: position, size, pen (color/width/style), brush color, font (family/size/bold/italic), text content, corner radius. Emits signals for each property change; MainWindow creates undo commands from these.

**`Src/Common/Common.pri`** — Shared build config. Defines version, libtiff include/link paths (from `3rdParty/`), OpenCV paths, and post-link DLL copy steps.

### Key Design Patterns

- **Tool-based drawing**: `QAtGraphicsView::setTool()` switches interaction mode. Drawing tools create temp items on mouse press, update on move, finalize on release via `AddItemCommand`.
- **Undo via commands**: All mutations go through `QUndoStack::push()`. The `PropertyPanel` never modifies items directly—it emits signals, and `MainWindow` creates `PropertyChangeCommand` instances.
- **Lossless TIFF**: `exportTiffLossless()` writes uncompressed RGBA TIFF via libtiff `TIFFWriteScanline`. `importTiffWithLibtiff()` reads via `TIFFReadScanline`. `ImageItem::m_rawTiffData` preserves original file bytes for re-export.
- **Clipboard**: Items are serialized to `QDataStream` with a custom MIME type. `createItemByType()` deserializes items by type ID.
- **Selection-driven property panel**: `QGraphicsScene::selectionChanged` triggers `PropertyPanel::setItem()`. Single selection shows properties; no selection disables the panel.

### Third-Party Dependencies

- **libtiff** — Headers in `3rdParty/include/`, linked via `Common.pri` using `$$PWD/../../3rdParty/` relative paths.
- **OpenCV 4.13.0** — Still referenced in `Common.pri` with hardcoded local path (`D:/opencv-MinGW/`). Can be removed if not needed.

### Keyboard Shortcuts

| Key | Tool |
|-----|------|
| V | Select |
| R | Rectangle |
| E | Ellipse |
| L | Line |
| C | Bézier Curve |
| T | Text |
| I | Import Image |
