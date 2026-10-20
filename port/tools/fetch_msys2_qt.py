#!/usr/bin/env python3
"""
Fetch the Windows Qt the cross-build links against: MSYS2's clang64 packages
(clang, libc++, UCRT -- llvm-mingw's ABI), pinned by port/qt/msys2-qt.lock.

  fetch_msys2_qt.py --dest DIR                  download the lock's packages into DIR
  fetch_msys2_qt.py --dest DIR --sources OUT    and their source archives into OUT
  fetch_msys2_qt.py --resolve [--qt 6.8.2-1]    rewrite the lock

The Qt version is Debian's own, so that Debian's moc, uic and rcc serve as the
cross-build's host tools (QT_HOST_PATH). MSYS2 keeps old package files but
not old repository databases, so --resolve rebuilds the repository as it was
when that Qt was built: each dependency at the newest version uploaded by
then, followed recursively through the packages' .PKGINFO. Where every file
of a package uploaded by then has since been deleted, the oldest one left
stands in, and the lock's header names it. The lock records
every file and its SHA-256; a download that does not match fails.
Unpacks with `tar --zstd`; prints DIR/clang64. Beside it, DIR/msys2-qt.packages.json
records each package's name, base, version, licences and files, from which
the deploy folder's third-party notices and source offer are written.
"""
import argparse, datetime, hashlib, io, json, os, re, subprocess, sys, tarfile, tempfile, urllib.error, urllib.parse, urllib.request

REPO = 'https://repo.msys2.org/mingw/clang64/'
SOURCES = 'https://repo.msys2.org/mingw/sources/'
PREFIX = 'mingw-w64-clang-x86_64-'
LOCK = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'qt', 'msys2-qt.lock')
# Beside clang64: each package's name, base, version, licences and files.
INDEX = 'msys2-qt.packages.json'
# Grace after Qt's upload for its dependencies' contemporaries.
WINDOW = datetime.timedelta(days=1)
# The C and C++ runtime is fetched too, as Qt was built against it: MSYS2's
# libc++.dll exports the unwinder (_Unwind_Resume) that llvm-mingw's keeps in
# libunwind.dll, so Qt does not load with llvm-mingw's. linscp is compiled by
# the llvm-mingw of the same LLVM release (cmake/toolchain-llvm-mingw.cmake).
# qt6-translations: Qt's own texts (standard buttons and dialogs), as
# Debian's qt6-translations-l10n gives them on Linux. qt6-svg: the icon
# sets (assets/icons), drawn from their SVGs as qt6-svg-dev does on Linux.
ROOTS = ['qt6-base', 'qt6-svg', 'qt6-translations', 'libc++', 'libunwind', 'libwinpthread']
# Not fetched: gcc-libs is a virtual package for that runtime; the Vulkan
# headers are a build-time dependency only, and so is Python with what it
# brings (glib2's code generators): Qt links none of them.
SKIP = {'gcc-libs', 'vulkan-headers',
        'python', 'python-packaging', 'mpdecimal', 'tcl', 'tk', 'tzdata', 'ncurses', 'readline',
        'termcap', 'wineditline'}


def get(url):
    request = urllib.request.Request(url, headers={'User-Agent': 'linscp-fetch-msys2-qt'})
    return urllib.request.urlopen(request).read()


def listing():
    """{package name: [(version, uploaded, file name), ...]}"""
    # Names are URL-encoded in the index (libc%2B%2B).
    html = urllib.parse.unquote(get(REPO).decode())
    rx = re.compile(r'<a href="(' + re.escape(PREFIX) + r'([^"]+)-([^-"]+-[^-"]+)-any\.pkg\.tar\.zst)">[^<]*</a>\s+'
                    r'(\d\d-\w\w\w-\d{4} \d\d:\d\d)')
    result = {}
    for m in rx.finditer(html):
        uploaded = datetime.datetime.strptime(m.group(4), '%d-%b-%Y %H:%M')
        result.setdefault(m.group(2), []).append((m.group(3), uploaded, m.group(1)))
    return result


