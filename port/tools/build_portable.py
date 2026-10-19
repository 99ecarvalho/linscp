#!/usr/bin/env python3
"""
Build linscp's portable Windows package: the deploy folder, the licenses,
and an empty linscp.ini beside linscp.exe, which keeps the configuration
there (WinSCP's portable mode: an INI file beside the executable is used;
linscp-cli shares it). CMake's package-portable target runs it.

  build_portable.py --version 0.7.0 --deploy build/deploy --out build/packages
                    --license FILE|NAME ...

Writes linscp-<version>-portable.zip (its files in a linscp-<version>
folder) and prints its path.
"""
import argparse, os, sys, zipfile


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--version', required=True)
    ap.add_argument('--deploy', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--license', action='append', default=[])
    a = ap.parse_args()
    top = f'linscp-{a.version}'
    os.makedirs(a.out, exist_ok=True)
    target = os.path.join(os.path.abspath(a.out), f'{top}-portable.zip')
    with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for directory, _, files in os.walk(a.deploy):
            for name in sorted(files):
                path = os.path.join(directory, name)
                z.write(path, os.path.join(top, os.path.relpath(path, a.deploy)))
        for entry in a.license:
            source, name = entry.split('|')
            z.write(source, os.path.join(top, 'licenses', name))
        # Empty: core writes the configuration into it.
        z.writestr(os.path.join(top, 'linscp.ini'), '')
    print(target)
    return 0


if __name__ == '__main__':
    sys.exit(main())
