#!/usr/bin/env python3
"""
Keep port/casefold in step with upstream's case-mismatched includes.

A mismatch is an #include whose spelling differs only in case from a file in
the *including file's own directory* -- what a case-insensitive file system
would have resolved. A basename match anywhere is not enough: putty/windows/
gss.c includes `security.h`, the Windows SDK's header, not core's Security.h.

Fails when:
  - upstream has a mismatch with no forwarding header in port/casefold
  - a forwarding header in port/casefold is no longer needed
  - a forwarding header no longer reaches an existing file
"""
import os, re, sys

UPSTREAM = '3rd-party/winscp/source'
SCOPE = ['core', 'filezilla', 'putty']       # the trees the port compiles
CASEFOLD = 'port/casefold'
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')
TARGET = re.compile(r'^\s*#\s*include\s*"([^"]+)"')


def mismatches():
    found = {}
    for sub in SCOPE:
        for dirpath, _, names in os.walk(os.path.join(UPSTREAM, sub)):
            here = set(names)
            folded = {n.lower(): n for n in names}
            for name in names:
                if not name.lower().endswith(('.c', '.cpp', '.h', '.hpp')):
                    continue
                path = os.path.join(dirpath, name)
                for line in open(path, encoding='latin-1'):
                    m = INCLUDE.match(line)
                    if not m:
                        continue
                    spelled = m.group(1).replace('\\', '/')
                    if '/' in spelled:
                        continue
                    real = folded.get(spelled.lower())
                    if real and (spelled not in here):
                        found[spelled] = os.path.join(dirpath, real)
    return found


def main():
    failures = []
    wanted = mismatches()
    present = {n for n in os.listdir(CASEFOLD) if n != 'README.md'}

    for spelled, real in sorted(wanted.items()):
        if spelled not in present:
            failures.append(f'{spelled}: upstream includes it, but {real} has '
                            f'different case and port/casefold has no forwarder')
    for name in sorted(present - set(wanted)):
        failures.append(f'{name}: no longer needed -- upstream fixed the spelling')
    for name in sorted(present & set(wanted)):
        text = open(os.path.join(CASEFOLD, name), encoding='utf-8').read()
        targets = [m.group(1) for m in map(TARGET.match, text.splitlines()) if m]
        if len(targets) != 1:
            failures.append(f'{name}: expected exactly one forwarding #include')
            continue
        target = os.path.normpath(os.path.join(CASEFOLD, targets[0]))
        if not os.path.isfile(target) or \
           os.path.normpath(target) != os.path.normpath(wanted[name]):
            failures.append(f'{name}: forwards to {target}, expected {wanted[name]}')

    print(f'case-mismatched includes: {len(wanted)}; forwarders: {len(present)}')
    if failures:
        print('\nINCLUDE CASE:')
        for f in failures:
            print('  ', f)
        return 1
    print('OK: every case-mismatched include has exactly its forwarder')
    return 0


if __name__ == '__main__':
    sys.exit(main())
