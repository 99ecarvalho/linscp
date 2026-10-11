#!/usr/bin/env python3
"""
The preprocessor configuration upstream compiles core with.

Read from the pristine ScpCore.cbproj rather than copied, so it follows
upstream: a define added there reaches every compile here without an edit.
Before this existed the census compiled core with none of them -- not even
WINSCP, which PuTTY and the other vendored libraries test to drop the parts
WinSCP replaces. Every count was measured in a configuration upstream never
builds.

The project's own defines are then filtered per target, each for a reason:

  WINDOWS_ONLY  select Windows code paths, so they are wrong on POSIX
  DROPPED       meaningful only to C++Builder's own runtime

  upstream_defines.py --target TRIPLE [--config Debug|Release|Base]
      prints one -DNAME[=VALUE] per line
"""
import argparse, os, sys
import xml.etree.ElementTree as ET

CBPROJ = '3rd-party/winscp/source/ScpCore.cbproj'

WINDOWS_ONLY = {'_WINDOWS', 'WIN32', 'WINNT'}
DROPPED = {
    # C++Builder's CRT linkage. mingw's headers honour a pre-set _CRTIMP, so
    # defining it empty would strip dllimport from msvcrt's data symbols.
    '_CRTIMP',
}


def is_windows(target):
    # Accepts a clang triple or CMake's CMAKE_SYSTEM_NAME ("Windows").
    t = target.lower()
    return 'mingw' in t or 'windows' in t


def _strip_ns(tag):
    return tag.split('}', 1)[-1]


def _groups(root):
    """[(Condition, {child tag: text})] for every PropertyGroup."""
    out = []
    for g in root.iter():
        if _strip_ns(g.tag) != 'PropertyGroup':
            continue
        children = {_strip_ns(c.tag): (c.text or '') for c in g}
        out.append((g.get('Condition', ''), children))
    return out


def _split(value):
    # MSBuild variables such as $(Defines) chain the groups together; the
    # chaining is what we reproduce by hand, so the references themselves go.
    return [d for d in value.split(';') if d and not d.startswith('$(')]


def project_defines(config, path=CBPROJ):
    """Base defines plus the given configuration's, in project order.

    config 'Base' returns only the configuration-independent part; CMake adds
    _DEBUG or NDEBUG per build type itself."""
    groups = _groups(ET.parse(path).getroot())

    base = [c['Defines'] for cond, c in groups
            if cond.strip() == "'$(Base)'!=''" and 'Defines' in c]
    if len(base) != 1:
        sys.exit(f'{path}: expected one base <Defines>, found {len(base)}')
    result = _split(base[0])

    if config != 'Base':
        # Which Cfg_N flag stands for the requested configuration.
        flag = None
        for cond, children in groups:
            if f"'$(Config)'=='{config}'" in cond:
                flag = next((k for k in children if k.startswith('Cfg_')), None)
        if flag is None:
            sys.exit(f'{path}: no configuration named {config!r}')
        cfg = [c['Defines'] for cond, c in groups
               if cond.strip() == f"'$({flag})'!=''" and 'Defines' in c]
        if len(cfg) != 1:
            sys.exit(f'{path}: expected one {config} <Defines>, found {len(cfg)}')
        result = _split(cfg[0]) + result

    # C++Builder's "_TCHAR maps to wchar_t" option is not in <Defines>, but is
    # what defines UNICODE and _UNICODE. Without them Win32's TEXT() yields
    # narrow strings and core's DoAssert(const wchar_t *, ...) stops matching.
    mapping = [c['_TCHARMapping'] for cond, c in groups
               if cond.strip() == "'$(Base)'!=''" and '_TCHARMapping' in c]
    if mapping == ['wchar_t']:
        result += ['UNICODE', '_UNICODE']
    return result


def defines(target, config='Debug', path=CBPROJ):
    out = []
    for d in project_defines(config, path):
        name = d.split('=', 1)[0]
        if name in DROPPED:
            continue
        if name in WINDOWS_ONLY and not is_windows(target):
            continue
        out.append(d)
    return out


def flags(target, config='Debug', path=CBPROJ):
    return ['-D' + d for d in defines(target, config, path)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--target', required=True)
    ap.add_argument('--config', default='Debug', choices=['Debug', 'Release', 'Base'])
    a = ap.parse_args()
    for f in flags(a.target, a.config):
        print(f)
    return 0


if __name__ == '__main__':
    sys.exit(main())
