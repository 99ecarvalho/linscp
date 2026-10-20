#!/usr/bin/env python3
"""
Assemble a folder that runs on Windows as it is: the programs, the Qt
plugins the shell needs, and every DLL they load from the Qt prefix -- found
by following their imports, so nothing the programs do not use comes along.

  deploy_windows.py --objdump llvm-objdump --qt-prefix PREFIX --dest DIR --version X.Y.Z
                    [--translations DIR] [--winscpnet WinSCPnet.dll --cli linscp-cli.exe]
                    program.exe ...

PREFIX is MSYS2's clang64 tree (port/tools/fetch_msys2_qt.py); its bin holds
Qt, its dependencies and the C++ runtime (ADR 0011). Qt finds the plugins in
the folders beside the executable. DLLs not in PREFIX are the system's.
Translations/ gets core's translations (--translations: gen_translations.py's
output) and Qt's own (qtbase_*.qm from PREFIX), where the shell looks for both.
With --winscpnet, dotnet/ gets WinSCP's .NET assembly (net40) and the client
as the WinSCP.exe it starts, with the client's DLLs: what WinSCP's
PowerShell extensions load from %WINSCP_PATH%, which the shell points there.

licenses/third-party/ gets the licence texts of every MSYS2 package a deployed
file comes from (fetch_msys2_qt.py's msys2-qt.packages.json, beside PREFIX),
with THIRD-PARTY-NOTICES.txt listing them and SOURCE-OFFER.txt offering their
source code, as the LGPL and GPL ask of whoever redistributes their binaries.
"""
import argparse, glob, json, os, re, shutil, sys

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
    ap.add_argument('--version', required=True)
    ap.add_argument('--translations')
    ap.add_argument('--winscpnet')
    # Files copied as they are (the shell extension, which needs no DLLs).
    ap.add_argument('--extra', action='append', default=[])
    ap.add_argument('--cli')
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
    for extra in a.extra:
        shutil.copy2(extra, a.dest)
    if a.winscpnet:
        # The assembly looks for winscp.exe beside itself.
        dotnet = os.path.join(a.dest, 'dotnet')
        os.makedirs(dotnet)
        shutil.copy2(a.winscpnet, dotnet)
        shutil.copy2(a.cli, os.path.join(dotnet, 'WinSCP.exe'))
        _, imports = table(a.objdump, a.cli)
        for dll in imports:
            if dll in available:
                shutil.copy2(available[dll], dotnet)
    used = notices(a.qt_prefix, a.dest, a.version)
    print(f'{a.dest}: {len(a.programs)} programs, {len(PLUGINS)} plugins, {len(copied)} DLLs, '
          f'{len(sources)} translations' + (', the .NET assembly' if a.winscpnet else '') +
          f'; licences and the source offer of {used} third-party packages')
    return 0


SOURCES = 'https://repo.msys2.org/mingw/sources/'
LICENCE_FILE = re.compile(r'^(licen[cs]e|copying|notice|copyright)', re.IGNORECASE)
CONTACT = 'ecorreia@apliant.com.br'

OFFER = """\
Source code offer for the third-party components of linscp for Windows

linscp for Windows includes, in binary form, the third-party components listed
below and in THIRD-PARTY-NOTICES.txt, built by the MSYS2 project. Some of them
are licensed under the GNU Lesser General Public License or the GNU General
Public License, which entitle you to their complete corresponding source code.

For at least three years after we last distribute this version of linscp, we
will give anyone who asks a copy of the complete corresponding source code of
each component below, for no more than our reasonable cost of physically
performing the distribution (or at no charge, as a download). Write to
{contact}, naming linscp {version} and the components you want.

Each component is listed below with the address where the MSYS2 project
published its source archive (the upstream source and the build script,
PKGBUILD, that produced the binaries included here). MSYS2 may remove older
archives over time; this offer does not depend on them, as we keep a copy of
every archive listed.

linscp's own source code, for this exact version, is at
https://github.com/99ecarvalho/linscp (tag v{version}).

{components}
"""

