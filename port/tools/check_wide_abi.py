#!/usr/bin/env python3
"""
Enforce ADR 0001: the wide-character ABI must stay uniform.

We build with -fshort-wchar, so wchar_t is 2 bytes. libstdc++'s
char_traits<wchar_t> and libc's wide functions are compiled against a 4-byte
wchar_t, so touching them is an ABI mismatch that links cleanly and then
misbehaves at runtime -- the worst possible failure mode.

Banned in port/: std::wstring, std::wstring_view, wide iostreams, <cwchar>,
<cwctype>, and direct calls to the libc wide functions. The shim implements what
it needs itself; vcl_unicodestring.cpp is the one allowed place for the
wchar_t/char16_t reinterpret_cast.
"""
import os, re, sys

BANNED = [
    (re.compile(r'\bstd::wstring\b'),        'std::wstring (char_traits<wchar_t> calls libc)'),
    (re.compile(r'\bstd::wstring_view\b'),   'std::wstring_view'),
    (re.compile(r'\bstd::w(cout|cerr|clog|cin|ostream|istream|fstream)\b'), 'wide iostreams'),
    (re.compile(r'#\s*include\s*<cwchar>'),  '<cwchar>'),
    (re.compile(r'#\s*include\s*<cwctype>'), '<cwctype>'),
    (re.compile(r'#\s*include\s*<wchar\.h>'),'<wchar.h>'),
    (re.compile(r'\b(wcslen|wcscmp|wcsncmp|wcscpy|wcsncpy|wcschr|wcsrchr|'
                r'wcsstr|wmemcmp|wmemchr|wmemcpy|wmemmove|wmemset|swprintf|'
                r'wcscoll|wcsxfrm|wcstol|wcstod|iswalpha|iswdigit|towlower|'
                r'towupper)\s*\('), 'libc wide function'),
]

ROOTS = ['port/shim', 'port/platform', 'qt']
EXTS = {'.h', '.hpp', '.c', '.cpp', '.cc'}

def main():
    hits = []
    scanned = 0
    for root in ROOTS:
        if not os.path.isdir(root):
            continue
        for dp, _, ns in os.walk(root):
            for n in ns:
                if os.path.splitext(n)[1] not in EXTS:
                    continue
                p = os.path.join(dp, n)
                # win32_on_posix is where the replacements for those libc
                # functions live, so it necessarily names them. Exempting it by
                # path rather than loosening the patterns keeps the ban in force
                # everywhere else.
                if os.path.basename(p).startswith('win32_on_posix'):
                    continue
                scanned += 1
                for i, line in enumerate(open(p, encoding='utf-8',
                                              errors='replace'), 1):
                    if line.lstrip().startswith(('//', '*')):
                        continue          # commentary may name them
                    if '// ADR 0001: Windows CRT' in line:
                        continue          # Windows-only code, where the CRT's
                                          # wchar_t is 2 bytes; marked per line
                    for pat, why in BANNED:
                        if pat.search(line):
                            hits.append((p, i, why, line.strip()[:70]))

    print(f'files scanned: {scanned}')

    # The 2-byte assumption must also be asserted in the source itself.
    us = 'port/shim/vcl_unicodestring.h'
    if os.path.exists(us):
        if 'static_assert(sizeof(wchar_t) == 2' not in open(us).read():
            hits.append((us, 0, 'missing static_assert(sizeof(wchar_t) == 2)', ''))

    if hits:
        print(f'\nADR 0001 VIOLATIONS ({len(hits)}):')
        for p, i, why, line in hits:
            print(f'  {p}:{i}: {why}')
            if line:
                print(f'      {line}')
        return 1
    print('\nOK: no 4-byte-wchar_t ABI dependencies in port/ or qt/')
    return 0

if __name__ == '__main__':
    sys.exit(main())
