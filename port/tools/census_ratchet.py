#!/usr/bin/env python3
"""
Ratchet the compile-census numbers so progress cannot silently slip back.

The shim is built incrementally against real core code, which means the honest
measure of progress is "how many errors are left". That number should only ever
go down. Without a check, a refactor that reintroduces a missing type looks like
a passing build, because nothing in core compiles cleanly yet anyway.

This test pins the current count per translation unit. A regression fails. An
improvement also fails, with the command to re-baseline -- a stale baseline is
how a ratchet quietly stops ratcheting.

  census_ratchet.py            verify against port/tests/census_baseline.json
  census_ratchet.py --update   record the current numbers as the new baseline
"""
import argparse, json, os, re, subprocess, sys

BASELINE = 'port/tests/census_baseline.json'

def measure(tu, target):
    r = subprocess.run(
        [sys.executable, 'port/tools/compile_census.py', '--tu', tu,
         '--target', target, '--show', '0'],
        capture_output=True, text=True)
    out = r.stdout
    errors = re.search(r'total errors\s*:\s*(\d+)', out)
    if not errors:
        sys.exit(f'could not parse census output for {tu}:\n{out}\n{r.stderr}')
    types = re.findall(r'--- missing type: (\d+) distinct ---', out)
    return {
        'errors': int(errors.group(1)),
        'missing_types': int(types[0]) if types else 0,
    }

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--update', action='store_true')
    ap.add_argument('--target', default='x86_64-w64-mingw32')
    a = ap.parse_args()

    base = {}
    if os.path.exists(BASELINE):
        base = json.load(open(BASELINE))
    tus = sorted(base.get('units', {})) or ['source/core/Common.cpp']

    current = {tu: measure(tu, a.target) for tu in tus}

    if a.update:
        json.dump({'target': a.target, 'units': current},
                  open(BASELINE, 'w'), indent=2, sort_keys=True)
        open(BASELINE, 'a').write('\n')
        print(f'baseline updated ({BASELINE}):')
        for tu, m in current.items():
            print(f"  {tu}: {m['errors']} errors, {m['missing_types']} types")
        return 0

    if not base:
        sys.exit(f'no baseline; run: {sys.argv[0]} --update')

    failures = []
    for tu, now in current.items():
        was = base['units'].get(tu)
        if was is None:
            failures.append(f'{tu}: not in baseline')
            continue
        for key in ('errors', 'missing_types'):
            if now[key] > was[key]:
                failures.append(
                    f'{tu}: {key} REGRESSED {was[key]} -> {now[key]}')
            elif now[key] < was[key]:
                failures.append(
                    f'{tu}: {key} improved {was[key]} -> {now[key]} '
                    f'(good -- re-baseline with: port/tools/census_ratchet.py --update)')
        print(f"  {tu}: {now['errors']} errors, {now['missing_types']} missing types")

    if failures:
        print('\nCENSUS RATCHET:')
        for f in failures:
            print('  ', f)
        return 1
    print('\nOK: census matches baseline')
    return 0

if __name__ == '__main__':
    sys.exit(main())
