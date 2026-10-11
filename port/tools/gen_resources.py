#!/usr/bin/env python3
"""
Generate a C++ string table from WinSCP's .rc string tables.

Why this rather than a resource loader, and rather than patching core:

  core/Common.cpp defines LoadStr in terms of the Win32 LoadString API. So the
  cheapest correct route is to implement *LoadString* over a generated table.
  Core stays byte-identical, no .res binary format has to be parsed, and the
  piece we add is additive -- which also makes it the kind of change upstream
  could plausibly take, rather than a fork of core's own code.

The .rc format here is plain text in two shapes:

    IDENT, "text"                 a single line
    IDENT,                        an id, then adjacent string literals on
      "first part\\n"              following lines, concatenated as C does
      "second part"

Numeric ids come from the matching .h, which #defines each IDENT.

  gen_resources.py --src <winscp/source> --out <generated.cpp>
"""
import argparse, os, re, sys

DEFINE = re.compile(r'^#define\s+([A-Za-z_]\w*)\s+(\d+)\s*$')
ENTRY_FULL = re.compile(r'^\s+([A-Za-z_]\w*)\s*,\s*(".*")\s*$')
ENTRY_ID = re.compile(r'^\s+([A-Za-z_]\w*)\s*,\s*$')
LITERAL = re.compile(r'^\s*(".*")\s*,?\s*$')

# Resource-compiler escapes. RC also accepts "" for an embedded quote, which is
# handled separately because it is not a backslash escape.
ESCAPES = {'n': '\n', 'r': '\r', 't': '\t', '\\': '\\', '"': '"', '0': '\0', 'a': '\a'}


def unquote(text):
    """Concatenate adjacent "..." literals and resolve escapes."""
    out = []
    i = 0
    while i < len(text):
        if text[i] != '"':
            i += 1
            continue
        i += 1
        while i < len(text):
            c = text[i]
            if c == '"':
                # A doubled quote inside a literal is one quote character.
                if i + 1 < len(text) and text[i + 1] == '"':
                    out.append('"')
                    i += 2
                    continue
                i += 1
                break
            if c == '\\' and i + 1 < len(text):
                out.append(ESCAPES.get(text[i + 1], text[i + 1]))
                i += 2
                continue
            out.append(c)
            i += 1
    return ''.join(out)


def parse_ids(src):
    ids = {}
    for name in ('TextsCore.h', 'TextsFileZilla.h', 'TextsWin.h'):
        path = os.path.join(src, 'resource', name)
        if not os.path.exists(path):
            continue
        for line in open(path, encoding='utf-8', errors='replace'):
            m = DEFINE.match(line)
            if m:
                ids[m.group(1)] = int(m.group(2))
    return ids


def parse_strings(src, rc_files):
    """Return {IDENT: text}, preserving the order the .rc declares."""
    found = {}
    for name in rc_files:
        path = os.path.join(src, 'resource', name)
        if not os.path.exists(path):
            continue
        lines = open(path, encoding='utf-8', errors='replace').read().split('\n')
        inside = False
        pending = None
        parts = []

        def flush():
            if pending is not None and parts:
                found[pending] = unquote(' '.join(parts))

        for line in lines:
            stripped = line.strip()
            if stripped == 'BEGIN':
                inside = True
                continue
            if stripped == 'END':
                flush()
                pending, parts = None, []
                inside = False
                continue
            if not inside or not stripped or stripped.startswith('//'):
                continue

            m = ENTRY_FULL.match(line)
            if m:
                flush()
                pending, parts = None, []
                found[m.group(1)] = unquote(m.group(2))
                continue
            m = ENTRY_ID.match(line)
            if m:
                flush()
                pending, parts = m.group(1), []
                continue
            m = LITERAL.match(line)
            if m and pending is not None:
                parts.append(m.group(1))
                continue
        flush()
    return found


def cxx_escape(s):
    out = []
    for ch in s:
        if ch == '\\':
            out.append('\\\\')
        elif ch == '"':
            out.append('\\"')
        elif ch == '\n':
            out.append('\\n')
        elif ch == '\r':
            out.append('\\r')
        elif ch == '\t':
            out.append('\\t')
        elif ch == '\0':
            out.append('\\0')
        elif ord(ch) < 0x20:
            out.append(f'\\x{ord(ch):02x}')
        elif ord(ch) > 0x7E:
            # Emitted as a universal character name so the generated file stays
            # pure ASCII and cannot be corrupted by an encoding mismatch.
            out.append(f'\\u{ord(ch):04X}')
        else:
            out.append(ch)
    return ''.join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--src', default='3rd-party/winscp/source')
    ap.add_argument('--out', required=True)
    ap.add_argument('--rc', nargs='*',
                    # TextsWin1.rc: the frontend's texts, which a headless
                    # frontend needs too (button captions, for one).
                    default=['TextsCore1.rc', 'TextsCore2.rc',
                             'TextsFileZilla.rc', 'TextsWin1.rc'])
    a = ap.parse_args()

    ids = parse_ids(a.src)
    strings = parse_strings(a.src, a.rc)

    rows = []
    unknown = []
    for name, text in strings.items():
        if name in ids:
            rows.append((ids[name], name, text))
        else:
            unknown.append(name)
    rows.sort()

    # Duplicate ids would make lookup order-dependent, so refuse rather than
    # silently pick one.
    seen = {}
    dupes = []
    for ident, name, _ in rows:
        if ident in seen:
            dupes.append(f'{ident}: {seen[ident]} and {name}')
        seen[ident] = name

    os.makedirs(os.path.dirname(a.out) or '.', exist_ok=True)
    with open(a.out, 'w', encoding='ascii') as f:
        f.write('// Generated by port/tools/gen_resources.py -- do not edit.\n')
        f.write('//\n')
        f.write('// WinSCP resource strings, lifted from the .rc tables so that\n')
        f.write('// LoadString can serve them without a .res parser and without\n')
        f.write('// changing core.\n\n')
        f.write('#include "vcl_resources.h"\n\n')
        f.write('namespace linscp {\n\n')
        f.write('const TResourceString ResourceStrings[] = {\n')
        for ident, name, text in rows:
            f.write(f'  {{ {ident}, L"{cxx_escape(text)}" }},  // {name}\n')
        f.write('};\n\n')
        f.write('const std::size_t ResourceStringCount =\n'
                '  sizeof(ResourceStrings) / sizeof(ResourceStrings[0]);\n\n')
        f.write('}  // namespace linscp\n')

    print(f'ids defined        : {len(ids)}')
    print(f'strings in .rc     : {len(strings)}')
    print(f'emitted (id known) : {len(rows)}')
    if unknown:
        print(f'WITHOUT a numeric id: {len(unknown)}')
        for n in unknown[:10]:
            print(f'   {n}')
    if dupes:
        print(f'DUPLICATE ids: {len(dupes)}')
        for d in dupes[:10]:
            print(f'   {d}')
        return 1
    print(f'-> {a.out}')
    return 1 if unknown else 0


if __name__ == '__main__':
    sys.exit(main())
