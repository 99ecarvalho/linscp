#!/usr/bin/env python3
"""
Assemble a folder that runs on Windows as it is: the programs, the Qt
plugins the shell needs, and every DLL they load from the Qt prefix -- found
by following their imports, so nothing the programs do not use comes along.

  deploy_windows.py --objdump llvm-objdump --qt-prefix PREFIX --dest DIR
                    [--translations DIR] program.exe ...

PREFIX is MSYS2's clang64 tree (port/tools/fetch_msys2_qt.py); its bin holds
Qt, its dependencies and the C++ runtime (ADR 0011). Qt finds the plugins in
the folders beside the executable. DLLs not in PREFIX are the system's.
Translations/ gets core's translations (--translations: gen_translations.py's
output) and Qt's own (qtbase_*.qm from PREFIX), where the shell looks for both.
"""
import argparse, glob, os, shutil, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from check_dll_imports import table  # noqa: E402

PLUGINS = [
    'platforms/qwindows.dll',          # the window system
    'styles/qmodernwindowsstyle.dll',  # native look (Windows 10/11)
    'imageformats/qico.dll',           # .ico icons
    # View > Thumbnails' previews (PNG and BMP are Qt's own).
    'imageformats/qjpeg.dll',
    'imageformats/qgif.dll',
    'imageformats/qsvg.dll',
    # HTTPS (Help > Check for Updates): Windows' own TLS, Schannel.
    'tls/qschannelbackend.dll',
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--objdump', required=True)
    ap.add_argument('--qt-prefix', required=True)
    ap.add_argument('--dest', required=True)
    ap.add_argument('--translations')
    ap.add_argument('programs', nargs='+')
    a = ap.parse_args()
    bin_dir = os.path.join(a.qt_prefix, 'bin')
    available = {n.lower(): os.path.join(bin_dir, n) for n in os.listdir(bin_dir) if n.lower().endswith('.dll')}

    shutil.rmtree(a.dest, ignore_errors=True)
    os.makedirs(a.dest)
    todo = []
    for program in a.programs:
        shutil.copy2(program, a.dest)
        todo.append(program)
    for plugin in PLUGINS:
        source = os.path.join(a.qt_prefix, 'share', 'qt6', 'plugins', plugin)
        target = os.path.join(a.dest, plugin)
        os.makedirs(os.path.dirname(target), exist_ok=True)
        shutil.copy2(source, target)
        todo.append(source)

    # Upstream's name: TGUIConfiguration looks there (TranslationsSubFolder).
    translations = os.path.join(a.dest, 'Translations')
    os.makedirs(translations)
    sources = glob.glob(os.path.join(a.qt_prefix, 'share', 'qt6', 'translations', 'qtbase_*.qm'))
    if a.translations:
        sources += glob.glob(os.path.join(a.translations, 'linscp.*'))
    for source in sources:
        shutil.copy2(source, translations)

    copied, seen = set(), set()
    while todo:
        path = todo.pop()
        if path in seen:
            continue
        seen.add(path)
        _, imports = table(a.objdump, path)
        for dll in imports:
            if dll in available and dll not in copied:
                copied.add(dll)
                shutil.copy2(available[dll], a.dest)
                todo.append(available[dll])
    print(f'{a.dest}: {len(a.programs)} programs, {len(PLUGINS)} plugins, {len(copied)} DLLs, '
          f'{len(sources)} translations')
    return 0


if __name__ == '__main__':
    sys.exit(main())
