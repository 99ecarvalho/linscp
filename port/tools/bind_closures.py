#!/usr/bin/env python3
"""
Discover the call sites where core names a bare member function.

Borland implicitly binds `this` when a method name is passed where a __closure
is expected, as in

    FTerminal->OnQueryUser = TerminalQueryUser;
    Synchronize(DistributeProgressUpdate);

That is not valid C++ at all -- the name is neither a call nor a
pointer-to-member -- so these sites must be rewritten rather than merely
retyped. The CLOSURE macro recovers the enclosing class from decltype(*this),
so only the method name is needed, which is all the original source gives us.

Rather than infer which arguments are closures by regex, the compiler is asked:
each site produces a diagnostic naming an exact file, line and column. This
tool records them; transform.py applies the list deterministically on every
run, and fails loudly if a recorded site no longer holds the expected
identifier, which is what an upstream change would look like.

  bind_closures.py --discover   recompile and rewrite port/patches/closure_sites.json
  bind_closures.py              verify the recorded sites still match the source
"""
import argparse, json, os, re, subprocess, sys

SITES = 'port/patches/closure_sites.json'
DIAG = re.compile(
    r'^(?P<file>[^:]+):(?P<line>\d+):(?P<col>\d+): error: '
    r'reference to non-static member function must be called')
# When the offending token sits inside a macro body, clang reports the error at
# the *expansion* point and then points at the definition with one of these per
# level of nesting. The identifier we must wrap lives in the definition, not at
# the call, so a single macro accounts for every one of its call sites: the
# ADSTR macro in SessionInfo.cpp alone produced 61 duplicate reports.
EXPANDED = re.compile(
    r'^(?P<file>[^:]+):(?P<line>\d+):(?P<col>\d+): note: '
    r"expanded from macro '(?P<macro>\w+)'")
IDENT = re.compile(r'[A-Za-z_]\w*')

INCLUDES = [
    'port/shim', 'port/compat', 'build/gen/source/core',
    '3rd-party/winscp/source/resource', '3rd-party/winscp/source/windows',
    '3rd-party/winscp/source/filezilla', '3rd-party/winscp/source/putty',
    '3rd-party/winscp/source/putty/windows',
    '3rd-party/winscp/libs/openssl/include', '3rd-party/winscp/libs/neon/src',
    '3rd-party/winscp/libs/expat/lib',
]


def compile_tu(path, target='x86_64-w64-mingw32'):
    cmd = ['clang++', f'--target={target}', '-std=c++17', '-fsyntax-only',
           '-fms-extensions', '-fshort-wchar', '-ferror-limit=0',
           # One line per diagnostic. Without this clang interleaves the source
           # echo and a caret line, which breaks the scan for the 'expanded
           # from macro' notes that follow an error.
           '-fno-caret-diagnostics', '-fmacro-backtrace-limit=0',
           '-Wno-everything']
    for inc in INCLUDES:
        cmd += ['-I', inc]
    cmd.append(path)
    return subprocess.run(cmd, capture_output=True, text=True).stderr


def discover(gen):
    """Resolve each diagnostic to the place the identifier actually appears."""
    found = {}
    core = os.path.join(gen, 'source/core')
    for name in sorted(os.listdir(core)):
        if not name.endswith('.cpp'):
            continue
        lines = compile_tu(os.path.join(core, name)).split('\n')
        i = 0
        while i < len(lines):
            m = DIAG.match(lines[i])
            if not m:
                i += 1
                continue
            # Collect the expansion chain following this error.
            chain = [(m.group('file'), int(m.group('line')), int(m.group('col')))]
            macros = set()
            j = i + 1
            while j < len(lines):
                n = EXPANDED.match(lines[j])
                if not n:
                    break
                chain.append((n.group('file'), int(n.group('line')),
                              int(n.group('col'))))
                macros.add(n.group('macro'))
                j += 1

            # The right target is the first location in the chain holding an
            # identifier that is not itself one of the macro names: at a macro
            # call site the token is the macro's own name, while in the
            # definition it is the member function we need to bind.
            chosen = None
            for path, lineno, col in chain:
                rel = os.path.relpath(path, gen)
                ident = identifier_at(gen, rel, lineno, col)
                if ident is not None and ident not in macros:
                    chosen = (rel, lineno, col)
                    break
            if chosen is not None:
                found[chosen] = None
            i = j
    return sorted(found)


def identifier_at(gen, path, lineno, col):
    """The identifier beginning at a 1-based line and column, or None."""
    full = os.path.join(gen, path)
    if not os.path.exists(full):
        return None
    lines = open(full, newline='', encoding='utf-8',
                 errors='surrogateescape').read().split('\n')
    if lineno < 1 or lineno > len(lines):
        return None
    text = lines[lineno - 1]
    if col < 1 or col > len(text):
        return None
    m = IDENT.match(text[col - 1:])
    return m.group(0) if m else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--gen', default='build/gen')
    ap.add_argument('--discover', action='store_true')
    a = ap.parse_args()

    if a.discover:
        sites = []
        unresolved = []
        for path, lineno, col in discover(a.gen):
            ident = identifier_at(a.gen, path, lineno, col)
            if ident is None:
                unresolved.append(f'{path}:{lineno}:{col}')
                continue
            sites.append({'file': path, 'line': lineno, 'column': col,
                          'identifier': ident})
        os.makedirs(os.path.dirname(SITES), exist_ok=True)
        json.dump({'sites': sites}, open(SITES, 'w'), indent=2)
        open(SITES, 'a').write('\n')
        print(f'discovered {len(sites)} closure call sites -> {SITES}')
        by_file = {}
        for s in sites:
            by_file[s['file']] = by_file.get(s['file'], 0) + 1
        for f, n in sorted(by_file.items(), key=lambda x: -x[1]):
            print(f'  {n:4}  {f}')
        if unresolved:
            print(f'\nUNRESOLVED positions ({len(unresolved)}):')
            for u in unresolved[:10]:
                print(f'   {u}')
            return 1
        return 0

    if not os.path.exists(SITES):
        sys.exit(f'no site list; run: {sys.argv[0]} --discover')
    sites = json.load(open(SITES))['sites']
    bad = []
    applied = 0
    for s in sites:
        ident = identifier_at(a.gen, s['file'], s['line'], s['column'])
        if ident == s['identifier']:
            continue          # not yet bound
        if ident == 'CLOSURE':
            # Already bound. Confirm it is this identifier that was wrapped,
            # so a different nearby rewrite cannot pass as this one.
            full = os.path.join(a.gen, s['file'])
            text = open(full, newline='', encoding='utf-8',
                        errors='surrogateescape').read().split('\n')[s['line'] - 1]
            if text[s['column'] - 1:].startswith(f"CLOSURE({s['identifier']})"):
                applied += 1
                continue
        bad.append(f"{s['file']}:{s['line']}:{s['column']}: expected "
                   f"{s['identifier']!r}, found {ident!r}")
    print(f'recorded sites: {len(sites)}  ({applied} already bound)')
    if bad:
        print(f'\nSTALE ({len(bad)}) -- upstream moved; re-run with --discover:')
        for b in bad[:15]:
            print('  ', b)
        return 1
    print('OK: every recorded site matches, bound or not')
    return 0


if __name__ == '__main__':
    sys.exit(main())
