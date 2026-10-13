# ADR 0014 — The shell's settings are upstream's GUI configuration, compiled as it is

**Status:** accepted 2026-10-13
**Date:** 2026-10-13

## Context

Preferences (P3-04) need somewhere to keep the GUI's settings. WinSCP keeps
them in core's own configuration storage (the registry, or `WinSCP.ini`),
under `Configuration\Interface\...`, through two classes over core's
`TConfiguration`:

- **`TGUIConfiguration`** (`windows/GUIConfiguration.cpp`, about 1,400
  lines): transfer settings presets, synchronization and queue options,
  the language and its translation, PuTTY's path. It depends on core and
  on little of the GUI.
- **`TWinConfiguration`** (`windows/WinConfiguration.cpp`, about 4,000
  lines): the panels, Commander, Explorer, editors, windows, security.
  It is bound to the VCL GUI (the terminal manager, the drag extension,
  drive views, fonts).

A spike compiled `GUIConfiguration.cpp` from the transformed tree: 23
errors on Linux and 11 on Windows. All were the locale and
translation-module calls, the VCL's `Languages()` and `Screen`, and one
property subscript.

## Decision

- **`GUIConfiguration.cpp` is compiled as it is**, from the transformed
  tree. The transform's scope takes single files as well as directories,
  so only this unit and its header come from `windows/`. A line-preserving
  patch (`gui-configuration-property-subscript.patch`) makes one
  string-list subscript compile under clang.
- **`port/gui` builds it**, VCL side and without Qt, through a linscp
  stand-in for the GUI's precompiled header, `FormsPCH.h`. Upstream's
  would bring the whole VCL GUI; linscp's brings core's PCH, `TextsWin.h`
  and `GuiShim.h`. The stand-in alone redirects the unit's
  `LoadLibraryEx`/`FreeLibrary`, which load nothing but translations as
  data files, to the shim's translation modules (ADR 0013).
- **`GuiShim` supplies what the unit asks of the VCL and Windows:**
  - Delphi's integer `HINST`;
  - `TLanguages`/`Languages()`, Windows' locale list on both platforms,
    so translations match locales the same everywhere;
  - `Screen` (no VCL forms);
  - `ResStringCleanupCache`;
  - `KnownFolderPath`;
  - on Linux, `GetLocaleInfo`, `GetUserDefaultUILanguage` (from `LC_ALL`,
    `LC_MESSAGES`, `LANG`) and the `LANGID` macros.
- **The locale data is Windows' own.** `port/tools/windows_locales.py`
  dumps it on Windows (`EnumSystemLocales` and `GetLocaleInfo`) into
  `port/gui/locales.inc`, because upstream matches translations by Windows'
  three-letter abbreviations, so a reconstruction would drift.
- **The shell's configuration is a `TGUIConfiguration`**:
  `TShellConfiguration`, made by the shell's `CreateConfiguration` as
  upstream's `UserInterface.cpp` makes `TWinConfiguration`, which it plays
  the part of. The command-line client keeps core's own `TConfiguration`.
  The storage set-up (`/ini`, `/rawconfig`, POSIX's INI) is shared.
- **What the shell keeps of `TWinConfiguration`'s settings, it keeps
  under `TWinConfiguration`'s keys and defaults** (`Interface\LocaleSafe`
  first). A `WinSCP.ini` therefore carries over in both directions.
- **The language is chosen as upstream chooses it:** the saved one, else
  the user interface's. The translations are in `Translations/` beside the
  executable, upstream's folder, as `linscp.<LANG>`, upstream's naming
  after the executable. `--lang` applies a locale for one run without
  saving it. The shell's and Qt's texts follow the locale core applied.

## Consequences

- **Upstream's code decides the settings that matter most**:
  - the presets and their storage;
  - the synchronize parameters the Synchronize dialog remembers;
  - the queue's options;
  - the language and its fallbacks (Brazilian Portuguese's `PTB` finds
    `PT`; Traditional Chinese's `CHT` finds `CH`).

  A pin bump brings upstream's changes to them.
- **`TWinConfiguration`'s settings are mirrored, not compiled.** Each one
  the shell adopts is a key, a default and a load/save line in
  `TShellConfiguration`, checked against upstream's `REGCONFIG`.
- **WinSCP's "additional languages"** (Esperanto, Kabyle), which upstream
  lists through the translation DLL's version information, are not listed:
  linscp's translation files carry none.

## Verification

- `qt_session_pt_BR` and `qt_language`: Brazilian Portuguese chosen and
  loaded through upstream's `LoadNewResourceModule`.
- By hand, on Linux and natively on Windows 11:
  - the system's language by default;
  - a saved `LocaleSafe` over it;
  - `--lang` for one run, leaving `LocaleSafe` as saved;
  - the INI written with upstream's `Interface` keys.