def pkginfo(data):
    with tarfile.open(fileobj=io.BytesIO(subprocess.run(['zstd', '-dc'], input=data, capture_output=True,
                                                         check=True).stdout)) as t:
        text = t.extractfile('.PKGINFO').read().decode()
    return [l.split(' = ', 1)[1] for l in text.splitlines() if l.startswith('depend = ')]


def index(data, filename):
    """What a package is and installs: the third-party notices and the
    source offer the deploy folder carries (deploy_windows.py) come from it."""
    with tarfile.open(fileobj=io.BytesIO(subprocess.run(['zstd', '-dc'], input=data, capture_output=True,
                                                         check=True).stdout)) as t:
        fields = {}
        for line in t.extractfile('.PKGINFO').read().decode().splitlines():
            if ' = ' in line:
                key, value = line.split(' = ', 1)
                fields.setdefault(key, []).append(value)
        files = sorted(m.name for m in t.getmembers() if m.isfile() and m.name.startswith('clang64/'))
    name = fields['pkgname'][0]
    return {'name': name[len(PREFIX):] if name.startswith(PREFIX) else name,
            'base': fields['pkgbase'][0], 'version': fields['pkgver'][0],
            'licenses': [l[len('spdx:'):] if l.startswith('spdx:') else l for l in fields.get('license', [])],
            'url': fields.get('url', [''])[0], 'file': filename, 'files': files}


def resolve(qt):
    packages = listing()
    qt_files = [p for p in packages['qt6-base'] if p[0] == qt]
    if not qt_files:
        sys.exit(f'qt6-base {qt} is not in the repository')
    snapshot = qt_files[0][1] + WINDOW
    chosen, todo, later = {}, list(ROOTS), []
    while todo:
        name = todo.pop()
        if name in chosen or name in SKIP:
            continue
        candidates = packages.get(name, []) + packages.get(name + '-git', [])
        versions = [p for p in candidates if p[1] <= snapshot]
        if name == 'qt6-base':
            versions = qt_files
        elif name.startswith('qt6-'):
            # Qt's other modules at Qt's own version, whenever uploaded.
            versions = [p for p in packages.get(name, []) if p[0] == qt]
            if not versions:
                sys.exit(f'{name}: no {qt} in the repository')
        if versions:
            _, _, filename = max(versions, key=lambda p: p[1])
        elif candidates:
            # The repository has since deleted every file uploaded by then:
            # the oldest one left, the nearest to that state.
            _, uploaded, filename = min(candidates, key=lambda p: p[1])
            later.append((name, uploaded, filename))
        else:
            sys.exit(f'{name}: not in the repository')
        data = get(REPO + urllib.parse.quote(filename))
        chosen[name] = (filename, hashlib.sha256(data).hexdigest())
        for dep in pkginfo(data):
            dep = re.split(r'[<>=]', dep, maxsplit=1)[0]
            if dep.startswith(PREFIX):
                todo.append(dep[len(PREFIX):])
    with open(LOCK, 'w', newline='\n') as f:
        f.write(f'# MSYS2 clang64 packages for Qt {qt}, as the repository stood on {snapshot:%Y-%m-%d}:\n'
                '# port/tools/fetch_msys2_qt.py --resolve. <sha256> <file>\n')
        if later:
            f.write('# No file uploaded by then remains of these; locked instead, the oldest left:\n')
        for name, uploaded, filename in sorted(later):
            f.write(f'#   {name}: {filename[len(PREFIX):]}, uploaded {uploaded:%Y-%m-%d}\n')
        for name in sorted(chosen):
            filename, digest = chosen[name]
            f.write(f'{digest} {filename}\n')
    print(f'{len(chosen)} packages locked')


