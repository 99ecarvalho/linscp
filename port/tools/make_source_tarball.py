#!/usr/bin/env python3
"""
Build linscp's release source tarball, linscp-X.Y.Z.tar.xz: everything a
distribution or Flatpak recipe builds from, offline (RELEASE-PLAN 0.4, R0-14).

  make_source_tarball.py --out DIR [--version X.Y.Z] [--ref HEAD]

Writes DIR/linscp-X.Y.Z.tar.xz and DIR/linscp-X.Y.Z.tar.xz.sha256 (sha256sum
format) and prints the tarball's path. The version defaults to the
project(linscp VERSION ...) of CMakeLists.txt at the ref.

Contents, read from git objects (never the working tree, so local edits and
untracked or ignored files cannot leak in):

  - the files the superproject tracks at the ref;
  - the files of the 3rd-party/winscp, 3rd-party/putty and 3rd-party/openssl
    submodules at the commits the ref records for them. Their own nested
    submodules (OpenSSL's test suites and fuzz corpora) are not included;
  - not the linscp-website submodule, and no .git anywhere.

The vendored upstream tree (3rd-party/winscp) loses what linscp does not
build and may not redistribute (Phase 0 of the release plan):

  libs/mfc/            a subset of a non-free class library; FileZilla's API
                       comes from port/mfc-compat (ADR 0017)
  libs/openssl/        a trimmed copy pre-configured for 32-bit Windows; the
                       build compiles 3rd-party/openssl. Kept: the files
                       port/openssl/winscp-changes.txt lists, which
                       openssl_tree.py compares with the release on every
                       build and fails without
  images               *.ico *.bmp *.png *.jpg *.gif *.cur *.ani *.svg *.tif,
                       licensed for the upstream application only
  deployment/          the upstream installer, its icon and licence texts
  source/**/*.dfm      form files, which embed the upstream images. Kept: the
                       few in KEEP_DFM, which hold no image data and which the
                       qt_gui_menus tests read for the menu layout

Safety checks fail the run when anything that must not ship survives: a
libs/mfc path, a file containing the non-free library's banner, an excluded
image under 3rd-party/winscp, a linscp-website path, or a kept form file with
binary (image) data.

Reproducible: entries sorted, parent directories included, every mtime the
ref's commit time (SOURCE_DATE_EPOCH when set), owner root:root (0/0), modes
0644/0755 from git's executable bit, GNU tar headers (no PAX records), xz at
preset 9e. The same ref gives byte-identical output on any machine.
"""
import argparse, hashlib, lzma, os, re, subprocess, sys, tarfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))

# Submodules shipped in the tarball; any other gitlink must be decided on here.
SUBMODULES = ('3rd-party/winscp', '3rd-party/putty', '3rd-party/openssl')
OMITTED_SUBMODULES = ('linscp-website',)
UPSTREAM = '3rd-party/winscp'

IMAGE_EXTS = ('.ico', '.bmp', '.png', '.jpg', '.gif', '.cur', '.ani', '.svg', '.tif')
# Form files the test suite reads (port/tests/check_menus.py: qt_gui_menus,
# qt_gui_menus_explorer). They reference image lists by name and embed none;
# check_kept_dfm() enforces that.
KEEP_DFM = frozenset({
    'source/forms/NonVisual.dfm',
    'source/forms/ScpCommander.dfm',
    'source/forms/ScpExplorer.dfm',
})
# Split, so that this file does not itself hold the banner it looks for.
MFC_BANNER = b'microsoft ' + b'foundation classes'   # compared lower-cased
OPENSSL_CHANGES = 'port/openssl/winscp-changes.txt'
XZ_PRESET = 9 | lzma.PRESET_EXTREME


def fail(msg):
    sys.exit(f'make_source_tarball: {msg}')


def git(repo, *args, binary=False):
    r = subprocess.run(['git', '-C', repo, *args], capture_output=True)
    if r.returncode != 0:
        fail(f'git -C {repo} {" ".join(args)}: {r.stderr.decode(errors="replace").strip()}')
    return r.stdout if binary else r.stdout.decode('utf-8')


def ls_tree(repo, rev):
    """[(mode, type, object, path)] of every entry of rev's tree, recursively."""
    out = []
    for rec in git(repo, 'ls-tree', '-r', '-z', '--full-tree', rev, binary=True).split(b'\0'):
        if not rec:
            continue
        meta, path = rec.split(b'\t', 1)
        mode, kind, obj = meta.decode().split()
        out.append((mode, kind, obj, path.decode('utf-8')))
    return out


