#!/usr/bin/env python3
"""
Fetch the Inno Setup compiler the Windows installer is built with
(github.com/jrsoftware/issrc), as WinSCP's is: Inno Setup 6.

  fetch_innosetup.py --dest build/tools

Runs the pinned release's own installer under Wine, portable (nothing
registered in the Wine prefix), into DEST/innosetup-<version>, and prints the
path of its ISCC.exe; port/packaging/CMakeLists.txt runs that under Wine too.
Checked against the pinned SHA-256.
"""
import argparse, hashlib, os, subprocess, sys, tempfile, urllib.request

VERSION = '6.7.3'
SHA256 = '9c73c3bae7ed48d44112a0f48e66742c00090bdb5bef71d9d3c056c66e97b732'
NAME = f'innosetup-{VERSION}.exe'
URL = f'https://github.com/jrsoftware/issrc/releases/download/is-{VERSION.replace(".", "_")}/{NAME}'


def wine_path(path):
    return subprocess.run(['winepath', '-w', path], check=True, capture_output=True, text=True).stdout.strip()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dest', required=True)
    a = ap.parse_args()
    target = os.path.abspath(os.path.join(a.dest, f'innosetup-{VERSION}'))
    iscc = os.path.join(target, 'ISCC.exe')
    if not os.path.exists(iscc):
        request = urllib.request.Request(URL, headers={'User-Agent': 'linscp-fetch-innosetup'})
        data = urllib.request.urlopen(request).read()
        digest = hashlib.sha256(data).hexdigest()
        if digest != SHA256:
            sys.exit(f'{NAME}: SHA-256 {digest}, expected {SHA256}')
        os.makedirs(target, exist_ok=True)
        with tempfile.TemporaryDirectory() as temp:
            setup = os.path.join(temp, NAME)
            with open(setup, 'wb') as f:
                f.write(data)
            env = dict(os.environ, WINEDEBUG='-all')
            subprocess.run(['wine', setup, '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/CURRENTUSER',
                            '/PORTABLE=1', f'/DIR={wine_path(target)}'], check=True, env=env)
        if not os.path.exists(iscc):
            sys.exit(f'{NAME} did not install {iscc}')
    print(iscc)


if __name__ == '__main__':
    main()
