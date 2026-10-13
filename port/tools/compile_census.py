#!/usr/bin/env python3
"""
Compile a real core translation unit and classify what the shim is still missing.

This is the measurement P0-09 exists for. Rather than estimate the shim's scope
from grep counts, compile actual upstream code against it and let the compiler
enumerate the gap, deduplicated and ranked.

Targets Windows (mingw) by default: core's Win32 includes are satisfied by the
real mingw headers there, so every remaining diagnostic is a genuine VCL gap
rather than a platform one.

  compile_census.py [--tu source/core/Common.cpp] [--target TRIPLE] [--show N]
"""
import argparse, collections, glob, os, re, subprocess, sys


def _upstream_defines():
    # Loaded by path: under python -I the script's own directory is not on
    # sys.path, so a plain import of a sibling module would fail.
    import importlib.util
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        'upstream_defines.py')
    spec = importlib.util.spec_from_file_location('upstream_defines', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

# clang diagnostic shapes, mapped to what each implies the shim must supply.
PATTERNS = [
    ('missing type',       re.compile(r"unknown type name '([^']+)'")),
    ('missing type',       re.compile(r"no type named '([^']+)'")),
    ('missing type',       re.compile(r"implicit instantiation of undefined template '([^']+)'")),
    ('missing identifier', re.compile(r"use of undeclared identifier '([^']+)'")),
    ('missing identifier', re.compile(r"no member named '([^']+)' in")),
    ('missing header',     re.compile(r"'([^']+)' file not found")),
    ('incomplete type',    re.compile(r"variable has incomplete type '([^']+)'")),
    ('incomplete type',    re.compile(r"member access into incomplete type '([^']+)'")),
    ('not a class',        re.compile(r"member reference base type '([^']+)'")),
    ('bad call',           re.compile(r"too (?:few|many) arguments to function call")),
    ('no viable',          re.compile(r"no viable conversion from '([^']+)'")),
    ('no viable',          re.compile(r"no matching (?:function|constructor) for call to '([^']+)'")),
]

def _compiler(target):
    """The Windows build's own compiler for its target -- llvm-mingw's clang++,
    with its sysroot and libc++ (ADR 0011) -- when it is there; else the host
    clang for the target, which needs a mingw-w64 sysroot with C++ headers."""
    if 'mingw' in target:
        roots = [os.environ.get('LINSCP_LLVM_MINGW', '')] + sorted(
            d for d in glob.glob('build/tools/llvm-mingw-*-ucrt-*') if os.path.isdir(d))[::-1]
        for root in roots:
            exe = os.path.join(root, 'bin', f'{target}-clang++') if root else ''
            if exe and os.path.exists(exe):
                return [exe]
    return ['clang++', f'--target={target}']


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tu', default='source/core/Common.cpp')
    ap.add_argument('--gen', default='build/gen')
    ap.add_argument('--target', default='x86_64-w64-mingw32')
    ap.add_argument('--show', type=int, default=25)
    ap.add_argument('--max-errors', default='0')
    # Empty to skip: the ratchet runs many of these at once.
    ap.add_argument('--report', default='build/compile_census.txt')
    ap.add_argument('--raw', action='store_true',
                    help="print clang's diagnostics instead of the summary")
    a = ap.parse_args()

    tu = os.path.join(a.gen, a.tu)
    if not os.path.exists(tu):
        sys.exit(f'missing {tu} - run transform.py first')

    up = '3rd-party/winscp/source'
    cmd = [
        *_compiler(a.target), '-std=c++17', '-fsyntax-only',
        '-fms-extensions', '-fshort-wchar', f'-ferror-limit={a.max_errors}',
        '-I', 'port/shim',

        # Everything the transform covers comes from the generated tree, so a
        # unit sees one consistent set of rewritten headers.
        '-I', os.path.join(a.gen, 'source/core'),
        '-I', os.path.join(up, 'resource'),
        '-I', os.path.join(up, 'windows'),
        '-I', os.path.join(a.gen, 'source/filezilla'),
        '-I', os.path.join(a.gen, 'source/putty'),
        # putty/windows supplies platform.h, which core reaches through
        # PuttyIntf.h. That is PuTTY's *Windows* platform layer, so on Linux
        # it accounts for much of the remaining excess; see P1-09.
        '-I', os.path.join(a.gen, 'source/putty/windows'),
        '-I', os.path.join(up, 'components'),
        '-I', '3rd-party/winscp/libs/openssl/include',
        '-I', '3rd-party/winscp/libs/neon/src',
        '-I', '3rd-party/winscp/libs/expat/lib',
        '-I', '3rd-party/winscp/libs/libs3/inc',
        '-Wno-everything',
    ] + _upstream_defines().flags(a.target) + [
        tu,
    ]
    # port/platform holds POSIX stand-ins for Win32 headers. Offering them to a
    # Windows target would shadow the real mingw headers, which looked like
    # progress the first time it happened.
    if 'windows' not in a.target and 'mingw' not in a.target:
        cmd[1:1] = ['-I', 'port/platform']

    r = subprocess.run(cmd, capture_output=True, text=True)
    err = r.stderr
    if a.raw:
        print(err, end='')
        return 0 if r.returncode == 0 else 1

    # Count fatal errors too. A missing header produces only a *fatal* error and
    # aborts the parse, so filtering on ': error:' alone reported such a unit as
    # having zero problems -- the most misleading result possible.
    errors = [l for l in err.split('\n')
              if ': error:' in l or ': fatal error:' in l]
    fatal = [l for l in err.split('\n') if ': fatal error:' in l]
    found = collections.defaultdict(collections.Counter)
    unmatched = []
    for line in errors:
        for kind, pat in PATTERNS:
            m = pat.search(line)
            if m:
                found[kind][m.group(1) if m.groups() else '<no symbol>'] += 1
                break
        else:
            unmatched.append(line)

    print(f'translation unit : {a.tu}')
    print(f'target           : {a.target}')
    print(f'clang exit        : {r.returncode}')
    print(f'total errors      : {len(errors)}')
    if fatal:
        print(f'ABORTED EARLY     : {len(fatal)} fatal error(s) -- the parse '
              f'stopped, so this count is a floor, not a total')
        for l in fatal[:3]:
            print('   ', l.split('fatal error:')[-1].strip()[:90])
    print()
    for kind in dict.fromkeys(k for k, _ in PATTERNS):
        if kind not in found:
            continue
        c = found[kind]
        print(f'--- {kind}: {len(c)} distinct ---')
        for name, n in c.most_common(a.show):
            print(f'  {n:5}  {name}')
        if len(c) > a.show:
            print(f'  ... and {len(c) - a.show} more')
        print()
    if unmatched:
        print(f'--- unclassified diagnostics: {len(unmatched)} ---')
        for l in unmatched[:8]:
            print('  ', l.split('error:')[-1].strip()[:100])

    if fatal and not found:
        print('\n(no classified diagnostics: compilation never got past the '
              'fatal error above)')

    os.makedirs('build', exist_ok=True)
    if a.report:
        with open(a.report, 'w') as f:
            for kind, c in found.items():
                for name, n in c.most_common():
                    f.write(f'{n}\t{kind}\t{name}\n')
        print(f'\n-> {a.report}')
    return 0

if __name__ == '__main__':
    sys.exit(main())
