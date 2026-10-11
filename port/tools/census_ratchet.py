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

The units are every core .cpp upstream's ScpCore.cbproj builds, read from the
project itself, so a unit upstream adds appears here as a failure demanding a
baseline rather than going unmeasured. They are compiled in parallel.

Both targets are measured. Linux compiles core now too, and the whole point of
the project is one codebase for both, so letting one platform drift ahead
unnoticed is exactly the failure this guards against.

  census_ratchet.py            verify against port/tests/census_baseline.json
  census_ratchet.py --update   record the current numbers as the new baseline
"""
import argparse, json, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

BASELINE = 'port/tests/census_baseline.json'
TARGETS = ['x86_64-w64-mingw32', 'x86_64-pc-linux-gnu']
GEN = 'build/gen'
CBPROJ = '3rd-party/winscp/source/ScpCore.cbproj'


def project_units():
    """The core translation units upstream builds, in source/core/X.cpp form."""
    text = open(CBPROJ, encoding='utf-8', errors='replace').read()
    units = sorted(set(re.findall(r'Include="core\\([^"\\]+\.cpp)"', text)))
    if not units:
        sys.exit(f'{CBPROJ}: no core units found -- has the project layout changed?')
    return [f'source/core/{u}' for u in units]


def regenerate():
    """Measure the tree the current transform and patches produce, not whatever
    build/gen held when someone last ran the transform by hand. Nothing else
    regenerates it -- CMake writes its own trees -- so a stale one once hid two
    newly recorded closure sites from the census."""
    for cmd in (['port/tools/transform.py', '--out', GEN],
                ['port/tools/verify_transform.py', GEN, '3rd-party/winscp']):
        r = subprocess.run([sys.executable] + cmd, capture_output=True, text=True)
        if r.returncode != 0:
            sys.exit(f'{cmd[0]} failed:\n{r.stdout}{r.stderr}')

def measure(tu, target):
    r = subprocess.run(
        [sys.executable, 'port/tools/compile_census.py', '--tu', tu,
         '--target', target, '--show', '0', '--report', ''],
        capture_output=True, text=True)
    out = r.stdout
    errors = re.search(r'total errors\s*:\s*(\d+)', out)
    if not errors:
        sys.exit(f'could not parse census output for {tu}:\n{out}\n{r.stderr}')
    types = re.findall(r'--- missing type: (\d+) distinct ---', out)
    return {
        'errors': int(errors.group(1)),
        'missing_types': int(types[0]) if types else 0,
        # An aborted parse yields a floor, not a total, so a unit that starts
        # aborting would otherwise look like a sudden improvement.
        'aborted': 'ABORTED EARLY' in out,
    }

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--update', action='store_true')
    a = ap.parse_args()

    regenerate()
    base = {}
    if os.path.exists(BASELINE):
        base = json.load(open(BASELINE))
    tus = project_units()

    jobs = [(t, tu) for t in TARGETS for tu in tus]
    with ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as pool:
        results = list(pool.map(lambda j: measure(j[1], j[0]), jobs))
    current = {t: {} for t in TARGETS}
    for (t, tu), m in zip(jobs, results):
        current[t][tu] = m

    if a.update:
        json.dump({'targets': current}, open(BASELINE, 'w'),
                  indent=2, sort_keys=True)
        open(BASELINE, 'a').write('\n')
        print(f'baseline updated ({BASELINE}):')
        for t in TARGETS:
            total = sum(m['errors'] for m in current[t].values())
            clean = sum(1 for m in current[t].values() if m['errors'] == 0)
            aborted = sum(1 for m in current[t].values() if m.get('aborted'))
            print(f'  {t}: {total} errors, {clean}/{len(tus)} clean, '
                  f'{aborted} aborting')
        return 0

    if not base:
        sys.exit(f'no baseline; run: {sys.argv[0]} --update')

    failures = []
    for t in TARGETS:
        was_t = base.get('targets', {}).get(t)
        if was_t is None:
            failures.append(f'{t}: not in baseline')
            continue
        total = sum(m['errors'] for m in current[t].values())
        clean = sum(1 for m in current[t].values() if m['errors'] == 0)
        aborted = sum(1 for m in current[t].values() if m.get('aborted'))
        print(f'  {t}: {total} errors, {clean}/{len(tus)} clean, '
              f'{aborted} aborting')
        for tu in sorted(set(was_t) - set(current[t])):
            failures.append(f'{t} {tu}: no longer in {CBPROJ} -- re-baseline')
        for tu, now in sorted(current[t].items()):
            was = was_t.get(tu)
            if was is None:
                failures.append(f'{t} {tu}: not in baseline')
                continue
            if now.get('aborted') and not was.get('aborted'):
                failures.append(
                    f'{t} {tu}: now ABORTS early -- its count is a floor, not a '
                    f'total, so any apparent improvement is not real')
                continue
            if was.get('aborted') and not now.get('aborted'):
                failures.append(
                    f'{t} {tu}: no longer aborts (good -- re-baseline with: '
                    f'port/tools/census_ratchet.py --update)')
                continue
            for key in ('errors', 'missing_types'):
                if now[key] > was[key]:
                    failures.append(
                        f'{t} {tu}: {key} REGRESSED {was[key]} -> {now[key]}')
                elif now[key] < was[key]:
                    failures.append(
                        f'{t} {tu}: {key} improved {was[key]} -> {now[key]} '
                        f'(good -- re-baseline with: '
                        f'port/tools/census_ratchet.py --update)')

    if failures:
        print('\nCENSUS RATCHET:')
        for f in failures:
            print('  ', f)
        return 1
    print('\nOK: census matches baseline on both targets')
    return 0


if __name__ == '__main__':
    sys.exit(main())
