#!/usr/bin/env python3
"""
Compile-test every transformed property declaration.

Extracts each PROP_* macro call from the generated tree, wraps it in a synthetic
struct with stub members matching the referenced fields/accessors, and compiles
the lot with clang. This validates macro arity and expansion for the whole real
corpus without needing the full VCL shim to exist yet.
"""
import re, subprocess, sys, os, collections

GEN = sys.argv[1] if len(sys.argv) > 1 else 'build/gen/source/core'
OUT = sys.argv[2] if len(sys.argv) > 2 else 'build/propcheck.cpp'

BUILTIN = {'bool','int','unsigned','char','wchar_t','double','float','long',
           'void','size_t','short','signed','__int64','unsigned int',
           'unsigned long','unsigned char','unsigned short','long long',
           'unsigned long long','long double','int64_t','uint32_t'}

CALL = re.compile(r'\b(PROP_(?:R_F|R_G|R_GI|RW_FF|RW_FS|RW_GF|RW_GS|W_F|W_S|'
                  r'IDX_R_G|IDX_RW_GS))\((.*)\)\s*;')

def split_args(s):
    out, depth, cur = [], 0, ''
    for ch in s:
        if ch in '(<[': depth += 1
        elif ch in ')>]': depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur.strip()); cur = ''
        else:
            cur += ch
    out.append(cur.strip())
    return out

def base_type(t):
    t = t.replace('*', ' ').replace('&', ' ')
    t = re.sub(r'\bconst\b', ' ', t).strip()
    return t

items, types = [], set()
for dirpath, _, names in os.walk(GEN):
    for n in sorted(names):
        p = os.path.join(dirpath, n)
        for line in open(p, encoding='utf-8', errors='surrogateescape'):
            m = CALL.search(line)
            if not m:
                continue
            macro, args = m.group(1), split_args(m.group(2))
            items.append((macro, args, f'{n}'))
            types.add(base_type(args[0]))
            if macro.startswith('PROP_IDX'):
                types.add(base_type(args[2]))

simple, nested = set(), collections.defaultdict(set)
for t in types:
    if not t or t in BUILTIN:
        continue
    if re.fullmatch(r'[A-Za-z_]\w*', t):
        simple.add(t)
    elif re.fullmatch(r'[A-Za-z_]\w*(::[A-Za-z_]\w*)+', t):
        parts = t.split('::')
        nested[parts[0]].add('::'.join(parts[1:]))
simple -= set(nested)
types = simple

body = []
for i, (macro, a, src) in enumerate(items):
    T = a[0]
    s = [f'// {src}', f'struct S{i} {{']
    if macro == 'PROP_R_F':      s += [f'  {T} {a[2]};']
    elif macro == 'PROP_R_G':    s += [f'  {T} {a[2]}() const;']
    elif macro == 'PROP_R_GI':   s += [f'  {T} {a[2]}(int) const;']
    elif macro == 'PROP_RW_FF':  s += [f'  {T} {a[2]};']
    elif macro == 'PROP_RW_FS':  s += [f'  {T} {a[2]};', f'  void {a[3]}({T});']
    elif macro == 'PROP_RW_GF':  s += [f'  {T} {a[2]}() const;', f'  {T} {a[3]};']
    elif macro == 'PROP_RW_GS':  s += [f'  {T} {a[2]}() const;', f'  void {a[3]}({T});']
    elif macro == 'PROP_W_F':    s += [f'  {T} {a[2]};']
    elif macro == 'PROP_W_S':    s += [f'  void {a[2]}({T});']
    elif macro == 'PROP_IDX_R_G':   s += [f'  {T} {a[3]}({a[2]}) const;']
    elif macro == 'PROP_IDX_RW_GS': s += [f'  {T} {a[3]}({a[2]}) const;',
                                          f'  void {a[4]}({a[2]}, {T});']
    idx = '0' if macro == 'PROP_R_GI' else None
    call = f'{macro}(' + ', '.join(a[:-1] + ['0']) + ')' if idx else f'{macro}(' + ', '.join(a) + ')'
    s += [f'  {call};', '};']
    body.append('\n'.join(s))

with open(OUT, 'w') as f:
    f.write('#include "vcl_property.h"\n')
    for t in sorted(types):
        f.write(f'struct {t} {{ }};\n')
    for outer, inners in sorted(nested.items()):
        inner_decls = ' '.join(f'struct {i} {{ }};' for i in sorted(inners))
        f.write(f'struct {outer} {{ {inner_decls} }};\n')
    f.write('\n'.join(body))
    f.write('\nint main() { return 0; }\n')

# Chained subscripts on an indexed property compile as a two-argument getter
# call under clang's __declspec(property), silently changing meaning. Core does
# not do this today; fail loudly if an upstream change introduces it.
indexed_names = {a[1] for macro, a, _ in items if macro.startswith('PROP_IDX')}
chained = []
if indexed_names:
    alt = '|'.join(sorted(re.escape(n) for n in indexed_names))
    chain_re = re.compile(r'\b(' + alt + r')\s*\[[^\]]+\]\s*\[')
    for dirpath, _, names in os.walk(GEN):
        for n in sorted(names):
            fp = os.path.join(dirpath, n)
            for lineno, line in enumerate(
                    open(fp, encoding='utf-8', errors='ignore'), 1):
                m = chain_re.search(line)
                if m:
                    chained.append(f'{n}:{lineno}: {m.group(1)}[...][...]')
if chained:
    print(f'CHAINED INDEXED-PROPERTY SUBSCRIPTS ({len(chained)}):')
    for c in chained[:10]:
        print('  ', c)
    print('  clang treats these as a two-argument getter call; assign to a local first.')
    sys.exit(1)

print(f'declarations extracted: {len(items)}')
print('by macro:', dict(collections.Counter(m for m, _, _ in items)))
print(f'stub types declared: {len(types)} simple + {len(nested)} with nested members')

r = subprocess.run(['clang++', '-std=c++17', '-fms-extensions', '-fshort-wchar',
                    '-fsyntax-only',
                    '-I', 'port/shim', OUT], capture_output=True, text=True)
errs = [l for l in r.stderr.split('\n') if ': error:' in l]
print(f'\nclang exit={r.returncode}  errors={len(errs)}')
for e in errs[:15]:
    print('  ', e)
sys.exit(0 if r.returncode == 0 else 1)
