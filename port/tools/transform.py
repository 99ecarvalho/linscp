#!/usr/bin/env python3
"""
Transform pristine WinSCP source into a clang-compilable tree.

Reads 3rd-party/winscp (a pinned, unmodified submodule) and writes a transformed
copy. The repo never contains a modified copy of WinSCP source; this script plus
the patch series in port/patches/ is the whole delta.

Transformations are one-line-in/one-line-out. That is not an imposed constraint:
every __property declaration in core is already on a single line (verified: 0 of
591 span lines), so preserving them costs nothing and keeps `diff -u upstream gen`
reviewable with zero offsets.

Rules applied:
  __property <T> <Name> = { read = X [, write = Y] };   -> PROP_* macro call
  __property <T> <Name> = { read = X, index = I };      -> PROP_R_GI
  __property <T> <Name>[IT Index] = { ... };            -> PROP_IDX_* macro call

A read/write target is classified as a method if the enclosing file declares it
with parentheses, otherwise as a field. Fields are never followed by '('.

Usage:
  transform.py --src 3rd-party/winscp --out build/gen [--check] [--report FILE]
"""
import argparse, os, re, shutil, sys, json
from collections import Counter

PROP_RE = re.compile(
    r'(?P<indent>\s*)__property\s+(?P<decl>[^=;{}]+?)\s*(?P<idx>\[[^\]]*\])?\s*'
    r'=\s*\{(?P<body>[^}]*)\}\s*;(?P<tail>.*)$')

def split_type_name(decl):
    """'TVSFixedFileInfo *FixedApplicationInfo' -> ('TVSFixedFileInfo *', 'FixedApplicationInfo')"""
    m = re.match(r'^(?P<type>.*?)(?P<ptr>[\s*&]+)(?P<name>\w+)$', decl.strip())
    if not m:
        return None, None
    return (m.group('type') + m.group('ptr')).strip(), m.group('name')

def parse_body(body):
    """'read = GetX, write = SetY' -> {'read':'GetX','write':'SetY'}"""
    out = {}
    for part in re.split(r',(?![^(]*\))', body):   # don't split inside MACRO(a, b)
        if '=' not in part:
            continue
        k, v = part.split('=', 1)
        out[k.strip()] = v.strip()
    return out

class Transformer:
    def __init__(self):
        self.stats = Counter()
        self.unhandled = []

    def transform_file(self, path, text):
        methods = set(re.findall(r'\b(\w+)\s*\(', text))
        out = []
        for lineno, line in enumerate(text.split('\n'), 1):
            if '__property' not in line or '#define' in line:
                out.append(line); continue
            new = self.transform_line(path, lineno, line, methods)
            out.append(new if new is not None else line)
        return '\n'.join(out)

    def transform_line(self, path, lineno, line, methods):
        m = PROP_RE.match(line)
        if not m:
            self.stats['unmatched'] += 1
            self.unhandled.append((path, lineno, line.strip()))
            return None
        typ, name = split_type_name(m.group('decl'))
        if not name:
            self.stats['unsplittable'] += 1
            self.unhandled.append((path, lineno, line.strip()))
            return None

        b = parse_body(m.group('body'))
        r, w, i = b.get('read'), b.get('write'), b.get('index')
        indexed = bool(m.group('idx'))
        ind, tail = m.group('indent'), m.group('tail')

        def kind(x, write=False):
            if x in methods:
                return 'S' if write else 'G'
            return 'F'

        if indexed:
            it = re.match(r'\[\s*([\w:<>\s*&]+?)\s+\w+\s*\]', m.group('idx'))
            itype = it.group(1).strip() if it else 'int'
            if r and w:
                call = f'PROP_IDX_RW_GS({typ}, {name}, {itype}, {r}, {w})'
                self.stats['idx_rw'] += 1
            elif r:
                call = f'PROP_IDX_R_G({typ}, {name}, {itype}, {r})'
                self.stats['idx_r'] += 1
            else:
                self.stats['unhandled_idx'] += 1
                self.unhandled.append((path, lineno, line.strip())); return None
        elif i is not None and r:
            call = f'PROP_R_GI({typ}, {name}, {r}, {i})'
            self.stats['r_gi'] += 1
        elif r and w:
            call = f'PROP_RW_{kind(r)}{kind(w, True)}({typ}, {name}, {r}, {w})'
            if kind(r) == 'F' and kind(w, True) == 'F' and r == w:
                call = f'PROP_RW_FF({typ}, {name}, {r})'
            self.stats[f'rw_{kind(r)}{kind(w, True)}'] += 1
        elif r:
            call = f'PROP_R_{kind(r)}({typ}, {name}, {r})'
            self.stats[f'r_{kind(r)}'] += 1
        elif w:
            call = f'PROP_W_{kind(w, True)}({typ}, {name}, {w})'
            self.stats[f'w_{kind(w, True)}'] += 1
        else:
            self.stats['empty_body'] += 1
            self.unhandled.append((path, lineno, line.strip())); return None

        self.stats['transformed'] += 1
        return f'{ind}{call};{tail}'

EXTS = {'.h', '.hpp', '.cpp', '.c', '.inc'}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--src', default='3rd-party/winscp')
    ap.add_argument('--out', default='build/gen')
    ap.add_argument('--subdirs', nargs='*', default=['source/core'])
    ap.add_argument('--check', action='store_true',
                    help='report only; do not write the output tree')
    ap.add_argument('--report')
    a = ap.parse_args()

    t = Transformer()
    nfiles = 0
    for sub in a.subdirs:
        root = os.path.join(a.src, sub)
        if not os.path.isdir(root):
            sys.exit(f'missing source dir: {root}')
        for dirpath, _, names in os.walk(root):
            for n in names:
                src = os.path.join(dirpath, n)
                rel = os.path.relpath(src, a.src)
                dst = os.path.join(a.out, rel)
                if os.path.splitext(n)[1].lower() not in EXTS:
                    continue
                text = open(src, encoding='utf-8', errors='surrogateescape').read()
                new = t.transform_file(rel, text)
                nlines_ok = len(text.split('\n')) == len(new.split('\n'))
                if not nlines_ok:
                    sys.exit(f'LINE COUNT CHANGED in {rel} — transform is not 1:1')
                nfiles += 1
                if not a.check:
                    os.makedirs(os.path.dirname(dst), exist_ok=True)
                    open(dst, 'w', encoding='utf-8', errors='surrogateescape').write(new)

    print(f'files processed: {nfiles}')
    for k, v in sorted(t.stats.items()):
        print(f'  {k:16} {v}')
    if t.unhandled:
        print(f'\nUNHANDLED ({len(t.unhandled)}):')
        for p, l, s in t.unhandled[:20]:
            print(f'  {p}:{l}: {s[:100]}')
    if a.report:
        json.dump({'stats': dict(t.stats), 'unhandled': t.unhandled},
                  open(a.report, 'w'), indent=2)
    return 1 if t.unhandled else 0

if __name__ == '__main__':
    sys.exit(main())
