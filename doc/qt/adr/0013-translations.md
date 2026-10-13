# ADR 0013 — Core is translated with WinSCP's translations; the shell with Qt Linguist

**Status:** accepted 2026-10-13
**Date:** 2026-10-13

## Context

WinSCP is translated into 55 languages by its own translators, and the
files are in the pinned submodule (`translations/<LANG>.ini`: the forms'
texts and each `.rc` string table, by resource name). Upstream builds
translation DLLs from them and, in its GUI, makes the chosen one the main
module's `ResInstance`, which core's `LoadStr` reads. The Qt shell's own
texts are new: none of WinSCP's forms is reused, so its form translations
do not apply.

## Decision

- **Core's texts use WinSCP's translations, through upstream's own hook.**
  `port/tools/gen_translations.py` turns each INI's string tables into a
  per-language file keyed by resource id. The shim loads one as a module
  and makes it `ResInstance`, as upstream does with a DLL. Core is
  unchanged. Strings missing from a translation fall back to English.
- **The shell's texts use Qt Linguist** (`port/qt/translations/*.ts`),
  compiled into the shell, with `translations_current` guarding that the
  `.ts` files keep up with the sources.
- **Qt's own texts use Qt's translations**, from Qt's installation, or from
  MSYS2's `qt6-translations`, locked with Qt (ADR 0011).
- **One locale for all three**, from `--lang` or the system, mapped to
  WinSCP's language codes as upstream maps a Windows locale.

## Consequences

- linscp speaks every language WinSCP does, for every message core gives,
  with no translation work of its own; a WinSCP bump brings updated
  translations.
- The shell's texts need translators of their own. Upstream's translation
  workflow (Crowdin, INI files) does not cover them yet; were the shell
  merged upstream, its `.ts` texts could move there.
- A translation module is a shim concept on both targets; Windows does not
  use translation DLLs.

## Verification

`qt_session_pt_BR`, `qt_language`, `translations_current` and `gui_smoke`
(doc/qt/translations.md), on Linux, under Wine and natively on Windows 11.
