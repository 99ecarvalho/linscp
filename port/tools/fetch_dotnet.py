#!/usr/bin/env python3
"""
Fetch the .NET SDK ctest dotnet runs WinSCP's assembly with.

  fetch_dotnet.py --dest DIR [--version 8.0.425]

Installs the SDK into DIR with Microsoft's dotnet-install script (no root,
nothing outside DIR) and prints the path of DIR/dotnet[.exe]. Does nothing
when that SDK version is already there.
"""
import argparse, os, platform, subprocess, sys, tempfile, urllib.request

VERSION = '8.0.425'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dest', required=True)
    ap.add_argument('--version', default=VERSION)
    a = ap.parse_args()
    windows = platform.system() == 'Windows'
    dotnet = os.path.join(a.dest, 'dotnet.exe' if windows else 'dotnet')
    if not os.path.isdir(os.path.join(a.dest, 'sdk', a.version)):
        script = 'dotnet-install.ps1' if windows else 'dotnet-install.sh'
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, script)
            request = urllib.request.Request(f'https://dot.net/v1/{script}', headers={'User-Agent': 'linscp-fetch-dotnet'})
            with open(path, 'wb') as f:
                f.write(urllib.request.urlopen(request).read())
            if windows:
                cmd = ['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', path,
                       '-Version', a.version, '-InstallDir', a.dest, '-NoPath']
            else:
                cmd = ['bash', path, '--version', a.version, '--install-dir', a.dest, '--no-path']
            subprocess.run(cmd, check=True, stdout=sys.stderr)
    print(dotnet)


if __name__ == '__main__':
    main()
