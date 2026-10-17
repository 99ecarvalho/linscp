# Translations

How linscp is translated (P2-09): which texts come from where, how a
language is chosen, and what to do when adding a text or a language. The
decision is [ADR 0013](adr/0013-translations.md).

## Three sets of texts

| Texts | Source | Built into | Loaded by |
|---|---|---|---|
| **Core's**: errors, prompts, the host key question, progress lines, button captions core names (about 1,250 strings) | WinSCP's own translations, `3rd-party/winscp/translations/<LANG>.ini`, by WinSCP's translators | `Translations/linscp.<LANG>` beside the executable, one per language (55), by `port/tools/gen_translations.py` | upstream's `TGUIConfiguration` (ADR 0014), into the shim |
| **Qt's**: standard dialogs and buttons | Qt's translations: Debian's `qt6-translations-l10n`; MSYS2's `qt6-translations` (locked with Qt) | `qtbase_<locale>.qm`, from Qt's installation; in a Windows deploy folder, `translations/` | a `QTranslator` |
| **The shell's**: menus, columns, dialog titles and labels of `port/qt` | `port/qt/translations/linscp_<locale>.ts`, Qt Linguist files in this repository | `.qm` resources inside the shell (`:/i18n`), by the build machine's `lrelease` | a `QTranslator` |

All three follow one locale, chosen as upstream's GUI chooses it (ADR 0014):
the configuration's (`Interface\LocaleSafe`, WinSCP's key), else the user
interface's (Windows' UI language; on Linux `LC_ALL`, `LC_MESSAGES` or
`LANG`). Upstream's `TGUIConfiguration`, compiled as it is, loads the
translation. `--lang=<locale>` on the shell's command line (`--lang=pt_BR`,
`--lang=de`) applies one for that run without saving it. The shell's and
Qt's texts follow the locale core's are in (`port/qt/Language.cpp`), and
number formats stay the system's unless `--lang` is given. Choosing it in
the shell's preferences comes with P3-04.

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
- The files are in `Translations/` beside the executable, named after it
  (`linscp.<LANG>`), where upstream's GUI looks. Upstream's
  `TGUIConfiguration` picks one as it picks a DLL: by the locale's Windows
  abbreviation (`PTB`), then by its first two letters (`PT`).
- The language codes are upstream's. They are Windows' language
  abbreviations, cut to the language where one translation serves it:
  `PT` (Brazilian Portuguese) and `PTG` (Portugal), `CHS` and `CH`
  (Simplified and Traditional Chinese), `JP`, `IN` (Indonesian), `NO`,
  `SRL` (Serbian, Latin script), `FP` (Filipino), `MYA` (Burmese), `XXE`
  (Esperanto), `XXK` (Kabyle); elsewhere the language's two letters (`DE`,
  `FR`). Esperanto and Kabyle are not listed yet: upstream finds them
  through a translation DLL's version information, which linscp's files do
  not have.

The translations follow the WinSCP pin: a bump brings its translators'
latest work, and new strings appear in English until they are translated
upstream.

### The shell's: Qt Linguist

The shell's texts are `tr()` strings in `port/qt`, with the `.ts` files in
`port/qt/translations`. The build machine's `lupdate` and `lrelease` are used
for both targets: Debian's `qt6-l10n-tools`, which `qt6-tools-dev-tools`
brings. They compile each `.ts` to a `.qm`, embedded with Qt's resource
compiler. Brazilian Portuguese (`linscp_pt_BR.ts`) was the first translation;
the others followed (P3-25). Each follows the terms of WinSCP's own
translation into its language, so the shell reads like the rest of WinSCP. Most of the shell's strings are WinSCP's
captions and hints, so `port/tools/winscp_ts_suggest.py <ts> <LANG> --write`
fills a `.ts` file's unfinished strings from WinSCP's translation of the
same English text: by the WinSCP form the shell's dialog follows when that form
has it, else where every occurrence agrees. A text with Qt's `%1`, `%2`
matches WinSCP's with printf's `%s`, `%d` in the same places, and takes its
translation with them turned back. What it cannot place is left for a
person.

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

To add a language for the shell (`<locale>` as Qt names it, `<LANG>` as
WinSCP does):

```sh
port/tools/ts_language.py new <locale> <LANG>       # linscp_<locale>.ts, WinSCP's texts in it
port/tools/ts_language.py export port/qt/translations/linscp_<locale>.ts todo.json
# ... a translator fills todo.json's "translation"s, with its glossary ...
port/tools/ts_language.py import port/qt/translations/linscp_<locale>.ts todo.json
cmake -S . -B build/linux                           # the build picks the new file up
```

A text the shell has in two places (a menu item in the main window and in a
panel's popup) is filled from the other with `ts_language.py same <ts>...`,
where its translations agree.

`new` copies the shell's texts from an existing `.ts`, lets lupdate lay out
the language's plural forms, and fills what WinSCP's translation has.
`export` writes the rest as JSON, with the texts already translated as a
glossary of WinSCP's terms. `import` takes back each translation that fits
its English text (see below) and reports the others.

The file is named after the language alone (`linscp_de.ts`) unless WinSCP
has two of it: `pt_BR` and `pt_PT`, `zh_CN` and `zh_TW`. Also `sr_Latn`
(WinSCP's Serbian is in Latin script), `nb` (`NO`), `fil` (`FP`), `my`
(`MYA`), `id` (`IN`), `ja` (`JP`). The shell loads the file from core's
locale, falling back from `de_AT` to `de`.

ctest `translations_current` also refuses a translation that does not fit
its text: a `%1` or `%n` lost or added, more than one `&` mnemonic, other
HTML tags, other leading or trailing spaces. `winscp_ts_suggest.py` leaves
out WinSCP's translations that would fail it, for a person to do.

### Which languages

Every language WinSCP has, except Esperanto and Kabyle, which core cannot
choose yet (above). WinSCP's translations cover about 850 of the shell's
1,121 texts: its captions, hints and messages. The other 270 or so are
linscp's own, or worded differently from WinSCP's. They are translated
for linscp with WinSCP's terms, in batches, the languages most used on the
web first (W3Techs' content languages); TRACKER's P3-25 lists them. Those
texts have not been reviewed by native speakers. Corrections are welcome:
edit the `<translation>`, or open the file in Qt Linguist.

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
