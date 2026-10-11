#!/usr/bin/env python3
"""
port/putty/overrides.txt must list exactly the symbols both core and PuTTY
define. A new one would be a duplicate-definition link error at best, or an
override nobody reviewed; a stale one weakens a PuTTY symbol for no reason.

  check_overrides.py NM CORE_ARCHIVE PUTTY_ARCHIVE OVERRIDES
"""
import subprocess, sys


def defined(nm, archive):
    out = subprocess.run([nm, '--defined-only', archive], capture_output=True,
                         text=True, check=True).stdout
    syms = set()
    for line in out.splitlines():
        parts = line.split()
        # Strong or weak definitions of code and data; .refptr.* are mingw's
        # own COMDAT helpers and duplicate legitimately.
        if len(parts) == 3 and parts[1] in 'TDRBWV' and not parts[2].startswith('.refptr.'):
            syms.add(parts[2])
    return syms


def main(nm, core, putty, overrides):
    listed = {l.strip() for l in open(overrides) if l.strip() and not l.startswith('#')}
    both = defined(nm, core) & defined(nm, putty)
    failures = []
    for s in sorted(both - listed):
        failures.append(f'{s}: defined by both core and PuTTY, but not in {overrides}')
    for s in sorted(listed - both):
        failures.append(f'{s}: listed, but no longer defined by both')
    print(f'overridden symbols: {len(listed)}; defined by both: {len(both)}')
    if failures:
        for f in failures:
            print('  ', f)
        return 1
    print('OK: overrides.txt lists exactly what core overrides')
    return 0


if __name__ == '__main__':
    sys.exit(main(*sys.argv[1:5]))