class Blobs:
    """Blob contents through one git cat-file --batch per repository."""

    def __init__(self, repo):
        self.p = subprocess.Popen(['git', '-C', repo, 'cat-file', '--batch'],
                                  stdin=subprocess.PIPE, stdout=subprocess.PIPE)

    def read(self, obj):
        self.p.stdin.write(obj.encode() + b'\n')
        self.p.stdin.flush()
        header = self.p.stdout.readline().split()
        if len(header) != 3 or header[1] != b'blob':
            fail(f'object {obj}: {b" ".join(header).decode(errors="replace")}')
        data = self.p.stdout.read(int(header[2]))
        self.p.stdout.read(1)  # the newline after the contents
        return data

    def close(self):
        self.p.stdin.close()
        self.p.wait()


def project_version(ref):
    text = git(ROOT, 'show', f'{ref}:CMakeLists.txt')
    m = re.search(r'project\(\s*linscp\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)\b', text)
    if not m:
        fail(f'no project(linscp VERSION X.Y.Z) in CMakeLists.txt at {ref}')
    return m.group(1)


def kept_openssl_files(ref):
    """libs/openssl/<file> for each file port/openssl/winscp-changes.txt lists."""
    keep = set()
    for line in git(ROOT, 'show', f'{ref}:{OPENSSL_CHANGES}').split('\n'):
        line = line.split('#', 1)[0].strip()
        if line:
            keep.add('libs/openssl/' + line.split()[0])
    return keep


def upstream_excluded(path, keep_openssl):
    """Why a path of the vendored upstream tree is left out, or None."""
    low = path.lower()
    if path.startswith('libs/mfc/'):
        return 'libs/mfc'
    if path.startswith('libs/openssl/') and path not in keep_openssl:
        return 'libs/openssl'
    if path.startswith('deployment/'):
        return 'deployment'
    if low.endswith(IMAGE_EXTS):
        return 'image'
    if path.startswith('source/') and low.endswith('.dfm') and path not in KEEP_DFM:
        return 'dfm'
    return None


def check_kept_dfm(path, data):
    # A form's binary properties (Bitmap, PngImage.Data, Picture.Data, ...)
    # are written as "Name = {" followed by hex lines.
    if re.search(rb'=\s*\{\s*$', data, re.M):
        fail(f'{path}: a kept form file has binary (image) data; drop it from KEEP_DFM')


def collect(ref):
    """[(tar path, mode, data)] of every file, and the exclusion counts."""
    files = []
    excluded = {}
    gitlinks = {}
    blobs = Blobs(ROOT)
    for mode, kind, obj, path in ls_tree(ROOT, ref):
        if kind == 'commit':
            gitlinks[path] = obj
            continue
        files.append((path, mode, blobs.read(obj)))
    blobs.close()

    unknown = sorted(set(gitlinks) - set(SUBMODULES) - set(OMITTED_SUBMODULES))
    if unknown:
        fail(f'submodules neither shipped nor omitted: {", ".join(unknown)}; '
             'add them to SUBMODULES or OMITTED_SUBMODULES')
    keep_openssl = kept_openssl_files(ref)

    for sub in SUBMODULES:
        if sub not in gitlinks:
            fail(f'{sub}: not a submodule at {ref}')
        commit = gitlinks[sub]
        repo = os.path.join(ROOT, sub)
        if not os.path.exists(os.path.join(repo, '.git')):
            fail(f'{sub}: not initialised (git submodule update --init {sub})')
        if subprocess.run(['git', '-C', repo, 'cat-file', '-e', f'{commit}^{{commit}}'],
                          capture_output=True).returncode != 0:
            fail(f'{sub}: commit {commit} recorded at {ref} is not in the submodule '
                 f'(git submodule update {sub})')
        blobs = Blobs(repo)
        for mode, kind, obj, path in ls_tree(repo, commit):
            if kind == 'commit':
                excluded['nested submodule'] = excluded.get('nested submodule', 0) + 1
                continue
            if sub == UPSTREAM:
                why = upstream_excluded(path, keep_openssl)
                if why:
                    excluded[why] = excluded.get(why, 0) + 1
                    continue
            data = blobs.read(obj)
            if sub == UPSTREAM and path in KEEP_DFM:
                check_kept_dfm(f'{sub}/{path}', data)
            files.append((f'{sub}/{path}', mode, data))
        blobs.close()

    missing = sorted(f for f in keep_openssl
                     if f'{UPSTREAM}/{f}' not in {p for p, _, _ in files})
    if missing:
        fail(f'listed in {OPENSSL_CHANGES} but not in {UPSTREAM}: {", ".join(missing)}')
    return files, excluded


