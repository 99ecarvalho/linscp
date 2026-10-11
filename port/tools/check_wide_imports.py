#!/usr/bin/env python3
"""
Enforce ADR 0001 at link level: no library built with -fshort-wchar may call
the C library's wide-character functions.

Those take glibc's 4-byte wchar_t; called with 2-byte strings they link
cleanly and corrupt memory (PuTTY's dupwcs reached glibc's wcscpy this way,
and std::wstring reaches wcslen through char_traits). The source-level check
(check_wide_abi.py) covers port/; this covers everything linked, upstream
included, by reading the archives' undefined symbols. A wide function the
archives define themselves (the platform layer's _wfopen, PuTTY's dupwcs) is
not libc's and is fine.

  check_wide_imports.py NM ARCHIVE...
"""
import re, subprocess, sys

# glibc's wide functions, under their own names or C23/C99 aliases
# (__isoc23_swscanf).
WIDE = re.compile(r'^(?:__isoc\d+_)?(wcs\w*|wmem\w*|wcr\w*|wct\w*|mbs\w*|mbrtowc|mbtowc|btowc|'
                  r'w?printf_?w?|[fsv]?w(printf|scanf)|vf?sw(printf|scanf)|sw(printf|scanf)|'
                  r'fgetw\w*|fputw\w*|getw\w*|putw\w*|isw\w*|tow\w*|wcsftime)$')

# Reviewed exceptions: a single character, never a buffer, so the width of
# wchar_t does not matter.
ALLOWED = {
    'iswalpha_l': "linscp_WideIsAlpha classifies one code point",
}


def symbols(nm, archive):
    out = subprocess.run([nm, '-A', archive], capture_output=True, text=True, check=True).stdout
    undefined, defined = set(), set()
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[-2] == 'U':
            undefined.add((archive, parts[0].rsplit(':', 1)[0], parts[-1]))
        elif len(parts) >= 3 and parts[-2] in 'TtWwDdBbRrVv':
            defined.add(parts[-1])
    return undefined, defined


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    nm, archives = argv[1], argv[2:]
    undefined, defined = set(), set()
    for a in archives:
        u, d = symbols(nm, a)
        undefined |= u
        defined |= d
    bad = sorted((obj, sym) for _, obj, sym in undefined
                 if WIDE.match(sym) and sym not in defined and sym not in ALLOWED)
    print(f'archives scanned: {len(archives)}')
    if bad:
        print(f'\nC LIBRARY WIDE FUNCTIONS CALLED ({len(bad)}):')
        for obj, sym in bad:
            print(f'  {obj}: {sym}')
        return 1
    print('OK: no C library wide-character function is called')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
