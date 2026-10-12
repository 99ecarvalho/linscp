#!/usr/bin/env python3
"""
Fetch the rclone release the golden corpus was captured with.

  fetch_rclone.py --dest DIR [--version v1.75.2 | --corpus port/tests/golden/expected]

Downloads the official build for this platform from downloads.rclone.org
into DIR/rclone[.exe] and prints its path. With --corpus, the version is
read from the corpus' SERVERS.txt.
"""
import argparse, io, os, platform, shutil, sys, urllib.request, zipfile


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dest', required=True)
    ap.add_argument('--version')
    ap.add_argument('--corpus')
    a = ap.parse_args()
    version = a.version
    if a.corpus:
        version = open(os.path.join(a.corpus, 'SERVERS.txt'), encoding='utf-8').read().split()[1]
    if not version:
        sys.exit('give --version or --corpus')
    system = {'Linux': 'linux', 'Windows': 'windows', 'Darwin': 'osx'}[platform.system()]
    arch = {'x86_64': 'amd64', 'AMD64': 'amd64', 'aarch64': 'arm64', 'arm64': 'arm64'}[platform.machine()]
    name = f'rclone-{version}-{system}-{arch}'
    exe = 'rclone.exe' if system == 'windows' else 'rclone'
    target = os.path.join(a.dest, exe)
    if not os.path.exists(target):
        # The site refuses Python's default User-Agent.
        request = urllib.request.Request(f'https://downloads.rclone.org/{version}/{name}.zip',
                                         headers={'User-Agent': 'linscp-fetch-rclone'})
        data = urllib.request.urlopen(request).read()
        os.makedirs(a.dest, exist_ok=True)
        with zipfile.ZipFile(io.BytesIO(data)) as z, z.open(f'{name}/{exe}') as src, open(target, 'wb') as out:
            shutil.copyfileobj(src, out)
        os.chmod(target, 0o755)
    print(target)


if __name__ == '__main__':
    main()
