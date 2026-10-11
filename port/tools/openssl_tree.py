#!/usr/bin/env python3
"""
Assemble the OpenSSL source tree the build configures.

WinSCP vendors a trimmed OpenSSL, pre-configured for 32-bit Windows with x86
assembly and without the C fallbacks for that assembly or any Unix source, so
it cannot be built for x86-64 or Linux. The port builds the pristine release
of the same version instead (3rd-party/openssl), with OpenSSL's own Configure.

WinSCP's copy is not identical to the release, though: it carries C++Builder
workarounds and security backports. port/openssl/winscp-changes.txt lists
every difference and whether the port adopts it. This tool

  openssl_tree.py --out DIR   mirrors the release into DIR, then overlays the
                              adopted files from WinSCP's copy; unchanged
                              files keep their mtimes, so rebuilds stay small
  openssl_tree.py --check     fails if the two trees differ anywhere the list
                              does not account for, or a listed file no longer
                              differs
"""
import argparse, os, shutil, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RELEASE = os.path.join(ROOT, '3rd-party/openssl')
WINSCP = os.path.join(ROOT, '3rd-party/winscp/libs/openssl')
CHANGES = os.path.join(ROOT, 'port/openssl/winscp-changes.txt')
# Only sources can differ meaningfully; WinSCP's makefile and its generated
# headers have no counterpart in the release.
SOURCE_EXTS = ('.c', '.h', '.inc')


def load_changes(path=CHANGES):
    changes = {}
    for n, line in enumerate(open(path, encoding='utf-8'), 1):
        line = line.split('#', 1)[0].strip()
        if not line:
            continue
        parts = line.split(None, 2)
        if len(parts) < 3 or parts[1] not in ('adopt', 'skip'):
            sys.exit(f'{path}:{n}: expected "file adopt|skip reason"')
        changes[parts[0]] = parts[1]
    return changes


def read_lf(path):
    return open(path, 'rb').read().replace(b'\r\n', b'\n')


def differing(release=RELEASE, winscp=WINSCP):
    """Files present in both trees whose content differs, ignoring CRLF."""
    out = []
    for dirpath, _, names in os.walk(winscp):
        for n in names:
            if not n.endswith(SOURCE_EXTS):
                continue
            rel = os.path.relpath(os.path.join(dirpath, n), winscp).replace(os.sep, '/')
            theirs = os.path.join(release, rel)
            if os.path.isfile(theirs) and read_lf(os.path.join(winscp, rel)) != read_lf(theirs):
                out.append(rel)
    return sorted(out)


def check(changes, release=RELEASE, winscp=WINSCP):
    found = set(differing(release, winscp))
    problems = [f'{f}: differs from the release but is not in {os.path.relpath(CHANGES, ROOT)}'
                for f in sorted(found - set(changes))]
    problems += [f'{f}: listed, but no longer differs from the release'
                 for f in sorted(set(changes) - found)]
    return problems


def write_if_changed(path, data):
    if os.path.isfile(path) and open(path, 'rb').read() == data:
        return False
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(data)
    return True


def assemble(out, changes, release=RELEASE, winscp=WINSCP):
    expected = set()
    written = 0
    for dirpath, dirnames, names in os.walk(release):
        # The release's own submodules (fuzz corpora, test suites) are not
        # needed to build the libraries.
        dirnames[:] = [d for d in dirnames if d != '.git']
        for n in names:
            if n == '.git':
                continue
            src = os.path.join(dirpath, n)
            rel = os.path.relpath(src, release).replace(os.sep, '/')
            expected.add(rel)
            data = (read_lf(os.path.join(winscp, rel)) if changes.get(rel) == 'adopt'
                    else open(src, 'rb').read())
            dst = os.path.join(out, rel)
            if write_if_changed(dst, data):
                shutil.copymode(src, dst)
                written += 1
    return written, len(expected)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out')
    ap.add_argument('--check', action='store_true')
    a = ap.parse_args()
    changes = load_changes()
    problems = check(changes)
    if problems:
        print('OpenSSL: WinSCP\'s copy and the pinned release disagree:')
        for p in problems:
            print('  ', p)
        return 1
    if a.check:
        adopted = sum(1 for v in changes.values() if v == 'adopt')
        print(f'OK: {len(changes)} WinSCP changes accounted for, {adopted} adopted')
    if a.out:
        written, total = assemble(a.out, changes)
        print(f'OpenSSL tree: {total} files, {written} written')
    return 0


if __name__ == '__main__':
    sys.exit(main())
