#!/usr/bin/env python3
"""
Collect the source code the packages' source offers name.

  fetch_sources.py --from APPDIR|third-party.json [--from ...] --out DIR [--all]

Each --from is a package's third-party.json: the AppImage's (APPDIR is its
AppDir, or the squashfs-root of an extracted AppImage, holding it at
usr/share/licenses/linscp/; port/tools/build_appimage.py) or the Windows
build's (licenses/third-party.json; port/tools/deploy_windows.py). It lists
each third-party component with its licence and whether that licence is
copyleft (port/tools/licences.py); only the copyleft ones are fetched, unless
--all. The release workflow packs DIR as linscp-<version>-third-party-sources.tar.

A Debian source package (kind "debian": its .dsc and the tarballs it names)
comes from snapshot.debian.org, which keeps every version ever published,
into DIR/debian/<source package>/; each file is checked against the SHA-1
snapshot.debian.org records for it, and the tarballs against the SHA-256
their .dsc lists. Any other component (kind "archive") is a list of files
downloaded from their URLs into DIR/<source>/, checked against their SHA-256
where it is known. Files already in DIR that match are not downloaded again.
Prints what could not be fetched, with its licence, and exits 1 if anything
is missing.
"""
import argparse, hashlib, json, os, sys, time, urllib.error, urllib.parse, urllib.request

SNAPSHOT = 'https://snapshot.debian.org'
INDEXES = [os.path.join('usr', 'share', 'licenses', 'linscp', 'third-party.json'),
           os.path.join('licenses', 'third-party.json')]


def get(url, attempts=4):
    request = urllib.request.Request(url, headers={'User-Agent': 'linscp-fetch-sources'})
    for attempt in range(attempts):
        try:
            with urllib.request.urlopen(request, timeout=300) as response:
                return response.read()
        except urllib.error.HTTPError as e:
            if e.code < 500 or attempt == attempts - 1:
                raise
        except (urllib.error.URLError, TimeoutError):
            if attempt == attempts - 1:
                raise
        time.sleep(5 * (attempt + 1))
    raise AssertionError('unreachable')


def digest(path, algorithm):
    h = hashlib.new(algorithm)
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def dsc_sha256(path):
    """{file name: SHA-256} from a .dsc's Checksums-Sha256 field."""
    result, inside = {}, False
    for line in open(path, encoding='utf-8', errors='replace'):
        if line.startswith('Checksums-Sha256:'):
            inside = True
        elif inside and line.startswith(' '):
            sha, _, name = line.split()
            result[name] = sha
        elif inside:
            break
    return result


def fetch_archive(entry, out):
    """Downloads one component's files; returns (bytes on disk, problems)."""
    directory = os.path.join(out, entry['source'])
    os.makedirs(directory, exist_ok=True)
    size, problems = 0, []
    for f in entry['files']:
        name = f['name']
        if '/' in name or name.startswith('.'):
            return 0, [f'{entry["source"]}: unexpected file name {name}']
        target = os.path.join(directory, name)
        if not (os.path.isfile(target) and f.get('sha256') in (None, digest(target, 'sha256'))):
            data = get(f['url'])
            if f.get('sha256') and hashlib.sha256(data).hexdigest() != f['sha256']:
                problems.append(f'{name}: SHA-256 does not match {f["url"]}')
                continue
            with open(target + '.part', 'wb') as part:
                part.write(data)
            os.replace(target + '.part', target)
            print(target)
        size += os.path.getsize(target)
    return size, problems


def fetch(entry, out):
    """Downloads one Debian source package; returns (bytes on disk, problems)."""
    src, version = entry['source'], entry['version']
    quoted = f'{urllib.parse.quote(src, safe="")}/{urllib.parse.quote(version, safe="")}'
    info = json.loads(get(f'{SNAPSHOT}/mr/package/{quoted}/srcfiles?fileinfo=1'))
    directory = os.path.join(out, 'debian', src)
    os.makedirs(directory, exist_ok=True)
    # A file unchanged across versions (an extra upstream tarball, a
    # signature) is one hash known by every version's name: the .dsc says
    # which one this version uses.
    names = {sha1: sorted({s['name'] for s in seen}) for sha1, seen in info['fileinfo'].items()}
    dsc = f'{src}_{version.split(":", 1)[-1]}.dsc'
    dsc_hash = next((h for h, n in names.items() if dsc in n), None)
    if dsc_hash is None:
        return 0, [f'{src} {version}: no {dsc} on snapshot.debian.org']
    download(directory, dsc, dsc_hash)
    listed = dsc_sha256(os.path.join(directory, dsc))
    files = {dsc: dsc_hash}
    for sha1, candidates in sorted(names.items()):
        chosen = [n for n in candidates if n in listed]
        if len(chosen) == 1:
            files[chosen[0]] = sha1
    problems = []
    for name, sha in sorted(listed.items()):
        if '/' in name or name.startswith('.'):
            return 0, [f'{src} {version}: unexpected file name {name} in {dsc}']
        if name not in files:
            problems.append(f'{name}: listed in {dsc}, not on snapshot.debian.org')
            continue
        path = download(directory, name, files[name])
        if digest(path, 'sha256') != sha:
            problems.append(f'{name}: SHA-256 does not match {dsc}')
    size = sum(os.path.getsize(os.path.join(directory, n)) for n in files)
    return size, problems


def download(directory, name, sha1):
    """DIRECTORY/NAME, the snapshot.debian.org file SHA1, unless already there."""
    target = os.path.join(directory, name)
    if os.path.isfile(target) and digest(target, 'sha1') == sha1:
        return target
    data = get(f'{SNAPSHOT}/file/{sha1}')
    if hashlib.sha1(data).hexdigest() != sha1:
        raise ValueError(f'{name}: SHA-1 does not match snapshot.debian.org\'s')
    with open(target + '.part', 'wb') as f:
        f.write(data)
    os.replace(target + '.part', target)
    print(target)
    return target


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--from', dest='sources', action='append', required=True,
                    help='a third-party.json, or the folder holding it (an AppDir, a Windows build)')
    ap.add_argument('--out', required=True)
    ap.add_argument('--all', action='store_true', help='the permissively licensed components too')
    a = ap.parse_args()
    entries = []
    for source in a.sources:
        index = next((os.path.join(source, i) for i in INDEXES if os.path.isfile(os.path.join(source, i))),
                     source) if os.path.isdir(source) else source
        entries += [e for e in json.load(open(index, encoding='utf-8')) if a.all or e['copyleft']]
    total, missing = 0, []
    for entry in sorted(entries, key=lambda e: (e['source'], e['version'])):
        try:
            size, problems = (fetch if entry['kind'] == 'debian' else fetch_archive)(entry, a.out)
        except (urllib.error.URLError, OSError, ValueError, KeyError) as e:
            size, problems = 0, [f'{entry["source"]} {entry["version"]}: {e}']
        total += size
        missing += [f'{p} (licence: {entry["licence"]})' for p in problems]
    print(f'{a.out}: {len(entries)} components, {total / (1 << 20):.1f} MiB')
    for line in missing:
        print(f'not fetched: {line}', file=sys.stderr)
    return 1 if missing else 0


if __name__ == '__main__':
    sys.exit(main())
