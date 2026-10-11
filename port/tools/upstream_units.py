#!/usr/bin/env python3
"""
The translation units an upstream project builds, read from its .cbproj
(ScpCore.cbproj, core, by default).

Shared by the census and CMake, so both always compile exactly the set
upstream does: a unit upstream adds or drops changes both without an edit.

  upstream_units.py [--project P.cbproj]          one source/... per line
  upstream_units.py --separator ';'               one line, for CMake lists
"""
import argparse, os, re, sys

CBPROJ = '3rd-party/winscp/source/ScpCore.cbproj'


def project_units(path=CBPROJ):
    """Every .c/.cpp a project builds, as source/<path> with forward slashes.

    A .cbproj (C++Builder) or a .vcxproj (MSVC; WinSCP builds PuTTY's hot
    crypto loops that way, in libs/puttyvs)."""
    text = open(path, encoding='utf-8', errors='replace').read()
    if path.endswith('.vcxproj'):
        here = os.path.dirname(os.path.abspath(path))
        root = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                            '..', '..', '3rd-party', 'winscp'))
        units = []
        for m in re.findall(r'ClCompile Include="([^"]+\.(?:c|cpp))"', text):
            full = os.path.normpath(os.path.join(here, m.replace('\\', '/')))
            units.append(os.path.relpath(full, root).replace(os.sep, '/'))
        if not units:
            sys.exit(f'{path}: no units found')
        return sorted(units)
    units = sorted(set(m.replace('\\', '/') for m in
                       re.findall(r'Include="([^"]+\.(?:c|cpp))"', text)))
    if not units:
        sys.exit(f'{path}: no units found -- has the project layout changed?')
    return [f'source/{u}' for u in units]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--project', default=CBPROJ, help='a .cbproj under the submodule')
    ap.add_argument('--separator', default='\n')
    a = ap.parse_args()
    print(a.separator.join(project_units(a.project)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
