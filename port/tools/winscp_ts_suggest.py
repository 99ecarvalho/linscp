#!/usr/bin/env python3
"""Fill a shell .ts file's unfinished strings with WinSCP's own translations
of the same English text, where WinSCP has it (doc/qt/translations.md).

The shell's dialogs follow WinSCP's, captions included, so most of their
strings are WinSCP's: a form's caption or hint (forms/*.dfm; a hint's
"short|long" parts each) or a string-table entry (resource/*.rc). Each English text found there is looked up in WinSCP's
translation of the language (translations/<LANG>.ini, by form and object
path, or by resource name), and taken when every occurrence agrees.

  winscp_ts_suggest.py port/qt/translations/linscp_pt_BR.ts PT [--write]

Without --write, prints what it would fill; strings it cannot find, finds
translated differently in different places, or finds translated in a way
check_ts_current.py refuses, are left for a person.
"""
import argparse, glob, html, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from check_ts_current import problems  # noqa: E402

ROOT = os.path.join(os.path.dirname(__file__), '..', '..', '3rd-party', 'winscp')


def delphi_string(text):
    """'It'#39's' -> It's (a .dfm string literal, possibly concatenated)."""
    out = []
    for part in re.finditer(r"'((?:[^']|'')*)'|#(\d+)", text):
        if part.group(1) is not None:
            out.append(part.group(1).replace("''", "'"))
        else:
            out.append(chr(int(part.group(2))))
    return ''.join(out)


def dfm_captions(path):
    """(object path, English text) for each Caption, Hint and Items.Strings."""
    names = []
    # A collection's items (a list view's columns...) are keyed item0,
    # item1... in the translations; one counter per open collection.
    items = []
    lines = open(path, encoding='utf-8', errors='replace').read().replace('\r', '').split('\n')
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        m = re.match(r'(?:object|inherited|inline) (\w+)(:?)', line)
        if m:
            # An object with no name ("object TTBXItem") has no keys.
            names.append(m.group(1) if m.group(2) else None)
        elif line.endswith('= <') or line.endswith('<item') or (line == 'item'):
            if not line.startswith('item'):
                items.append(0)
            if line.endswith('item'):
                names.append(f'item{items[-1]}' if items else None)
                if items:
                    items[-1] += 1
        elif line in ('end', 'end>'):
            if names:
                names.pop()
            if (line == 'end>') and items:
                items.pop()
        else:
            m = re.match(r'(Caption|Hint) = (.*)', line)
            if m:
                value = m.group(2)
                while value.endswith('+'):
                    i += 1
                    value = value[:-1] + lines[i].strip()
                if None not in names:
                    yield '.'.join(names[1:] + [m.group(1)]), delphi_string(value)
            elif line.startswith('Items.Strings = ('):
                index = 0
                while True:
                    i += 1
                    item = lines[i].strip()
                    done = item.endswith(')')
                    if done:
                        item = item[:-1]
                    if None not in names:
                        yield '.'.join(names[1:] + ['Items', 'Strings', str(index)]), delphi_string(item)
                    index += 1
                    if done:
                        break
        i += 1


def rc_strings(path):
    for m in re.finditer(r'^\s*(\w+),\s*"((?:[^"\\]|\\.|"")*)"', open(path, encoding='utf-8', errors='replace').read(), re.M):
        yield m.group(1), m.group(2).replace('""', '"').replace('\\n', '\n').replace('\\t', '\t')


def ini_sections(path):
    sections = {}
    current = None
    for line in open(path, encoding='utf-8', errors='replace').read().replace('\r', '').split('\n'):
        if line.startswith('[') and line.endswith(']'):
            current = sections.setdefault(line[1:-1], {})
        elif current is not None and '=' in line:
            key, value = line.split('=', 1)
            if value.startswith('"') and value.endswith('"'):
                value = value[1:-1]
            current[key] = value.replace('\\r\\n', '\n').replace('\\n', '\n').replace('\\\\', '\\')
    return sections


# The WinSCP forms each shell class follows, whose wording wins when a text
# is translated differently elsewhere.
CONTEXT_FORMS = {
    'PreferencesDialog': ['Preferences.dfm'],
    'TransferSettingsFrame': ['CopyParams.dfm'],
    'TransferSettingsDialog': ['CopyParamCustom.dfm', 'CopyParamPreset.dfm', 'CopyParams.dfm'],
    'TransferSettingsBox': ['Copy.dfm', 'FullSynchronize.dfm'],
    'SynchronizeDialog': ['FullSynchronize.dfm'],
    'SynchronizeProgress': ['SynchronizeProgress.dfm'],
    'TransferDialog': ['Progress.dfm'],
    'PromptDialog': ['Authenticate.dfm'],
    'SyncChecklistDialog': ['SynchronizeChecklist.dfm'],
    'SiteManager': ['Login.dfm'],
    'SiteAdvanced': ['SiteAdvanced.dfm'],
    'MainWindow': ['NonVisual.dfm', 'ScpCommander.dfm', 'CustomScpExplorer.dfm', 'Copy.dfm'],
    'QueuePanel': ['NonVisual.dfm', 'CustomScpExplorer.dfm'],
    'FileView': ['SelectMask.dfm', 'NonVisual.dfm'],
    'EditorWindow': ['Editor.dfm', 'Editor.rc'],
    'FindDialog': ['Editor.dfm'],
    'PropertiesDialog': ['Properties.dfm'],
    'RightsFrame': ['Rights.dfm'],
    'CreateDirectoryDialog': ['CreateDirectory.dfm'],
    'LinkDialog': ['Symlink.dfm'],
    'RemoteTransferDialog': ['RemoteTransfer.dfm'],
    'OpenDirectoryDialog': ['OpenDirectory.dfm'],
    'LocationProfilesDialog': ['LocationProfiles.dfm'],
    'FindFilesDialog': ['FileFind.dfm'],
    'EditMaskDialog': ['EditMask.dfm'],
    'ServerInfoDialog': ['FileSystemInfo.dfm'],
    'GenerateUrlDialog': ['GenerateUrl.dfm'],
    'ConsoleDialog': ['Console.dfm'],
    'ImportSitesDialog': ['ImportSessions.dfm'],
    'CustomCommandDialog': ['CustomCommand.dfm'],
    'KeepUpToDateDialog': ['Synchronize.dfm'],
    'CleanupDialog': ['Cleanup.dfm'],
    'LicenseDialog': ['License.dfm'],
    'EditorPreferencesDialog': ['EditorPreferences.dfm'],
    'AboutDialog': ['About.dfm'],
}