def fetch(dest):
    root = os.path.join(dest, 'clang64')
    stamp = os.path.join(dest, 'msys2-qt.lock')
    packages = os.path.join(dest, INDEX)
    lock = open(LOCK, encoding='utf-8').read()
    unpacked = os.path.exists(stamp) and open(stamp, encoding='utf-8').read() == lock
    if unpacked and os.path.exists(packages):
        print(root)
        return
    # A tree unpacked before the index existed only gains the index.
    os.makedirs(dest, exist_ok=True)
    entries, gone = [], []
    for line in lock.splitlines():
        if not line or line.startswith('#'):
            continue
        digest, filename = line.split()
        try:
            data = get(REPO + urllib.parse.quote(filename))
        except urllib.error.HTTPError as e:
            if e.code != 404 or not unpacked:
                sys.exit(f'{filename}: {e} -- the repository no longer has it: --resolve the lock again')
            gone.append(filename)
            continue
        if hashlib.sha256(data).hexdigest() != digest:
            sys.exit(f'{filename}: SHA-256 does not match the lock')
        entries.append(index(data, filename))
        if unpacked:
            continue
        with tempfile.NamedTemporaryFile(suffix='.pkg.tar.zst', delete=False) as f:
            f.write(data)
        try:
            subprocess.run(['tar', '--zstd', '-xf', f.name, '-C', dest, 'clang64'], check=True)
        finally:
            os.unlink(f.name)
    # Unpacked here but since removed from the repository: the files the same
    # package installs today (the names do not change between releases), the
    # locked version, and a note that MSYS2 no longer publishes it.
    if gone:
        available = listing()
        for filename in gone:
            name, version, release = filename[len(PREFIX):-len('-any.pkg.tar.zst')].rsplit('-', 2)
            if name not in available:
                sys.exit(f'{filename}: removed from the repository, and no other {name}: --resolve the lock again')
            _, _, newest = max(available[name], key=lambda p: p[1])
            entry = index(get(REPO + urllib.parse.quote(newest)), filename)
            entry.update(version=f'{version}-{release}', unavailable=True)
            entries.append(entry)
            print(f'warning: {filename} is no longer in the repository; its files are taken from {newest}',
                  file=sys.stderr)
        entries.sort(key=lambda e: e['name'])
    with open(packages, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(entries, f, indent=1)
        f.write('\n')
    with open(stamp, 'w', encoding='utf-8', newline='\n') as f:
        f.write(lock)
    print(root)


def sources(dest, out):
    """The source archive of every package in DEST's index, into OUT: what
    linscp's source offer (deploy_windows.py) promises for three years, kept
    by us because the repository drops old archives."""
    entries = json.load(open(os.path.join(dest, INDEX), encoding='utf-8'))
    os.makedirs(out, exist_ok=True)
    missing = []
    for entry in entries:
        name = f'{entry["base"]}-{entry["version"]}.src.tar.zst'
        target = os.path.join(out, name)
        if os.path.exists(target):
            continue
        try:
            data = get(SOURCES + urllib.parse.quote(name))
        except urllib.error.HTTPError as e:
            missing.append(f'{name} ({e.code}; licence: {" AND ".join(entry["licenses"])})')
            continue
        with open(target, 'wb') as f:
            f.write(data)
        print(target)
    for line in missing:
        print(f'not in the repository: {line}', file=sys.stderr)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dest')
    ap.add_argument('--resolve', action='store_true')
    ap.add_argument('--sources', metavar='OUT', help='with --dest: download the packages\' source archives into OUT')
    ap.add_argument('--qt', default='6.8.2-1')
    a = ap.parse_args()
    if a.resolve:
        resolve(a.qt)
    elif a.dest and a.sources:
        fetch(a.dest)
        sources(a.dest, a.sources)
    elif a.dest:
        fetch(a.dest)
    else:
        ap.error('give --dest or --resolve')


if __name__ == '__main__':
    main()
