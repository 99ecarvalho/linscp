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
  binding   - a Borland implicit closure at a position recorded in
              port/patches/closure_sites.json, as CLOSURE(Name) or
              CLOSURE_OF(obj, Name). Recomputed here with a regex, not with
              transform.py's own scanner, so the two must agree
  slash     - an #include path with its backslashes made forward slashes
  inline    - in a header, a namespace-scope function definition given
              `inline` and nothing else
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
    # Relative to this script, as transform.py loads it; and required, so the
    # verifier cannot agree with a transform that bound nothing.
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        '..', 'patches', 'closure_sites.json')
    if not os.path.exists(path):
        sys.exit(f'missing {path}')
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
        # What precedes the name: an optional & (not &&), then an optional
        # `obj.` or `obj->`. The earliest position the pattern can start from
        # is the start of the whole expression.
        m = _BINDING_LEFT.search(out[:start])
        begin = m.start()
        obj, op = m.group('obj'), m.group('op')
        if obj is None:
            new = f'CLOSURE({ident})'
        elif op == '->':
            new = f'CLOSURE_OF({obj}, {ident})'
        else:
            new = f'CLOSURE_OF(&{obj}, {ident})'
        out = out[:begin] + new + out[start + len(ident):]
    return out


_BINDING_LEFT = _re.compile(
    r'(?:(?<!&)&\s*)?(?:(?P<obj>[A-Za-z_]\w*)\s*(?P<op>\.|->)\s*)?$')


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


def _load_patchset():
    # By path: under python -I the script's directory is not on sys.path.
    import importlib.util
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'patchset.py')
    spec = importlib.util.spec_from_file_location('patchset', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main(gen='build/gen', src='3rd-party/winscp'):
    changed = macros = files = closures = closeoffs = bindings = inlines = slashes = 0
    tryfinallys = 0
    patched_lines = 0
    patchset = _load_patchset()
    series = patchset.load_series()
    bad = []
    for dp, _, ns in os.walk(gen):
        for n in ns:
            g = os.path.join(dp, n)
            u = os.path.join(src, os.path.relpath(g, gen))
            if not os.path.exists(u):
                bad.append(f'{g}: no upstream counterpart'); continue
            rel_for_sites = os.path.relpath(g, gen).replace(os.sep, '/')
            pristine = open(u, newline='', encoding='utf-8', errors='surrogateescape').read()
            # The reviewed patch series is the baseline the rules apply to; the
            # lines it changes are accounted for by the patch itself.
            patched = patchset.apply(rel_for_sites, pristine, series)
            patched_lines += sum(1 for p, q in zip(pristine.split('\n'), patched.split('\n'))
                                 if p != q)
            a = patched.split('\n')
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
                elif (_re.match(r'\s*#\s*include\b', x) and '\\' in x and
                      y == x.replace('\\', '/')):
                    slashes += 1
                elif (u.lower().endswith('.h') and
                      y.lstrip() == 'inline ' + x.lstrip() and
                      y[:len(y) - len(y.lstrip())] == x[:len(x) - len(x.lstrip())]):
                    inlines += 1
                else:
                    bad.append(f'{u}:{i}: changed line matches no transform rule\n'
                               f'      - {x.strip()[:80]}\n      + {y.strip()[:80]}')

    print(f'files compared: {files}')
    print(f'lines changed:  {changed}')
    print(f'  property:     {macros}')
    print(f'  closure:      {closures}')
    print(f'  template end: {closeoffs}')
    print(f'  closure bind: {bindings}')
    print(f'  inline:       {inlines}')
    print(f'  include /:    {slashes}')
    print(f'  patch series: {patched_lines}')
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
