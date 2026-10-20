#!/usr/bin/env python3
"""
Guard: upstream's MFC subset (3rd-party/winscp/libs/mfc) is not free software
and is never built, generated or patched. FileZilla's MFC API is linscp's own
port/mfc-compat (ADR 0017).

Looks only at paths and build metadata, never at the subset's contents:

  - compile_commands.json: no unit compiled from, and no include path into,
    a libs/mfc directory;
  - the transform's list of generated files and slices: nothing under
    libs/mfc;
  - the generated tree: no libs/mfc directory left in it (by existence);
  - the patch series: no patch whose file headers name libs/mfc;
  - FileZilla's include path reaches <afx.h> in port/mfc-compat.

  check_no_mfc.py BUILD_DIR GEN_DIR UPSTREAM_DIR
"""
import json, os, re, subprocess, sys

MFC = re.compile(r'(^|[/\\])libs[/\\]mfc([/\\"\s]|$)')
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))


def main(argv):
    if len(argv) != 4:
        print(__doc__)
        return 2
    build, gen, upstream = argv[1:]
    bad = []

    db = os.path.join(build, 'compile_commands.json')
    entries = json.load(open(db, encoding='utf-8'))
    afx_seen = False
    for e in entries:
        text = e.get('command') or ' '.join(e.get('arguments', []))
        if MFC.search(e['file']) or MFC.search(text):
            bad.append(f'compile_commands.json: {e["file"]}')
        if '/source/filezilla/' in e['file'].replace('\\', '/') and 'port/mfc-compat/include' in text:
            afx_seen = True
    print(f'compile commands scanned: {len(entries)}')
    if not afx_seen:
        bad.append('FileZilla units are not compiled with port/mfc-compat/include')

    transform = os.path.join(ROOT, 'port', 'tools', 'transform.py')
    for flag in ('--list-outputs', '--list-slices'):
        out = subprocess.run([sys.executable, transform, '--src', upstream, flag],
                             capture_output=True, text=True, check=True).stdout.split()
        bad += [f'transform {flag}: {f}' for f in out if MFC.search(f)]

    for d in (os.path.join(gen, 'libs', 'mfc'), os.path.join(gen + '-slices', 'libs', 'mfc')):
        if os.path.exists(d):
            bad.append(f'generated tree still has {d} (delete it)')

    patches = os.path.join(ROOT, 'port', 'patches')
    for name in open(os.path.join(patches, 'series'), encoding='utf-8').read().split('\n'):
        name = name.strip()
        if not name or name.startswith('#'):
            continue
        for line in open(os.path.join(patches, name), encoding='utf-8', errors='replace'):
            if line.startswith(('--- ', '+++ ')) and MFC.search(line):
                bad.append(f'port/patches/{name} patches {line[4:].split()[0]}')
                break

    if bad:
        print(f'\nlibs/mfc IN USE ({len(bad)}):')
        for b in bad:
            print(f'  {b}')
        return 1
    print('OK: nothing is built, generated or patched from libs/mfc')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
