# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

The top-level `GraphicsDemo.pro` is a `TEMPLATE = subdirs` project that builds five subprojects: `Src` (the app), `Tests`, `ColorConversionTests`, `LittleCMSTest`, and `WidgetTest`.

```bash
# One-time configure (generates Makefile in build/)
qmake -o build/Makefile GraphicsDemo.pro
# or from within the build directory:
cd build && qmake ..

# Build everything
make -C build

# Build only the main app
make -C build/Src

# Run
open build/Src/GraphicsDemo.app

# Clean build
make -C build clean && make -C build
```

## Running Tests

There are two test suites. Both are Qt TestLib executables — run directly for text output or with `-xunitxml` for CI.

### AlignmentUtils tests
```bash
cd Tests && qmake Tests.pro && make && ./tst_alignment

# Run a single test function
./Tests/tst_alignment TestAlignment::testAlignLeft

# List available tests
./Tests/tst_alignment -functions
```

### Color conversion tests
```bash
cd ColorConversionTests && qmake ColorConversionTests.pro && make && ./tst_colorconversion

# Run a single test function
./tst_colorconversion TestColorConversion::testRgbToCmyk
```

## Linting

```bash
# Generate compile_commands.json first (required by clang-tidy)
cd build && qmake CONFIG+=ccache .. && compiledb make

# Then run clang-tidy on a file
clang-tidy Src/UI/mainwindow.cpp -p build/
```

A `.clang-tidy` config is already present with Qt-appropriate checks and naming conventions.

Requires `compiledb` (`pip install compiledb` or `brew install compiledb`) to generate `compile_commands.json`.

## Architecture

Qt 6 C++17 desktop app — a vector graphics editor (rectangles, ellipses, lines, Bezier curves, text, images, freehand drawing) on a canvas.

**Layer map:**
- **`Src/App/`** — `main.cpp`, entry point with i18n setup
- **`Src/Items/`** — All drawable item types. Every custom item inherits from both a `QGraphicsXxxItem` and `IGraphicsItem` (the shared interface at `Items/IGraphicsItem.h`)
- **`Src/UI/`** — Main window, custom QGraphicsView (`QAtGraphicsView`), property panel dock, dialogs (new file, import, export, align/distribute, gradient)
- **`Src/Commands/`** — QUndoCommand subclasses for all undoable operations (add, remove, move, property changes, paste, z-order, align/distribute, rotation)
- **`Src/Utils/`** — Stateless utility functions: image I/O with libtiff (`ImageUtils`), color helpers (`ColorUtils`), alignment/distribution math (`AlignmentUtils`)
- **`Src/Qt-Color-Widgets/`** and **`Src/QtGradientEditor/`** — Vendored third-party color picker and gradient editor libraries, statically linked via `.pri` includes
- **`Tests/`** — Qt TestLib unit tests for `AlignmentUtils`
- **`ColorConversionTests/`** — Qt TestLib unit tests for RGB-to-CMYK color conversion
- **`LittleCMSTest/`** — Standalone test app for LittleCMS color management integration
- **`WidgetTest/`** — Standalone test app for widget prototyping (has its own `CLAUDE.md`)

**Key design patterns:**
- **`IGraphicsItem`** (`Items/IGraphicsItem.h`) — Pure virtual interface that all custom items implement. Defines `itemType()`, `propertyFlags()`, `cloneItem()`, `serialize()`/`deserialize()`, and optional geometry/text/image accessors. `propertyFlags()` declares which properties an item supports; the property panel uses this to show/hide controls. Also provides `geometryRect()`/`setGeometryRect()` for pen-excluded precise geometry used by alignment and the property panel.
- **`CanvasItem`** — A special background QGraphicsRectItem (not an IGraphicsItem) that serves as the white drawing surface. Always at (0,0), non-selectable, non-movable. Defines PPI for physical-unit rulers.
- **`QAtGraphicsView`** — Custom QGraphicsView subclass. Manages tool state (select, hand, rect, ellipse, line, bezier, freehand, text, image), mouse interaction for drawing/moving, zoom, grid rendering, resize handles, clipboard paste tracking, and space-bar temporary hand-tool toggle. Signals `itemAdded()`, `selectionChanged()`, etc. up to `MainWindow`.
- **Undo/redo** — `MainWindow` owns a `QUndoStack`. Every state mutation goes through a `QUndoCommand` subclass from `Commands/Commands.h`. The property panel emits signals (e.g., `penChanged()`) that `MainWindow` catches to create `PropertyChangeCommand` instances.
- **Clipboard** — Items serialize/deserialize via `QDataStream` with a version header (`kSerializationVersion`). Copy writes serialized item data to `QClipboard` as a custom MIME type. Paste deserializes using `createItemByType()` factory. `deserialize()` returns `bool` and checks stream status.
- **Alignment/Distribution** — `AlignmentUtils` namespace provides pure functions that compute new positions without mutating items. `MainWindow` wraps results in `AlignItemsCommand` for undo support. `AlignmentUtils::itemGeometryRect()` uses `IGraphicsItem::geometryRect()` (pen-excluded) when available, falling back to `boundingRect()`.
- **`filterSelectableItems()`** — Inline function in `IGraphicsItem.h` that filters out `CanvasItem` (UserType+100) and `ResizeHandleItem` (UserType+200) from item lists. Used across MainWindow, QAtGraphicsView, and AlignLayoutDialog.

**Naming conventions** (from `.clang-tidy`):
- Classes: `CamelCase`, functions/variables: `camelBack`
- Member variables: `m_` prefix (e.g., `m_undoStack`)
- Constants/enums: `UPPER_CASE` or `kCamelCase`
- Qt signal/slot naming follows Qt conventions

**Internationalization** — Chinese translation via Qt Linguist (`.ts` files in `Src/translations/`).

**Dependencies** — Qt 6 (Core, Gui, Widgets) via Homebrew, libtiff for lossless TIFF export. Both the app and test builds enable RTTI (`CONFIG += rtti`) because `AlignmentUtils` uses `dynamic_cast<IGraphicsItem*>`. The `Common.pri` file hardcodes `/opt/homebrew` paths for libtiff — this only works on ARM Macs; Intel Macs need `/usr/local` paths.

**Technical review** — A comprehensive review (`TECHNICAL_REVIEW.md`) documented 54 issues with fix status. Key architectural changes that resulted: TIFF I/O moved from `MainWindow` to `ImageUtils`, `PropertyChangeCommand` geometry dispatch uses `setGeometryRect()` instead of `qgraphicsitem_cast` chains, and serialization has a versioned header format.
