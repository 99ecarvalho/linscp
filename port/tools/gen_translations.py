#!/usr/bin/env python3
"""
Core's texts in WinSCP's translations, for the shim's LoadString.

  gen_translations.py --src 3rd-party/winscp/source --translations 3rd-party/winscp/translations --out DIR

WinSCP's translations (translations/<LANG>.ini, one per language, named by
upstream's language codes) hold, besides the forms, the string tables of
each .rc file, keyed by resource name. For each language this writes
DIR/linscp.<LANG>: the strings of the same .rc files gen_resources.py builds
the English table from, keyed by numeric id, which the shim loads as a
translation module (vcl_resources.h: LoadTranslationModule), as upstream's
GUI loads the translation DLL WinSCP.<LANG>.

The file is UTF-8 text. A first line names it; then one string per line,
`<id>\\t<text>`, with backslash, newline, carriage return and tab escaped as
\\\\, \\n, \\r and \\t. Names upstream's headers do not define are skipped (a
translation of a string upstream has since dropped).
"""
import argparse, glob, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_resources import parse_ids, unquote  # noqa: E402

# The .rc files gen_resources.py builds the table from.
RC_SECTIONS = ['TextsCore1.rc', 'TextsCore2.rc', 'TextsFileZilla.rc', 'TextsWin1.rc']


def escape(text):
    return (text.replace('\\', '\\\\').replace('\n', '\\n').replace('\r', '\\r').replace('\t', '\\t'))


def read_translation(path):
    """{resource name: text} from the .rc sections of one translation."""
    strings, section = {}, None
    with open(path, encoding='utf-8-sig') as f:
        for raw in f:
            line = raw.rstrip('\r\n')
            if line.startswith('['):
                section = line.strip('[]')
                continue
            if section not in RC_SECTIONS or not line or line.startswith(';'):
                continue
            name, sep, value = line.partition('=')
            if sep and value.startswith('"'):
                strings[name.strip()] = unquote(value)
    return strings


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--src', required=True)
    ap.add_argument('--translations', required=True)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    ids = parse_ids(a.src)
    os.makedirs(a.out, exist_ok=True)
    count = 0
    for path in sorted(glob.glob(os.path.join(a.translations, '*.ini'))):
        lang = os.path.splitext(os.path.basename(path))[0]
        if lang.lower() == 'version':
            continue
        rows = sorted((ids[name], text) for name, text in read_translation(path).items() if name in ids)
        target = os.path.join(a.out, f'linscp.{lang}')
        content = (f'linscp translation {lang}, from WinSCP translations/{lang}.ini\n' +
                   ''.join(f'{ident}\t{escape(text)}\n' for ident, text in rows))
        # Rewritten only when it changes, so builds that depend on it do not redo.
        if not os.path.exists(target) or open(target, encoding='utf-8').read() != content:
            with open(target, 'w', encoding='utf-8', newline='\n') as f:
                f.write(content)
        count += 1
    print(f'{count} translations in {a.out}')


if __name__ == '__main__':
    main()
