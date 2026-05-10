# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```bash
# One-time configure (generates Makefile in build/)
qmake -o build/Makefile GraphicsDemo.pro
# or from within the build directory:
cd build && qmake ..

# Build
make -C build

# Run
open build/Src/GraphicsDemo.app

# Clean build
make -C build clean && make -C build
```

## Running Tests

```bash
# Configure tests
cd Tests && qmake Tests.pro && make && ./tst_alignment
```

The test binary `tst_alignment` is a Qt TestLib executable — run it directly for text output or with `-xunitxml` for CI.

```bash
# Run a single test function
./Tests/tst_alignment TestAlignment::testAlignLeft

# List available tests
./Tests/tst_alignment -functions
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

Qt 6.11 C++17 desktop app — a vector graphics editor (rectangles, ellipses, lines, Bezier curves, text, images, freehand drawing) on a canvas.

**Layer map:**
- **`Src/App/`** — `main.cpp`, entry point with i18n setup
- **`Src/Items/`** — All drawable item types. Every custom item inherits from both a `QGraphicsXxxItem` and `IGraphicsItem` (the shared interface at `Items/IGraphicsItem.h`)
- **`Src/UI/`** — Main window, custom QGraphicsView (`QAtGraphicsView`), property panel dock, dialogs (new file, import, export, align/distribute, gradient)
- **`Src/Commands/`** — QUndoCommand subclasses for all undoable operations (add, remove, move, property changes, paste, z-order, align/distribute, rotation)
- **`Src/Utils/`** — Stateless utility functions: image I/O with libtiff, color helpers, alignment/distribution math
- **`Src/Qt-Color-Widgets/`** and **`Src/QtGradientEditor/`** — Vendored third-party color picker and gradient editor libraries, statically linked
- **`Tests/`** — Qt TestLib unit tests (currently covers `AlignmentUtils`)

**Key design patterns:**
- **`IGraphicsItem`** (`Items/IGraphicsItem.h`) — Pure virtual interface that all custom items implement. Defines `itemType()`, `propertyFlags()`, `cloneItem()`, `serialize()`/`deserialize()`, and optional geometry/text/image accessors. `propertyFlags()` declares which properties an item supports; the property panel uses this to show/hide controls.
- **`CanvasItem`** — A special background QGraphicsRectItem (not an IGraphicsItem) that serves as the white drawing surface. Always at (0,0), non-selectable, non-movable. Defines PPI for physical-unit rulers.
- **`QAtGraphicsView`** — Custom QGraphicsView subclass. Manages tool state (select, hand, rect, ellipse, line, bezier, freehand, text, image), mouse interaction for drawing/moving, zoom, grid rendering, resize handles, clipboard paste tracking, and space-bar temporary hand-tool toggle. Signals `itemAdded()`, `selectionChanged()`, etc. up to `MainWindow`.
- **Undo/redo** — `MainWindow` owns a `QUndoStack`. Every state mutation goes through a `QUndoCommand` subclass from `Commands/Commands.h`. The property panel emits signals (e.g., `penChanged()`) that `MainWindow` catches to create `PropertyChangeCommand` instances.
- **Clipboard** — Items serialize/deserialize via `QDataStream` with a version header. Copy writes serialized item data to `QClipboard` as a custom MIME type. Paste deserializes using `createItemByType()` factory.
- **Alignment/Distribution** — `AlignmentUtils` namespace provides pure functions that compute new positions without mutating items. `MainWindow` wraps results in `AlignItemsCommand` for undo support. `AlignmentUtils::itemGeometryRect()` uses `IGraphicsItem::geometryRect()` (pen-excluded) when available, falling back to `boundingRect()`.

**Naming conventions** (from `.clang-tidy`):
- Classes: `CamelCase`, functions/variables: `camelBack`
- Member variables: `m_` prefix (e.g., `m_undoStack`)
- Constants/enums: `UPPER_CASE` or `kCamelCase`
- Qt signal/slot naming follows Qt conventions

**Internationalization** — Chinese translation via Qt Linguist (`.ts` files in `Src/translations/`).

**Dependencies** — Qt 6 (Core, Gui, Widgets) via Homebrew, libtiff for lossless TIFF export. Both the app and test builds enable RTTI (`CONFIG += rtti`) because `AlignmentUtils` uses `dynamic_cast<IGraphicsItem*>`.
