#!/usr/bin/env python3
"""
Audit the generated tree against pristine upstream.

Asserts the invariants the whole upstream-tracking model depends on:
  1. line counts are identical file-for-file
  2. every changed line contained __property upstream
  3. every changed line emits exactly one transform macro
  4. nothing else differs, including line endings

Exit non-zero on any violation.
"""
import os, re, sys

MINE = re.compile(r'\bPROP_(R_F|R_G|R_GI|RW_FF|RW_FS|RW_GF|RW_GS|W_F|W_S|'
                  r'IDX_R_G|IDX_RW_GS)\(')

def main(gen='build/gen', src='3rd-party/winscp'):
    changed = macros = files = 0
    bad = []
    for dp, _, ns in os.walk(gen):
        for n in ns:
            g = os.path.join(dp, n)
            u = os.path.join(src, os.path.relpath(g, gen))
            if not os.path.exists(u):
                bad.append(f'{g}: no upstream counterpart'); continue
            a = open(u, newline='', encoding='utf-8', errors='surrogateescape').read().split('\n')
            b = open(g, newline='', encoding='utf-8', errors='surrogateescape').read().split('\n')
            if len(a) != len(b):
                bad.append(f'{u}: line count {len(a)} -> {len(b)}'); continue
            files += 1
            for i, (x, y) in enumerate(zip(a, b), 1):
                if x == y:
                    continue
                changed += 1
                m = len(MINE.findall(y))
                macros += m
                if m != 1:
                    bad.append(f'{u}:{i}: emits {m} macros: {y.strip()[:80]}')
                if '__property' not in x:
                    bad.append(f'{u}:{i}: changed but upstream had no __property: {x.strip()[:80]}')
    print(f'files compared: {files}')
    print(f'lines changed:  {changed}')
    print(f'macros emitted: {macros}')
    if bad:
        print(f'\nVIOLATIONS ({len(bad)}):')
        for v in bad[:25]:
            print('  ', v)
        return 1
    print('\nOK: every changed line is exactly one property transform')
    return 0

if __name__ == '__main__':
    sys.exit(main(*sys.argv[1:]))
