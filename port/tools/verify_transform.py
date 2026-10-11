#!/usr/bin/env python3
"""
Audit the generated tree against pristine upstream.

Asserts the invariants the whole upstream-tracking model depends on:
  1. line counts are identical file-for-file
  2. every changed line is accounted for by exactly one transform rule
  3. nothing else differs, including line endings

A changed line must be one of:
  property  - contained __property, now emits exactly one PROP_* macro
  closure   - contained __closure, now declares a TClosure alias
  closeoff  - the line where a multi-line closure typedef ended, changed only
              by turning its ";" into ">;"

Exit non-zero on any violation.
"""
import os, re, sys

MINE = re.compile(r'\bPROP_(R_F|R_G|R_GI|RW_FF|RW_FS|RW_GF|RW_GS|W_F|W_S|'
                  r'IDX_R_G|IDX_RW_GS)\(')

def closeoff(old_line):
    """The expected rewrite of a line that merely closed a closure template."""
    k = old_line.rfind(';')
    if k < 0:
        return None
    return old_line[:k] + '>;' + old_line[k + 1:]


def main(gen='build/gen', src='3rd-party/winscp'):
    changed = macros = files = closures = closeoffs = 0
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
                if '__property' in x:
                    n = len(MINE.findall(y))
                    macros += n
                    if n != 1:
                        bad.append(f'{u}:{i}: emits {n} macros: {y.strip()[:80]}')
                elif '__closure' in x:
                    closures += 1
                    if 'TClosure<' not in y:
                        bad.append(f'{u}:{i}: __closure not rewritten: {y.strip()[:80]}')
                elif y == closeoff(x):
                    closeoffs += 1
                else:
                    bad.append(f'{u}:{i}: changed line matches no transform rule\n'
                               f'      - {x.strip()[:80]}\n      + {y.strip()[:80]}')

    print(f'files compared: {files}')
    print(f'lines changed:  {changed}')
    print(f'  property:     {macros}')
    print(f'  closure:      {closures}')
    print(f'  template end: {closeoffs}')
    if bad:
        print(f'\nVIOLATIONS ({len(bad)}):')
        for v in bad[:25]:
            print('  ', v)
        return 1
    print('\nOK: every changed line is accounted for by one transform rule')
    return 0

if __name__ == '__main__':
    sys.exit(main(*sys.argv[1:]))
