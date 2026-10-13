# ADR 0012 — The Qt shell reaches core through a bridge, never in one unit

**Status:** accepted 2026-10-13
**Date:** 2026-10-13

## Context

The Qt shell (P2) drives core, whose headers need the VCL shim: `vcl.h`,
`__property` as `__declspec(property)`, Win32 names and types from the
platform layer on POSIX, `TObject`, `UnicodeString`, and macros upstream
expects. Qt's headers bring their own world: `emit`/`signals`/`slots`,
`QT_*` macros, and on Linux a 4-byte `wchar_t` (ADR 0001). Either can
redefine or shadow the other's names, and the `wchar_t` widths cannot both
hold in one translation unit.

## Decision

- Qt translation units never include the shim or upstream headers; core's
  never include Qt.
- `port/qt/CoreBridge.h` is the only interface between them: plain C++,
  strings as UTF-8 `std::string`, errors as `std::runtime_error`.
  `CoreBridge.cpp` is its VCL side, compiled like any core unit.
- `port/qt/CMakeLists.txt` enforces the split: `linscp-qt-ui` (Qt, moc
  output included) and `linscp-qt-core` (the bridge) are separate targets
  with their own compile options.

## Consequences

- Each core facility the shell needs is a bridge function first. The bridge
  will grow with P2-03 (`Interface.h`'s prompts, errors and progress have to
  reach Qt dialogs) and the file panels; keep it narrow and value-typed.
- Callbacks from core to the shell (core threads, `Interface.h`) go through
  the bridge too, as plain function objects; the Qt side marshals them onto
  its own thread.

## Verification

ctest `gui_smoke` starts core through the bridge, shows the main window and
quits from the event loop, on Linux (offscreen) and on Windows under Wine;
`windows_imports` checks the shell's imports resolve against the DLLs it
ships with.
