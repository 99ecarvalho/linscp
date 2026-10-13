# Translations

How linscp is translated (P2-09): which texts come from where, how a
language is chosen, and what to do when adding a text or a language. The
decision is [ADR 0013](adr/0013-translations.md).

## Three sets of texts

| Texts | Source | Built into | Loaded by |
|---|---|---|---|
| **Core's**: errors, prompts, the host key question, progress lines, button captions core names (about 1,250 strings) | WinSCP's own translations, `3rd-party/winscp/translations/<LANG>.ini`, by WinSCP's translators | `translations/linscp.<LANG>` beside the executable, one per language (55), by `port/tools/gen_translations.py` | `CoreBridge::SetLanguage`, into the shim |
| **Qt's**: standard dialogs and buttons | Qt's translations: Debian's `qt6-translations-l10n`; MSYS2's `qt6-translations` (locked with Qt) | `qtbase_<locale>.qm`, from Qt's installation; in a Windows deploy folder, `translations/` | a `QTranslator` |
| **The shell's**: menus, columns, dialog titles and labels of `port/qt` | `port/qt/translations/linscp_<locale>.ts`, Qt Linguist files in this repository | `.qm` resources inside the shell (`:/i18n`), by the build machine's `lrelease` | a `QTranslator` |

All three follow one locale: `--lang=<locale>` on the shell's command line
(`--lang=pt_BR`, `--lang=de`), else the system's. Set once at startup, before
core starts (`port/qt/Language.cpp`). Choosing it in the shell's
preferences comes with P3-04.

### Core's: WinSCP's translations, through the same hook as upstream

Upstream's GUI translates core by loading a translation DLL
(`WinSCP.<LANG>`, built from the same INI files) and making it the main
module's `ResInstance`, which core's `LoadStr` reads from.

linscp does the same at the same point, so core is unchanged:

- `gen_translations.py` reads each INI's string-table sections (the four
  `.rc` files the English table is built from: `TextsCore1.rc`,
  `TextsCore2.rc`, `TextsFileZilla.rc`, `TextsWin1.rc`). It maps each
  resource name to its id through upstream's headers and writes
  `linscp.<LANG>`: a first line naming it, then `<id>\t<text>` per string,
  with `\\`, `\n`, `\r` and `\t` escaped.
- The shim (`port/shim/vcl_resources.h`) loads such a file as a module
  (`LoadTranslationModule`) and makes it `ResInstance`
  (`ChangeResourceModule`). Its `LoadString` serves a translation module's
  string, falling back to English for an id the translation lacks.
- The language codes are upstream's. They are Windows' language
  abbreviations, cut to the language where one translation serves it:
  `PT` (Brazilian Portuguese) and `PTG` (Portugal), `CHS` and `CH`
  (Simplified and Traditional Chinese), `JP`, `IN` (Indonesian), `NO`,
  `SRL` (Serbian, Latin script), `FP` (Filipino), `MYA` (Burmese), `XXE`
  (Esperanto), `XXK` (Kabyle); elsewhere the language's two letters (`DE`,
  `FR`). `CoreBridge::SetLanguage` maps a locale to them as upstream maps a
  Windows locale: the language-and-country code first, then the language's.

The translations follow the WinSCP pin: a bump brings its translators'
latest work, and new strings appear in English until they are translated
upstream.

### The shell's: Qt Linguist

The shell's texts are `tr()` strings in `port/qt`, with the `.ts` files in
`port/qt/translations`. The build machine's `lupdate` and `lrelease` are used
for both targets: Debian's `qt6-l10n-tools`, which `qt6-tools-dev-tools`
brings. They compile each `.ts` to a `.qm`, embedded with Qt's resource
compiler. Brazilian Portuguese (`linscp_pt_BR.ts`) is the first translation.
It follows the terms of WinSCP's own Brazilian translation, so the shell
reads like the rest of WinSCP.

## Working with it

After adding or changing a shell text (`tr("...")` in `port/qt`):

```sh
cmake --build build/linux --target update_translations   # runs lupdate over port/qt
```

That adds the text to every `.ts`, untranslated. Translate it with Qt
Linguist (`linguist port/qt/translations/linscp_pt_BR.ts`) or by editing
the `<translation>` element, and commit the `.ts` with the change. ctest
`translations_current` fails while a `.ts` lacks a text the sources have,
or keeps one they dropped. Untranslated texts are reported, not failed.

To add a language for the shell:

```sh
printf '<?xml version="1.0" encoding="utf-8"?>\n<!DOCTYPE TS>\n<TS version="2.1" language="de_DE">\n</TS>\n' \
  > port/qt/translations/linscp_de_DE.ts
cmake -S . -B build/linux && cmake --build build/linux --target update_translations
```

Core's languages need nothing: every language WinSCP has is generated.

## On Linux, standard buttons

Qt takes the labels of standard buttons (OK, Cancel in a
`QDialogButtonBox`) from the platform theme. On a GTK desktop that is GTK's
label, in the desktop's own language (`LANG`), not the one `--lang` asks
for. Qt's translations are loaded all the same, and serve wherever Qt
translates itself. This is how Qt programs behave on GNOME.

## Tests

| ctest | Checks |
|---|---|
| `qt_session_pt_BR` | a session opened in WinSCP's Portuguese: core's progress lines and its "credentials were not specified" error arrive in Portuguese, through the bridge |
| `qt_language` | the shell's and Qt's pt_BR translations load; a query's title and the prompt's title are the shell's translation; Qt translates its own texts |
| `translations_current` | each `.ts` holds exactly the shell's texts |
| `gui_smoke` | the shell reports the locale and core's translation in use |

They pass on Linux, under Wine, and natively on Windows 11 (from the deploy
folder, whose `translations/` holds core's 55 files and Qt's `qtbase_*.qm`).
