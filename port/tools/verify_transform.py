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
  binding   - a bare member-function name wrapped in CLOSURE(), recorded in
              port/patches/closure_sites.json
  closeoff  - the line where a multi-line closure typedef ended, changed only
              by turning its ";" into ">;"
  tryfinally- a `try`, `__finally` or closing brace rewritten to the
              LINSCP_TRY / LINSCP_FINALLY / LINSCP_END_FINALLY macros

Exit non-zero on any violation.
"""
import os, re, sys

MINE = re.compile(r'\bPROP_(R_F|R_G|R_GI|RW_FF|RW_FS|RW_GF|RW_GS|W_F|W_S|'
                  r'IDX_R_G|IDX_RW_GS)\(')

import json as _json
import re as _re


def _recorded_sites():
    """{(file, line): [(column, identifier), ...]} from the committed list."""
    path = 'port/patches/closure_sites.json'
    if not os.path.exists(path):
        return {}
    by_pos = {}
    for site in _json.load(open(path))['sites']:
        by_pos.setdefault((site['file'], site['line']), []).append(
            (site['column'], site['identifier']))
    return by_pos


_SITES = _recorded_sites()


def binding_of(rel_path, lineno, old_line):
    """The expected rewrite of a line, using the recorded positions.

    Position-based rather than a regex over identifiers: a line can mention a
    recorded name more than once while only one occurrence is a closure site,
    and a blanket substitution would wrap both.
    """
    sites = _SITES.get((rel_path, lineno))
    if not sites:
        return None
    out = old_line
    for col, ident in sorted(sites, reverse=True):
        start = col - 1
        if out[start:start + len(ident)] != ident:
            return None
        out = out[:start] + 'CLOSURE(' + ident + ')' + out[start + len(ident):]
    return out


def tryfinally_of(old_line):
    """The expected rewrite of one of the three try/__finally lines."""
    stripped = old_line.strip()
    if stripped == 'try':
        return old_line.replace('try', 'LINSCP_TRY', 1)
    if stripped == '__finally':
        return old_line.replace('__finally', 'LINSCP_FINALLY', 1)
    if stripped == '}':
        return old_line.replace('}', 'LINSCP_END_FINALLY', 1)
    return None


def closeoff(old_line):
    """The expected rewrite of a line that merely closed a closure template."""
    k = old_line.rfind(';')
    if k < 0:
        return None
    return old_line[:k] + '>;' + old_line[k + 1:]


def main(gen='build/gen', src='3rd-party/winscp'):
    changed = macros = files = closures = closeoffs = bindings = 0
    tryfinallys = 0
    bad = []
    for dp, _, ns in os.walk(gen):
        for n in ns:
            g = os.path.join(dp, n)
            u = os.path.join(src, os.path.relpath(g, gen))
            if not os.path.exists(u):
                bad.append(f'{g}: no upstream counterpart'); continue
            rel_for_sites = os.path.relpath(g, gen).replace(os.sep, '/')
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
                elif tryfinally_of(x) == y:
                    tryfinallys += 1
                elif binding_of(rel_for_sites, i, x) == y:
                    bindings += 1
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
    print(f'  closure bind: {bindings}')
    print(f'  try/finally:  {tryfinallys}')
    if bad:
        print(f'\nVIOLATIONS ({len(bad)}):')
        for v in bad[:25]:
            print('  ', v)
        return 1
    print('\nOK: every changed line is accounted for by one transform rule')
    return 0

if __name__ == '__main__':
    sys.exit(main(*sys.argv[1:]))
