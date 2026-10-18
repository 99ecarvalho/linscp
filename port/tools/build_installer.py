#!/usr/bin/env python3
"""
Build linscp's Windows installer: the deploy folder and the licenses,
compiled with port/packaging/windows/linscp.iss by the pinned Inno Setup
(fetch_innosetup.py), all under Wine. CMake's installer target runs it.

  build_installer.py --script linscp.iss --version 0.6.1 --deploy build/deploy
                     --work build/installer --tools build/tools --out build/packages
                     --license FILE|NAME ...

Prints the installer's path.
"""
import argparse, os, shutil, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))


def wine_path(path):
    return subprocess.run(['winepath', '-w', path], check=True, capture_output=True, text=True).stdout.strip()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--script', required=True)
    ap.add_argument('--version', required=True)
    ap.add_argument('--deploy', required=True)
    ap.add_argument('--work', required=True)
    ap.add_argument('--tools', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--license', action='append', default=[])
    a = ap.parse_args()

    iscc = subprocess.run([sys.executable, os.path.join(HERE, 'fetch_innosetup.py'), '--dest', a.tools],
                          check=True, stdout=subprocess.PIPE, text=True).stdout.strip().splitlines()[-1]
    licenses = os.path.abspath(os.path.join(a.work, 'licenses'))
    shutil.rmtree(licenses, ignore_errors=True)
    os.makedirs(licenses)
    for entry in a.license:
        source, name = entry.split('|')
        shutil.copyfile(source, os.path.join(licenses, name))
    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    env = dict(os.environ, WINEDEBUG='-all')
    subprocess.run(['wine', iscc, '/Q',
                    f'/DAppVersion={a.version}',
                    f'/DSourceDir={wine_path(os.path.abspath(a.deploy))}',
                    f'/DLicensesDir={wine_path(licenses)}',
                    f'/DOutputDir={wine_path(out)}',
                    wine_path(os.path.abspath(a.script))], check=True, env=env)
    target = os.path.join(out, f'linscp-{a.version}-setup.exe')
    if not os.path.exists(target):
        sys.exit(f'ISCC did not write {target}')
    print(target)


if __name__ == '__main__':
    main()
