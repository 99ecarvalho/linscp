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

    A .cbproj (C++Builder), a .vcxproj (MSVC; WinSCP builds PuTTY's hot
    crypto loops that way, in libs/puttyvs), or a Borland make file: every
    `$(OBJ_D)\\x.obj` it names, compiled from the source its explicit rule
    gives (neon's Makefile.bcb) or else from x.c in its `.PATH.c` directory
    (libs3's Makefile)."""
    text = open(path, encoding='utf-8', errors='replace').read()
    here = os.path.dirname(os.path.abspath(path))
    root = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                        '..', '..', '3rd-party', 'winscp'))
    if is_makefile(path):
        return make_units(path, text, here, root)
    if path.endswith('.vcxproj'):
        units = []
        for m in re.findall(r'ClCompile Include="([^"]+\.(?:c|cpp))"', text):
            full = os.path.normpath(os.path.join(here, m.replace('\\', '/')))
            units.append(os.path.relpath(full, root).replace(os.sep, '/'))
        if not units:
            sys.exit(f'{path}: no units found')
        return sorted(units)
    # In the project's order, which is the order C++Builder links the units
    # and so runs their static initialisers in; core depends on it (a global
    # in HierarchicalStorage.cpp is built from one in Common.cpp).
    units = list(dict.fromkeys(m.replace('\\', '/') for m in
                               re.findall(r'Include="([^"]+\.(?:c|cpp))"', text)))
    if not units:
        sys.exit(f'{path}: no units found -- has the project layout changed?')
    return [f'source/{u}' for u in units]


def is_makefile(path):
    return path.endswith(('.bcb', '.mak')) or os.path.basename(path) == 'Makefile'


# An object as WinSCP's Borland make files name one: `$(OBJ_D)\\x.obj`,
# `$(OBJ_D)\\x.$(O)`, or a bare `x.$(O)` (the MFC subset's borland.mak).
_OBJECT = re.compile(r'(?:\$\(OBJ_D\)\\)?\b(\w+)\.(?:obj|\$\(O\))(?![\w:])')


def make_units(path, text, here, root):
    """Every object a make file builds, each from the source its explicit
    rule gives, else x.c in its `.path.c` directory, else x.cpp or x.c next
    to the make file."""
    rules = dict(re.findall(
        r'^\$\(OBJ_D\)\\(\w+)\.(?:obj|\$\(O\)):\s*\$\(SRC_D\)[/\\](\S+\.c)\b', text, re.M))
    v = _make_vars(text)
    search = re.search(r'^\.path\.c\s*=\s*(\S+)', text, re.M | re.I)
    search = (re.sub(r'\$\((\w+)\)', lambda m: v.get(m.group(1), ''), search.group(1))
              if search else None)
    names = set()
    for line in text.split('\n'):
        # Inference rules and search paths (.c.$(O):, .path.$(O) = ...) name
        # suffixes, not objects; comments name anything.
        if line.lstrip().startswith(('.', '#')):
            continue
        names.update(_OBJECT.findall(line))
    units = []
    for name in sorted(names):
        candidates = []
        if name in rules:
            candidates.append(rules[name])
        if search:
            candidates.append(f'{search}/{name}.c')
        candidates += [f'{name}.cpp', f'{name}.c']
        for src in candidates:
            full = os.path.normpath(os.path.join(here, src.replace('\\', '/')))
            if os.path.isfile(full):
                units.append(os.path.relpath(full, root).replace(os.sep, '/'))
                break
        else:
            sys.exit(f'{path}: no source found for object {name}')
    if not units:
        sys.exit(f'{path}: no objects found')
    return sorted(units)


def _make_vars(text):
    """NAME=value assignments, continuation lines joined."""
    text = re.sub(r'\\\r?\n', ' ', text)
    out = {}
    for m in re.finditer(r'^([A-Za-z_]\w*)\s*=\s*(.*)$', text, re.M):
        out.setdefault(m.group(1), m.group(2).strip())
    return out


def make_defines(path):
    """The preprocessor defines a make file compiles with: its DEFINES (or
    ALLDEFINES, or the MFC subset's DEFS), plus the -D flags of the shared
    Makefile.inc's CFLAG_OPENSSL when the make file uses it."""
    text = open(path, encoding='utf-8', errors='replace').read()
    v = _make_vars(text)
    expand = lambda s: re.sub(r'\$\((\w+)\)', lambda m: v.get(m.group(1), ''), s)
    value = next((v[k] for k in ('ALLDEFINES', 'DEFINES', 'DEFS') if k in v), None)
    if value is None:
        sys.exit(f'{path}: no DEFINES, ALLDEFINES or DEFS')
    defines = [d for d in expand(value).split(';') if d.strip()]
    if 'CFLAG_OPENSSL' in text:
        inc = os.path.join(os.path.dirname(os.path.abspath(path)), '..', 'Makefile.inc')
        if not os.path.isfile(inc):
            inc = os.path.join(os.path.dirname(os.path.abspath(path)), '..', '..', 'Makefile.inc')
        # Only unconditional assignments: the !ifdef WIN64 branch adds _WIN64,
        # which the compiler defines itself for that target and must not be
        # defined for any other.
        depth = 0
        for line in open(inc, encoding='utf-8', errors='replace'):
            word = line.strip().split(None, 1)[0] if line.strip() else ''
            if word.startswith('!if'):
                depth += 1
            elif word == '!endif':
                depth -= 1
            elif depth == 0:
                m = re.match(r'CFLAG_OPENSSL\s*=(.*)$', line.strip())
                if m:
                    defines += re.findall(r'-D(\S+)', m.group(1))
    seen = []
    for d in defines:
        d = d.strip()
        if d not in seen:
            seen.append(d)
    return seen


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--project', default=CBPROJ, help='a .cbproj under the submodule')
    ap.add_argument('--separator', default='\n')
    ap.add_argument('--defines', action='store_true',
                    help='a make file\'s preprocessor defines instead of its units')
    a = ap.parse_args()
    if a.defines:
        print(a.separator.join(make_defines(a.project)))
        return 0
    print(a.separator.join(project_units(a.project)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
