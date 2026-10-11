#!/usr/bin/env python3
"""
The core translation units upstream builds, read from its ScpCore.cbproj.

Shared by the census and CMake, so both always compile exactly the set
upstream does: a unit upstream adds or drops changes both without an edit.

  upstream_units.py              one source/core/X.cpp per line
  upstream_units.py --separator ';'   one line, for CMake lists
"""
import argparse, re, sys

CBPROJ = '3rd-party/winscp/source/ScpCore.cbproj'


def project_units(path=CBPROJ):
    text = open(path, encoding='utf-8', errors='replace').read()
    units = sorted(set(re.findall(r'Include="core\\([^"\\]+\.cpp)"', text)))
    if not units:
        sys.exit(f'{path}: no core units found -- has the project layout changed?')
    return [f'source/core/{u}' for u in units]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--separator', default='\n')
    a = ap.parse_args()
    print(a.separator.join(project_units()))
    return 0


if __name__ == '__main__':
    sys.exit(main())
