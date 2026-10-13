#!/usr/bin/env python3
"""
Every function a Windows program imports from a DLL it ships with must be
exported by that DLL.

  check_dll_imports.py --objdump llvm-objdump --dlls DIR [--dlls DIR ...] program.exe ...

Windows refuses to start a program with an import it cannot resolve
(STATUS_ENTRYPOINT_NOT_FOUND); Wine starts it and fails only if the function
is called, so the Wine tests do not see this. Checked: each program, and the
DLLs found in the directories that it loads, recursively. DLLs from neither
(the system's) are not checked.
"""
import argparse, os, re, subprocess, sys


def table(objdump, path):
    """(exports, {dll: imported names}) of a PE file."""
    out = subprocess.run([objdump, '-p', path], capture_output=True, text=True, check=True).stdout
    exports, imports, current, in_exports = set(), {}, None, False
    for line in out.splitlines():
        m = re.match(r'\s*DLL Name: (\S+)', line)
        if m:
            current, in_exports = m.group(1).lower(), False
            imports.setdefault(current, set())
            continue
        if line.startswith('Export Table:'):
            current, in_exports = None, True
            continue
        if in_exports:
            m = re.match(r'\s+\d+\s+(?:0x)?[0-9a-fA-F]+\s+(\S+)$', line)
            if m:
                exports.add(m.group(1))
        elif current:
            stripped = line.strip()
            m = re.match(r'(?:\d+\s+)?(\S+)$', stripped)
            if m and stripped and not stripped.startswith(('vma:', 'Ordinal', 'Hint')):
                imports[current].add(m.group(1))
    return exports, imports


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--objdump', required=True)
    ap.add_argument('--dlls', action='append', default=[])
    ap.add_argument('programs', nargs='+')
    a = ap.parse_args()
    shipped = {}
    for d in a.dlls:
        for name in os.listdir(d):
            if name.lower().endswith('.dll'):
                shipped.setdefault(name.lower(), os.path.join(d, name))
    tables, failures = {}, 0
    todo, seen = list(a.programs), set()
    while todo:
        path = todo.pop()
        if path in seen:
            continue
        seen.add(path)
        _, imports = tables.setdefault(path, table(a.objdump, path))
        for dll, names in sorted(imports.items()):
            if dll not in shipped:
                continue
            target = shipped[dll]
            exports, _ = tables.setdefault(target, table(a.objdump, target))
            todo.append(target)
            missing = sorted(n for n in names if n not in exports)
            if missing:
                failures += 1
                print(f'FAIL {os.path.basename(path)} imports from {dll} what it does not export: '
                      f'{", ".join(missing[:10])}{" ..." if len(missing) > 10 else ""}')
    if failures == 0:
        print(f'OK: {len(seen)} files, every import from a shipped DLL resolves')
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
