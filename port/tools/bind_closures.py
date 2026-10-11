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

Discovery compiles a separate tree transformed *without* the recorded sites
bound. Compiling the ordinary tree would see the known sites already wrapped --
no longer errors -- and so "rediscover" only the new ones, silently dropping
the rest; that happened once and cut 62 sites to 2. Against an unbound tree the
list is rebuilt whole, discovery is idempotent, and the columns are in exactly
the coordinates transform.py applies them in.

  bind_closures.py --discover   recompile and rewrite port/patches/closure_sites.json
  bind_closures.py              verify the recorded sites still match the source
"""
import argparse, json, os, re, subprocess, sys


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

SITES = 'port/patches/closure_sites.json'
# Borland's implicit closure, in each shape clang reports differently: a bare
# method name, and its address taken with & (unqualified, or of another
# object's method). All three are recorded the same way -- the column of the
# method name -- and transform.py works out the shape from the source.
DIAG = re.compile(
    r'^(?P<file>[^:]+):(?P<line>\d+):(?P<col>\d+): error: '
    r'(reference to non-static member function must be called'
    r'|must explicitly qualify name of member function when taking its address'
    r'|cannot create a non-constant pointer to member function)')
# When the offending token sits inside a macro body, clang reports the error at
# the *expansion* point and then points at the definition with one of these per
# level of nesting. The identifier we must wrap lives in the definition, not at
# the call, so a single macro accounts for every one of its call sites: the
# ADSTR macro in SessionInfo.cpp alone produced 61 duplicate reports.
EXPANDED = re.compile(
    r'^(?P<file>[^:]+):(?P<line>\d+):(?P<col>\d+): note: '
    r"expanded from macro '(?P<macro>\w+)'")
IDENT = re.compile(r'[A-Za-z_]\w*')

DISCOVERY_GEN = 'build/gen-unbound'
INCLUDES = [
    'port/shim',
    '3rd-party/winscp/source/resource', '3rd-party/winscp/source/windows',
    '3rd-party/winscp/source/filezilla', '3rd-party/winscp/source/putty',
    '3rd-party/winscp/source/putty/windows',
    '3rd-party/winscp/libs/openssl/include', '3rd-party/winscp/libs/neon/src',
    '3rd-party/winscp/libs/expat/lib',
]


def compile_tu(path, gen, target='x86_64-w64-mingw32'):
    cmd = ['clang++', f'--target={target}', '-std=c++17', '-fsyntax-only',
           '-fms-extensions', '-fshort-wchar', '-ferror-limit=0',
           # One line per diagnostic. Without this clang interleaves the source
           # echo and a caret line, which breaks the scan for the 'expanded
           # from macro' notes that follow an error.
           '-fno-caret-diagnostics', '-fmacro-backtrace-limit=0',
           '-Wno-everything']
    for inc in [os.path.join(gen, 'source/core')] + INCLUDES:
        cmd += ['-I', inc]
    cmd += _upstream_defines().flags(target)
    cmd.append(path)
    return subprocess.run(cmd, capture_output=True, text=True).stderr


def discover(gen):
    """Resolve each diagnostic to the place the identifier actually appears."""
    found = {}
    core = os.path.join(gen, 'source/core')
    for name in sorted(os.listdir(core)):
        if not name.endswith('.cpp'):
            continue
        lines = compile_tu(os.path.join(core, name), gen).split('\n')
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
                found_at = method_name_at(gen, rel, lineno, col)
                if found_at is not None and found_at[1] not in macros:
                    chosen = (rel, lineno, found_at[0])
                    break
            if chosen is not None:
                found[chosen] = None
            i = j
    return sorted(found)


def method_name_at(gen, path, lineno, col):
    """(column, name) of the method a diagnostic refers to. The & forms are
    reported at the &, and of an object's method at the start of the chain,
    so skip the & and take the last name of `a.b` or `a->b`."""
    full = os.path.join(gen, path)
    if not os.path.exists(full):
        return None
    lines = open(full, newline='', encoding='utf-8',
                 errors='surrogateescape').read().split('\n')
    if lineno < 1 or lineno > len(lines):
        return None
    m = re.compile(r'&?\s*([A-Za-z_]\w*)(?:\s*(?:\.|->)\s*([A-Za-z_]\w*))?').match(
        lines[lineno - 1], col - 1)
    if not m:
        return None
    group = 2 if m.group(2) else 1
    return m.start(group) + 1, m.group(group)


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


def text_at(gen, site):
    full = os.path.join(gen, site['file'])
    return open(full, newline='', encoding='utf-8',
                errors='surrogateescape').read().split('\n')[site['line'] - 1]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--gen', default='build/gen')
    ap.add_argument('--discover', action='store_true')
    a = ap.parse_args()

    if a.discover:
        r = subprocess.run(
            [sys.executable, 'port/tools/transform.py', '--out', DISCOVERY_GEN,
             '--no-closure-sites'], capture_output=True, text=True)
        if r.returncode != 0:
            sys.exit(f'transform for discovery failed:\n{r.stdout}{r.stderr}')
        previous = set()
        if os.path.exists(SITES):
            previous = {(s['file'], s['line'], s['column'], s['identifier'])
                        for s in json.load(open(SITES))['sites']}
        sites = []
        unresolved = []
        for path, lineno, col in discover(DISCOVERY_GEN):
            ident = identifier_at(DISCOVERY_GEN, path, lineno, col)
            if ident is None:
                unresolved.append(f'{path}:{lineno}:{col}')
                continue
            sites.append({'file': path, 'line': lineno, 'column': col,
                          'identifier': ident})
        os.makedirs(os.path.dirname(SITES), exist_ok=True)
        json.dump({'sites': sites}, open(SITES, 'w'), indent=2)
        open(SITES, 'a').write('\n')
        print(f'discovered {len(sites)} closure call sites -> {SITES}')
        now = {(s['file'], s['line'], s['column'], s['identifier']) for s in sites}
        # Shown so a shrinking list is a visible decision, never a side effect.
        print(f'  vs previous: +{len(now - previous)} added, '
              f'-{len(previous - now)} removed')
        for s in sorted(previous - now)[:10]:
            print(f'     removed {s[0]}:{s[1]}:{s[2]} {s[3]}')
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
        # The & and object forms rewrite from a column left of the name.
        if re.search(rf"CLOSURE\({s['identifier']}\)|CLOSURE_OF\([\w&>-]+, "
                     rf"{s['identifier']}\)", text_at(a.gen, s)):
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
