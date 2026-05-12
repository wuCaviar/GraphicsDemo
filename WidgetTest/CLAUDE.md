# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```bash
# Configure (generates Makefile in build/)
qmake -o build/Makefile WidgetTest.pro

# Build
make -C build

# Run
open build/WidgetTest.app

# Clean build
make -C build clean && make -C build
```

## Architecture

A standalone Qt 6 C++17 test application for exploring Little CMS (lcms2) color space conversions — specifically bidirectional sRGB ↔ CMYK conversion using ICC profiles.

**Components:**
- **`ColorManager`** — Singleton that wraps lcms2. Loads two ICC profiles (sRGB IEC61966-2.1, Japan Color 2001 Coated), builds transforms on demand per rendering intent + flags, and exposes `toCmyk(QColor)` / `toRgb(Cmyk)` conversion. Profiles are loaded from hardcoded paths under `/Volumes/Caviar/Test/`.
- **`MainWindow`** — Qt Designer–based UI with RGB spin boxes (0–255), CMYK double spin boxes (0–100), a rendering-intent combo, and checkboxes for all lcms2 transform flags. Changing either side rebuilds the lcms2 transform with the current intent/flags and updates the other side.

**Data flow:** User edits RGB → `buildRGB2CMYKTransforms` + `toCmyk` → CMYK spin boxes update. User edits CMYK → `buildCMYK2RGBTransforms` + `toRgb` → RGB spin boxes update. Signal disconnection prevents feedback loops during programmatic updates.

**Dependencies:** Qt 6 (Widgets), lcms2 via Homebrew (`/opt/homebrew`). Also inherits the parent `GraphicsDemo` CLAUDE.md for the main vector editor project.
