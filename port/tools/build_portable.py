#!/usr/bin/env python3
"""
Build linscp's portable Windows package: the deploy folder, the licenses,
and an empty linscp.ini beside linscp.exe, which keeps the configuration
there (WinSCP's portable mode: an INI file beside the executable is used;
linscp-cli shares it). CMake's package-portable target runs it.

  build_portable.py --version 0.7.1 --deploy build/deploy --out build/packages
                    --license FILE|NAME ...

Writes linscp-<version>-portable.zip (its files in a linscp-<version>
folder) and prints its path.

The archive is reproducible: entries in a fixed order, permissions reduced
to 0644/0755, and, when SOURCE_DATE_EPOCH is set (cmake/reproducible.cmake
sets it for the package targets), every entry dated by it rather than by its
file's modification time.
"""
import argparse, os, stat, sys, time, zipfile


def entry(name, date_time, executable=False):
    info = zipfile.ZipInfo(name.replace(os.sep, '/'), date_time)
    info.compress_type = zipfile.ZIP_DEFLATED
    info.create_system = 3  # Unix, so external_attr carries the mode
    info.external_attr = ((0o755 if executable else 0o644) | stat.S_IFREG) << 16
    return info


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
    epoch = os.environ.get('SOURCE_DATE_EPOCH')
    # ZIP dates start in 1980 and have no time zone: UTC, so the archive does
    # not depend on the build machine's.
    fixed = time.gmtime(max(int(epoch), 315532800))[:6] if epoch else None

    def add(z, source, name):
        st = os.stat(source)
        date_time = fixed or time.localtime(st.st_mtime)[:6]
        with open(source, 'rb') as f:
            z.writestr(entry(name, date_time, bool(st.st_mode & stat.S_IXUSR)), f.read(),
                       compresslevel=9)

    files = []
    for directory, dirs, names in os.walk(a.deploy):
        dirs.sort()
        files += [os.path.join(directory, name) for name in sorted(names)]
    with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for path in files:
            add(z, path, os.path.join(top, os.path.relpath(path, a.deploy)))
        for license_entry in a.license:
            source, name = license_entry.split('|')
            add(z, source, os.path.join(top, 'licenses', name))
        # Empty: core writes the configuration into it.
        z.writestr(entry(os.path.join(top, 'linscp.ini'), fixed or time.localtime()[:6]), '')
    print(target)
    return 0


if __name__ == '__main__':
    sys.exit(main())