def safety_checks(files):
    for path, _, data in files:
        parts = path.split('/')
        if 'libs/mfc' in path:
            fail(f'{path}: a libs/mfc path would ship')
        if '.git' in parts:
            fail(f'{path}: a .git path would ship')
        if 'linscp-website' in parts:
            fail(f'{path}: a linscp-website path would ship')
        if path.startswith(UPSTREAM + '/') and path.lower().endswith(IMAGE_EXTS):
            fail(f'{path}: an image of the upstream tree would ship')
        if MFC_BANNER in data.lower():
            fail(f'{path}: contains the non-free class library banner')


def write_tarball(target, top, files, mtime):
    entries = {}
    for path, mode, data in files:
        parts = path.split('/')
        for i in range(1, len(parts)):
            entries.setdefault('/'.join(parts[:i]), None)
        entries[path] = (mode, data)

    def info(name, kind, mode):
        ti = tarfile.TarInfo(f'{top}/{name}' if name else top)
        ti.type, ti.mode, ti.mtime = kind, mode, mtime
        ti.uid = ti.gid = 0
        ti.uname = ti.gname = 'root'
        return ti

    tmp = target + '.tmp'
    with open(tmp, 'wb') as raw, \
            lzma.LZMAFile(raw, 'wb', format=lzma.FORMAT_XZ, check=lzma.CHECK_CRC64,
                          preset=XZ_PRESET) as xz, \
            tarfile.open(fileobj=xz, mode='w', format=tarfile.GNU_FORMAT) as tar:
        tar.addfile(info('', tarfile.DIRTYPE, 0o755))
        for name in sorted(entries, key=lambda p: p.split('/')):
            entry = entries[name]
            if entry is None:
                tar.addfile(info(name, tarfile.DIRTYPE, 0o755))
                continue
            mode, data = entry
            if mode == '120000':
                ti = info(name, tarfile.SYMTYPE, 0o777)
                ti.linkname = data.decode('utf-8')
                tar.addfile(ti)
                continue
            if mode not in ('100644', '100755'):
                fail(f'{name}: unexpected git mode {mode}')
            ti = info(name, tarfile.REGTYPE, 0o755 if mode == '100755' else 0o644)
            ti.size = len(data)
            tar.addfile(ti, _Reader(data))
    os.replace(tmp, target)


class _Reader:
    def __init__(self, data):
        self.data, self.pos = data, 0

    def read(self, n=-1):
        end = len(self.data) if n is None or n < 0 else self.pos + n
        chunk = self.data[self.pos:end]
        self.pos += len(chunk)
        return chunk


def main():
    ap = argparse.ArgumentParser(description='Build the release source tarball.')
    ap.add_argument('--out', required=True, help='directory to write the tarball to')
    ap.add_argument('--version', help='X.Y.Z (default: CMakeLists.txt at the ref)')
    ap.add_argument('--ref', default='HEAD', help='superproject commit (default: HEAD)')
    a = ap.parse_args()

    ref = git(ROOT, 'rev-parse', '--verify', f'{a.ref}^{{commit}}').strip()
    version = a.version or project_version(ref)
    if not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+', version):
        fail(f'version {version!r} is not X.Y.Z')
    epoch = os.environ.get('SOURCE_DATE_EPOCH')
    mtime = int(epoch) if epoch else int(git(ROOT, 'log', '-1', '--format=%ct', ref).strip())

    files, excluded = collect(ref)
    safety_checks(files)

    top = f'linscp-{version}'
    os.makedirs(a.out, exist_ok=True)
    target = os.path.join(os.path.abspath(a.out), f'{top}.tar.xz')
    write_tarball(target, top, files, mtime)

    h = hashlib.sha256()
    with open(target, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    with open(target + '.sha256', 'w', encoding='ascii', newline='\n') as f:
        f.write(f'{h.hexdigest()}  {os.path.basename(target)}\n')

    summary = ', '.join(f'{k} {v}' for k, v in sorted(excluded.items()))
    print(f'{len(files)} files from {ref[:12]}; left out: {summary}', file=sys.stderr)
    print(target)
    return 0


if __name__ == '__main__':
    sys.exit(main())
