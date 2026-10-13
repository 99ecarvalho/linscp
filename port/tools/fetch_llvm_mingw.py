#!/usr/bin/env python3
"""
Fetch the llvm-mingw toolchain the Windows build uses (github.com/mstorsjo/llvm-mingw).

  fetch_llvm_mingw.py --dest build/tools

Unpacks the pinned release into DEST/llvm-mingw-<release>-ucrt-ubuntu-20.04-x86_64
and prints that path; cmake/toolchain-llvm-mingw.cmake finds it there. The
release's LLVM is the one MSYS2 built the pinned Qt's C++ runtime with
(port/qt/msys2-qt.lock: libc++ 19.1.7), so the code linscp compiles needs
nothing that runtime lacks. Checked against the pinned SHA-256.
"""
import argparse, hashlib, os, subprocess, sys, tempfile, urllib.request

RELEASE = '20250114'  # LLVM 19.1.7
SHA256 = 'a16f52dee819797248e6c7d63b8b1e50a92119f45767ecd8e9633d1733b896e2'
NAME = f'llvm-mingw-{RELEASE}-ucrt-ubuntu-20.04-x86_64'
URL = f'https://github.com/mstorsjo/llvm-mingw/releases/download/{RELEASE}/{NAME}.tar.xz'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dest', required=True)
    a = ap.parse_args()
    target = os.path.join(a.dest, NAME)
    if not os.path.isdir(os.path.join(target, 'bin')):
        request = urllib.request.Request(URL, headers={'User-Agent': 'linscp-fetch-llvm-mingw'})
        data = urllib.request.urlopen(request).read()
        digest = hashlib.sha256(data).hexdigest()
        if digest != SHA256:
            sys.exit(f'{NAME}.tar.xz: SHA-256 {digest}, expected {SHA256}')
        os.makedirs(a.dest, exist_ok=True)
        with tempfile.NamedTemporaryFile(suffix='.tar.xz', delete=False) as f:
            f.write(data)
        try:
            subprocess.run(['tar', '-xJf', f.name, '-C', a.dest], check=True)
        finally:
            os.unlink(f.name)
    print(target)


if __name__ == '__main__':
    main()
