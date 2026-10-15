#!/usr/bin/env python3
"""Build linscp's Flatpak bundle from this checkout.

  build_flatpak.py [--work build/flatpak] [--out build/flatpak/packages]
                   [--builder-arg=ARG ...]

Runs flatpak-builder on port/packaging/flatpak/<id>.yml (the runtime, SDK
and LLVM extension installed for the user from Flathub when missing), then
exports a single-file bundle, linscp-<version>.flatpak, installable with
`flatpak install --user <file>`.
"""
import argparse, os, re, subprocess, sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
APP_ID = 'io.github._99ecarvalho.linscp'
MANIFEST = os.path.join(ROOT, 'port', 'packaging', 'flatpak', APP_ID + '.yml')


def version():
    text = open(os.path.join(ROOT, 'CMakeLists.txt'), encoding='utf-8').read()
    return re.search(r'project\(linscp VERSION ([0-9.]+)', text).group(1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--work', default=os.path.join(ROOT, 'build', 'flatpak'))
    ap.add_argument('--out')
    # flatpak-builder's own, as CI's --disable-rofiles-fuse (no FUSE in a container).
    ap.add_argument('--builder-arg', action='append', default=[])
    a = ap.parse_args()
    work = os.path.abspath(a.work)
    out = os.path.abspath(a.out or os.path.join(work, 'packages'))
    os.makedirs(out, exist_ok=True)
    repo = os.path.join(work, 'repo')
    subprocess.run(['flatpak-builder', '--user', '--install-deps-from=flathub', '--force-clean'] + a.builder_arg + [
                    '--state-dir', os.path.join(work, 'state'), '--repo', repo,
                    os.path.join(work, 'app'), MANIFEST], check=True)
    bundle = os.path.join(out, f'linscp-{version()}.flatpak')
    subprocess.run(['flatpak', 'build-bundle', '--runtime-repo=https://flathub.org/repo/flathub.flatpakrepo',
                    repo, bundle, APP_ID], check=True)
    print(bundle)
    return 0


if __name__ == '__main__':
    sys.exit(main())
