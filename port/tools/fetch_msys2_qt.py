#!/usr/bin/env python3
"""
Fetch the Windows Qt the cross-build links against: MSYS2's clang64 packages
(clang, libc++, UCRT -- llvm-mingw's ABI), pinned by port/qt/msys2-qt.lock.

  fetch_msys2_qt.py --dest DIR                  download the lock's packages into DIR
  fetch_msys2_qt.py --resolve [--qt 6.8.2-1]    rewrite the lock

The Qt version is Debian's own, so that Debian's moc, uic and rcc serve as the
cross-build's host tools (QT_HOST_PATH). MSYS2 keeps old package files but
not old repository databases, so --resolve rebuilds the repository as it was
when that Qt was built: each dependency at the newest version uploaded by
then, followed recursively through the packages' .PKGINFO. The lock records
every file and its SHA-256; a download that does not match fails.
Unpacks with `tar --zstd`; prints DIR/clang64.
"""
import argparse, datetime, hashlib, io, os, re, subprocess, sys, tarfile, tempfile, urllib.parse, urllib.request

REPO = 'https://repo.msys2.org/mingw/clang64/'
PREFIX = 'mingw-w64-clang-x86_64-'
LOCK = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'qt', 'msys2-qt.lock')
# Grace after Qt's upload for its dependencies' contemporaries.
WINDOW = datetime.timedelta(days=1)
# The C and C++ runtime is fetched too, as Qt was built against it: MSYS2's
# libc++.dll exports the unwinder (_Unwind_Resume) that llvm-mingw's keeps in
# libunwind.dll, so Qt does not load with llvm-mingw's. linscp is compiled by
# the llvm-mingw of the same LLVM release (cmake/toolchain-llvm-mingw.cmake).
ROOTS = ['qt6-base', 'libc++', 'libunwind', 'libwinpthread']
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


def resolve(qt):
    packages = listing()
    qt_files = [p for p in packages['qt6-base'] if p[0] == qt]
    if not qt_files:
        sys.exit(f'qt6-base {qt} is not in the repository')
    snapshot = qt_files[0][1] + WINDOW
    chosen, todo = {}, list(ROOTS)
    while todo:
        name = todo.pop()
        if name in chosen or name in SKIP:
            continue
        versions = [p for p in packages.get(name, []) + packages.get(name + '-git', []) if p[1] <= snapshot]
        if name == 'qt6-base':
            versions = qt_files
        if not versions:
            sys.exit(f'{name}: no version uploaded by {snapshot}')
        version, _, filename = max(versions, key=lambda p: p[1])
        data = get(REPO + urllib.parse.quote(filename))
        chosen[name] = (filename, hashlib.sha256(data).hexdigest())
        for dep in pkginfo(data):
            dep = re.split(r'[<>=]', dep, maxsplit=1)[0]
            if dep.startswith(PREFIX):
                todo.append(dep[len(PREFIX):])
    with open(LOCK, 'w', newline='\n') as f:
        f.write(f'# MSYS2 clang64 packages for Qt {qt}, as the repository stood on {snapshot:%Y-%m-%d}:\n'
                '# port/tools/fetch_msys2_qt.py --resolve. <sha256> <file>\n')
        for name in sorted(chosen):
            filename, digest = chosen[name]
            f.write(f'{digest} {filename}\n')
    print(f'{len(chosen)} packages locked')


def fetch(dest):
    root = os.path.join(dest, 'clang64')
    stamp = os.path.join(dest, 'msys2-qt.lock')
    lock = open(LOCK, encoding='utf-8').read()
    if os.path.exists(stamp) and open(stamp, encoding='utf-8').read() == lock:
        print(root)
        return
    os.makedirs(dest, exist_ok=True)
    for line in lock.splitlines():
        if not line or line.startswith('#'):
            continue
        digest, filename = line.split()
        data = get(REPO + urllib.parse.quote(filename))
        if hashlib.sha256(data).hexdigest() != digest:
            sys.exit(f'{filename}: SHA-256 does not match the lock')
        with tempfile.NamedTemporaryFile(suffix='.pkg.tar.zst', delete=False) as f:
            f.write(data)
        try:
            subprocess.run(['tar', '--zstd', '-xf', f.name, '-C', dest, 'clang64'], check=True)
        finally:
            os.unlink(f.name)
    with open(stamp, 'w', encoding='utf-8', newline='\n') as f:
        f.write(lock)
    print(root)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dest')
    ap.add_argument('--resolve', action='store_true')
    ap.add_argument('--qt', default='6.8.2-1')
    a = ap.parse_args()
    if a.resolve:
        resolve(a.qt)
    elif a.dest:
        fetch(a.dest)
    else:
        ap.error('give --dest or --resolve')


if __name__ == '__main__':
    main()
