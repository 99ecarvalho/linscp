#!/usr/bin/env python3
"""
A release program must not carry the absolute paths it was built from.

  check_build_paths.py --path DIR [--path DIR ...] program ...

cmake/reproducible.cmake maps the source, build and generated directories to
relative names (-ffile-prefix-map) and makes the build-tree RUNPATH
$ORIGIN-relative; a path that still shows up in a binary makes it differ
between two checkouts and gives away the build machine's layout. Each path is
looked for as UTF-8 and as UTF-16LE (wide string literals: Windows', and
core's under -fshort-wchar). Prints each hit with its offset and the text
around it, and fails if there is any.
"""
import argparse, sys


def context(data, at, width=120):
    start = max(0, data.rfind(b'\0', max(0, at - width), at) + 1)
    end = data.find(b'\0', at)
    end = at + width if end < 0 or end > at + width else end
    return data[start:end].decode('utf-8', 'replace')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--path', action='append', required=True)
    ap.add_argument('programs', nargs='+')
    a = ap.parse_args()
    # Too short to be distinctive ("/" or "/b") would match anything.
    paths = sorted({p.rstrip('/\\') for p in a.path if len(p.rstrip('/\\')) > 3})
    failed = False
    for program in a.programs:
        with open(program, 'rb') as f:
            data = f.read()
        for path in paths:
            for encoding in ('utf-8', 'utf-16-le'):
                needle = path.encode(encoding)
                at, hits = data.find(needle), 0
                while at >= 0:
                    hits += 1
                    if hits <= 10:
                        shown = context(data, at) if encoding == 'utf-8' else data[at:at + 2 * 80].decode(encoding, 'replace')
                        print(f'{program}: {encoding} {path!r} at 0x{at:x}: {shown}')
                    at = data.find(needle, at + 1)
                if hits:
                    failed = True
                    print(f'{program}: {hits} occurrence(s) of {path} ({encoding})')
    if not failed:
        print(f'no build paths in {len(a.programs)} program(s)')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