NOTICES = """\
Third-party components of linscp for Windows

These components are included in binary form, unmodified, as built by the
MSYS2 project (https://www.msys2.org). Each one's licence texts are in
third-party/<component>/. Where a licence requires it, the source code is
offered in SOURCE-OFFER.txt.

{components}
"""


def notices(qt_prefix, dest, version):
    """Licence texts, notices and the source offer of every MSYS2 package
    something in DEST came from; returns how many packages."""
    root = os.path.dirname(os.path.abspath(qt_prefix))
    index_path = os.path.join(root, 'msys2-qt.packages.json')
    if not os.path.exists(index_path):
        sys.exit(f'{index_path} is missing: run port/tools/fetch_msys2_qt.py --dest {root} again')
    packages = json.load(open(index_path, encoding='utf-8'))
    owner = {}
    for package in packages:
        for path in package['files']:
            owner[path.lower()] = package
    prefix_name = os.path.basename(os.path.abspath(qt_prefix))
    used = {}
    for directory, _, files in os.walk(dest):
        for name in files:
            path = os.path.join(directory, name)
            # What came from PREFIX has the same name there (Windows names:
            # matched without case, listed as deployed).
            shown = os.path.relpath(path, dest).replace(os.sep, '/')
            relative = shown.lower()
            candidates = [f'{prefix_name}/bin/{relative}',
                          f'{prefix_name}/share/qt6/plugins/{relative}',
                          f'{prefix_name}/share/qt6/translations/{os.path.basename(relative)}',
                          f'{prefix_name}/bin/{os.path.basename(relative)}']
            for candidate in candidates:
                if candidate.lower() in owner:
                    package = owner[candidate.lower()]
                    used.setdefault(package['name'], (package, []))[1].append(shown)
                    break
            else:
                # Nothing from PREFIX may ship without its notice.
                for origin in candidates:
                    if os.path.exists(os.path.join(root, origin)):
                        sys.exit(f'{shown}: comes from {origin}, which no package in {index_path} lists')
    third_party = os.path.join(dest, 'licenses', 'third-party')
    os.makedirs(third_party, exist_ok=True)
    offer, listing = [], []
    for name in sorted(used):
        package, deployed = used[name]
        # Its own share/licenses/<dir>/, or a licence file kept elsewhere (ICU's).
        texts = [f for f in package['files']
                 if '/share/licenses/' in f or LICENCE_FILE.search(os.path.basename(f))]
        if not texts:
            sys.exit(f'{name}: the package installs no licence text')
        for text in texts:
            target = os.path.join(third_party, name, os.path.basename(text))
            os.makedirs(os.path.dirname(target), exist_ok=True)
            shutil.copy2(os.path.join(root, text), target)
        source = f'{SOURCES}{package["base"]}-{package["version"]}.src.tar.zst'
        licence = ' AND '.join(package['licenses']) or 'see its licence texts'
        listing.append(f'{name} {package["version"]}\n'
                       f'  Licence: {licence}\n'
                       f'  Project: {package["url"]}\n'
                       f'  Licence texts: third-party/{name}/\n'
                       f'  Files: {", ".join(sorted(deployed))}\n'
                       f'  Source: {source}\n')
        offer.append(f'{name} {package["version"]} ({licence})\n  {source}\n')
    licenses = os.path.join(dest, 'licenses')
    with open(os.path.join(licenses, 'THIRD-PARTY-NOTICES.txt'), 'w', encoding='utf-8', newline='\r\n') as f:
        f.write(NOTICES.format(components='\n'.join(listing)))
    with open(os.path.join(licenses, 'SOURCE-OFFER.txt'), 'w', encoding='utf-8', newline='\r\n') as f:
        f.write(OFFER.format(contact=CONTACT, version=version, components='\n'.join(offer)))
    return len(used)


if __name__ == '__main__':
    sys.exit(main())
