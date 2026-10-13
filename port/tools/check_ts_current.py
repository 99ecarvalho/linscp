#!/usr/bin/env python3
"""
The shell's translations (Qt Linguist .ts) hold exactly the texts its
sources have: none missing, none left over.

  check_ts_current.py --lupdate lupdate --ts linscp_pt_BR.ts ... --sources a.cpp b.h ...

Runs lupdate over the sources into a copy of each .ts and compares the
(context, source text) pairs with the committed file. When they differ, the
sources changed without `cmake --build <build> --target update_translations`.
Untranslated texts are reported, not failed: a translation may lag behind.
"""
import argparse, os, shutil, subprocess, sys, tempfile
import xml.etree.ElementTree as ET


def messages(path):
    """{(context, source): finished?}"""
    result = {}
    for context in ET.parse(path).getroot().iter('context'):
        name = context.findtext('name')
        for message in context.iter('message'):
            translation = message.find('translation')
            finished = (translation is not None) and (translation.get('type') not in ('unfinished', 'vanished', 'obsolete'))
            result[(name, message.findtext('source'))] = finished
    return result


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--lupdate', required=True)
    ap.add_argument('--ts', nargs='+', required=True)
    ap.add_argument('--sources', nargs='+', required=True)
    a = ap.parse_args()
    failures = 0
    with tempfile.TemporaryDirectory() as tmp:
        for ts in a.ts:
            copy = os.path.join(tmp, os.path.basename(ts))
            shutil.copy(ts, copy)
            p = subprocess.run([a.lupdate, '-silent', '-no-obsolete', '-locations', 'none'] + a.sources + ['-ts', copy],
                               capture_output=True, text=True)
            if p.returncode != 0:
                print(p.stdout + p.stderr)
                return 1
            committed, current = messages(ts), messages(copy)
            missing = sorted(set(current) - set(committed))
            stale = sorted(set(committed) - set(current))
            name = os.path.basename(ts)
            for context, source in missing:
                print(f'FAIL {name}: not in the .ts: [{context}] {source!r}')
            for context, source in stale:
                print(f'FAIL {name}: no longer in the sources: [{context}] {source!r}')
            failures += len(missing) + len(stale)
            untranslated = sum(1 for done in committed.values() if not done)
            print(f'{name}: {len(committed)} texts, {untranslated} untranslated')
    if failures:
        print('The shell\'s texts changed: cmake --build <build> --target update_translations')
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