# English text -> the forms that have it, translated or not.
PRESENT = {}


def candidates(lang):
    """English text -> {source file: set of the language's translations}."""
    ini = ini_sections(os.path.join(ROOT, 'translations', lang + '.ini'))
    found = {}
    for dfm in glob.glob(os.path.join(ROOT, 'source', 'forms', '*.dfm')):
        name = os.path.basename(dfm)
        section = ini.get(name, {})
        for key, english in dfm_captions(dfm):
            if english:
                PRESENT.setdefault(english, set()).add(name)
            if key in section and english:
                pairs = [(english, section[key])]
                if key.endswith('.Hint') and ('|' in english):
                    # "Short|Long": the menu's text and its tooltip.
                    english_parts, translated_parts = english.split('|'), section[key].split('|')
                    pairs = list(zip(english_parts, translated_parts)) if len(english_parts) == len(translated_parts) else []
                for english_part, translation in pairs:
                    if english_part:
                        found.setdefault(english_part, {}).setdefault(name, []).append(translation)
    for rc in glob.glob(os.path.join(ROOT, 'source', 'resource', '*.rc')):
        name = os.path.basename(rc)
        section = ini.get(name, {})
        for key, english in rc_strings(rc):
            if key in section and english:
                found.setdefault(english, {}).setdefault(name, []).append(section[key])
    return found


PRINTF = r'%[-+ #0]*[0-9]*(?:\.[0-9]+)?[sdxXu]'


def printf_english(found, english):
    """WinSCP's text with printf placeholders where the shell's has Qt's %1,
    %2... in the same order, or None."""
    if not re.search(r'%\d', english):
        return None
    pattern = re.compile('^' + re.sub(r'%\d', lambda m: PRINTF, re.escape(english)) + '$')
    for key in found:
        if pattern.match(key):
            return key
    return None


def qt_placeholders(text):
    """WinSCP's printf placeholders, in order, as Qt's %1, %2..."""
    count = iter(range(1, 100))
    return re.sub(PRINTF, lambda m: '%%%d' % next(count), text)


def choose(found, context, english):
    """The one translation: the context's forms' if they agree, else
    everyone's if they do."""
    printf = printf_english(found, english)
    if printf is not None:
        translation, options = choose(found, context, printf)
        return (qt_placeholders(translation) if translation is not None else None), options
    by_source = found.get(english) or {}
    preferred = []
    for form in CONTEXT_FORMS.get(context, []):
        preferred += by_source.get(form, [])
    if preferred:
        # The form's own wording; where it has two, the one it uses most.
        return max(sorted(set(preferred)), key=preferred.count), set()
    if any(form in PRESENT.get(english, ()) for form in CONTEXT_FORMS.get(context, [])):
        # The form the dialog follows keeps the English ("R", "W", "X" of
        # the permissions): another form's translation would be another
        # meaning.
        return None, set()
    everyone = set(t for ts in by_source.values() for t in ts)
    if len(everyone) == 1:
        return next(iter(everyone)), set()
    return None, everyone


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('ts')
    ap.add_argument('lang')
    ap.add_argument('--write', action='store_true')
    a = ap.parse_args()
    found = candidates(a.lang)
    text = open(a.ts, encoding='utf-8').read()
    filled = left = 0

    def fill(m, context):
        nonlocal filled, left
        english = html.unescape(m.group(1))
        translation, options = choose(found, context, english)
        if (translation is None) and english.endswith(':'):
            translation, options = choose(found, context, english.rstrip(':'))
            if translation is not None:
                translation += ':'
        if (translation is not None) and problems(english, translation):
            # A translator's slip (a placeholder lost, a stray space): left
            # for a person, as check_ts_current would refuse it.
            print(f'  ?? {english!r} (WinSCP\'s does not fit: {translation!r})')
            left += 1
            return m.group(0)
        if translation is not None:
            filled += 1
            print(f'  {english!r} -> {translation!r}')
            return '<source>%s</source>\n        <translation>%s</translation>' % (
                m.group(1), html.escape(translation, quote=False).replace("'", '&apos;').replace('"', '&quot;'))
        left += 1
        print(f'  ?? {english!r}' + (f' (differs: {sorted(options)})' if options else ''))
        return m.group(0)

    parts = []
    for context in re.split(r'(?=<context>)', text):
        name = re.search(r'<name>(.*?)</name>', context)
        parts.append(re.sub(r'<source>([^<]*)</source>\s*<translation type="unfinished"></translation>',
                            lambda m: fill(m, name.group(1) if name else ''), context))
    text = ''.join(parts)
    print(f'{filled} filled, {left} left', file=sys.stderr)
    if a.write:
        open(a.ts, 'w', encoding='utf-8').write(text)
    return 0


if __name__ == '__main__':
    sys.exit(main())
