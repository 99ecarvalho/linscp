#!/usr/bin/env python3
"""
Measure the VCL surface the core actually depends on.

Rather than guess the shim's size, resolve every identifier the core uses against
the headers that really exist on this machine:

  core identifiers
    - names the core declares itself
    - Win32 (mingw-w64 headers)
    - OpenSSL
    - C and C++ standard library
    - language keywords
  = the VCL runtime surface the shim must provide

The leftover set is the actual specification for port/shim. Frequency ranks it,
so the shim can be built most-used-first.
"""
import os, re, sys, collections

IDENT = re.compile(r'\b[A-Za-z_]\w*\b')

# A name is plausibly VCL runtime if it looks like a VCL type or a known RTL
# string/alias. Everything else in the unresolved set is almost always a local,
# a parameter, an enum member or a vendored C symbol.
VCL_SHAPE = re.compile(r'^(T[A-Z]\w+|P[A-Z]\w+|_di_I\w+|'
                       r'UnicodeString|RawByteString|UTF8String|AnsiString|'
                       r'WideString|Exception|E[A-Z]\w+|Word|Byte|Cardinal|'
                       r'THandle|TThreadID)$')

KEYWORDS = set('''alignas alignof and and_eq asm auto bitand bitor bool break case
catch char char8_t char16_t char32_t class compl concept const consteval constexpr
constinit const_cast continue co_await co_return co_yield decltype default delete
do double dynamic_cast else enum explicit export extern false float for friend goto
if inline int long mutable namespace new noexcept not not_eq nullptr operator or
or_eq private protected public register reinterpret_cast requires return short
signed sizeof static static_assert static_cast struct switch template this
thread_local throw true try typedef typeid typename union unsigned using virtual
void volatile wchar_t while xor xor_eq override final'''.split())

def ids_in_tree(root, exts, limit_files=None):
    """All identifiers appearing anywhere under root. Over-broad on purpose: used
    only as a membership test for 'could this name come from here'."""
    s = set()
    n = 0
    for dp, _, ns in os.walk(root):
        for f in ns:
            if exts and os.path.splitext(f)[1] not in exts:
                continue
            p = os.path.join(dp, f)
            try:
                s.update(IDENT.findall(open(p, encoding='utf-8', errors='ignore').read()))
            except OSError:
                continue
            n += 1
            if limit_files and n >= limit_files:
                return s
    return s

DECL_PATTERNS = [
    re.compile(r'\b(?:class|struct|union)\s+(\w+)'),
    re.compile(r'\benum\s+(?:class\s+)?(\w+)'),
    re.compile(r'\btypedef\s+.*?\b(\w+)\s*;'),
    re.compile(r'\busing\s+(\w+)\s*='),
    re.compile(r'#define\s+(\w+)'),
    re.compile(r'\b(\w+)\s*\([^;{]*\)\s*(?:const)?\s*[;{]'),   # function decls
    re.compile(r'^\s*(?:\w+[\s*&]+)+?(\w+)\s*;', re.M),         # member fields
    re.compile(r'\b(\w+)\s*,|\b(\w+)\s*=\s*\d'),                # enum members
]

def main():
    CORE = 'build/gen/source/core'
    if not os.path.isdir(CORE):
        sys.exit('run transform.py first')

    core_text = []
    for dp, _, ns in os.walk(CORE):
        for f in ns:
            core_text.append(open(os.path.join(dp, f), encoding='utf-8',
                                  errors='ignore').read())
    blob = '\n'.join(core_text)

    used = collections.Counter(IDENT.findall(blob))

    own = set()
    for pat in DECL_PATTERNS:
        for m in pat.finditer(blob):
            own.update(g for g in m.groups() if g)

    print('resolving against real headers on this machine...', file=sys.stderr)
    win32   = ids_in_tree('/usr/x86_64-w64-mingw32/include', {'.h', '.hpp', ''})
    openssl = ids_in_tree('/usr/include/openssl', {'.h'})
    cxx     = ids_in_tree('/usr/include/c++', None)
    libc    = ids_in_tree('/usr/include/x86_64-linux-gnu', {'.h'}) | \
              ids_in_tree('/usr/lib/llvm-19/lib/clang/19/include', {'.h'})

    buckets = collections.OrderedDict([
        ('own (core)', own), ('win32', win32), ('openssl', openssl),
        ('c++ stdlib', cxx), ('libc/clang', libc), ('keyword', KEYWORDS),
    ])

    vcl = collections.Counter()
    attributed = collections.Counter()
    for name, count in used.items():
        if len(name) < 2:
            continue
        for bname, bset in buckets.items():
            if name in bset:
                attributed[bname] += 1
                break
        else:
            vcl[name] = count

    print('\n=== identifier attribution (distinct names) ===')
    for b, c in attributed.items():
        print(f'  {b:14} {c}')
    print(f'  {"UNRESOLVED":14} {len(vcl)}   <- candidate VCL surface')

    shaped = {n: c for n, c in vcl.items() if VCL_SHAPE.match(n)}
    print(f'\n=== VCL-shaped unresolved names ({len(shaped)}) ===')
    for n, c in sorted(shaped.items(), key=lambda x: -x[1]):
        print(f'  {c:6}  {n}')
    print(f'\n(remaining {len(vcl) - len(shaped)} unresolved names are locals,'
          f' parameters, enum members and vendored C symbols)')

    with open('build/census.txt', 'w') as f:
        for n, c in vcl.most_common():
            f.write(f'{c}\t{n}\n')
    print(f'\nfull list ({len(vcl)} names) -> build/census.txt')

if __name__ == '__main__':
    main()
