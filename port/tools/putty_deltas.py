#!/usr/bin/env python3
"""
WinSCP's PuTTY against stock PuTTY of the same release: which files WinSCP
changed or added, and whether that is still what doc/qt/putty-deltas.md
reviewed.

  putty_deltas.py --winscp 3rd-party/winscp/source/putty --stock 3rd-party/putty --check port/putty/deltas.txt
  putty_deltas.py ... --write port/putty/deltas.txt

Each line of the list is `<status> +<added> -<removed> <diff hash> <path>`:
status `modified` (a stock file WinSCP changed) or `added` (a file only
WinSCP has; its hash is the file's). Line endings are ignored. --check fails
when the list and the trees disagree in any way -- a delta that appeared,
went or changed -- because a bump of either side then needs the inventory
reviewed: re-run with --write and update doc/qt/putty-deltas.md alike.
"""
import argparse, difflib, hashlib, os, sys


def lines(path):
    return open(path, 'rb').read().decode('latin-1').replace('\r\n', '\n').split('\n')


def deltas(winscp, stock):
    result = []
    for d, _, names in os.walk(winscp):
        for n in sorted(names):
            path = os.path.join(d, n)
            rel = os.path.relpath(path, winscp).replace(os.sep, '/')
            ours = lines(path)
            theirs_path = os.path.join(stock, rel)
            if not os.path.exists(theirs_path):
                digest = hashlib.sha256('\n'.join(ours).encode('latin-1')).hexdigest()[:12]
                result.append(('added', len(ours), 0, digest, rel))
                continue
            theirs = lines(theirs_path)
            if ours == theirs:
                continue
            diff = list(difflib.unified_diff(theirs, ours, lineterm='', n=0))[2:]
            added = sum(1 for l in diff if l.startswith('+'))
            removed = sum(1 for l in diff if l.startswith('-'))
            digest = hashlib.sha256('\n'.join(diff).encode('latin-1')).hexdigest()[:12]
            result.append(('modified', added, removed, digest, rel))
    return sorted(result, key=lambda r: r[4])


def render(rows):
    return ''.join(f'{s} +{a} -{r} {h} {p}\n' for s, a, r, h, p in rows)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--winscp', required=True)
    ap.add_argument('--stock', required=True)
    group = ap.add_mutually_exclusive_group(required=True)
    group.add_argument('--check')
    group.add_argument('--write')
    a = ap.parse_args()
    rows = deltas(a.winscp, a.stock)
    header = ('# WinSCP\'s PuTTY against stock PuTTY: putty_deltas.py --write. Reviewed in\n'
              '# doc/qt/putty-deltas.md; regenerate only together with that review.\n')
    if a.write:
        with open(a.write, 'w', newline='\n') as f:
            f.write(header + render(rows))
        print(f'{len(rows)} deltas written')
        return 0
    recorded = [l for l in open(a.check, encoding='utf-8').read().split('\n') if l and not l.startswith('#')]
    current = render(rows).rstrip('\n').split('\n')
    if recorded == current:
        print(f'OK: {len(rows)} deltas, as reviewed')
        return 0
    for l in difflib.unified_diff(recorded, current, 'reviewed', 'now', lineterm='', n=0):
        print(l)
    print('PuTTY deltas changed since doc/qt/putty-deltas.md was reviewed: review them, then --write')
    return 1


if __name__ == '__main__':
    sys.exit(main())
