#!/usr/bin/env python3
"""Build linscp's AppImage from a configured Linux build tree.

  build_appimage.py --build build/linux [--out build/linux/packages]

Fetches the pinned tools (port/packaging/appimage/tools.lock) into
<build>/../tools/appimage, checking each one's SHA-256; installs linscp into
an AppDir (cmake --install, prefix /usr); has linuxdeploy and its Qt plugin
bring in the libraries and Qt's plugins; and packs it with appimagetool.
The tools run extracted (APPIMAGE_EXTRACT_AND_RUN), so no FUSE is needed.

The AppImage runs on distributions whose glibc is at least the build
machine's: build it on the oldest system to support (CI's Ubuntu).
"""
import argparse, hashlib, os, shutil, stat, subprocess, sys, urllib.request

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
LOCK = os.path.join(ROOT, 'port', 'packaging', 'appimage', 'tools.lock')
APP_ID = 'io.github.99ecarvalho.linscp'


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def fetch_tools(directory):
    os.makedirs(directory, exist_ok=True)
    tools = {}
    for line in open(LOCK):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        name, url, digest = line.split()
        path = os.path.join(directory, name)
        if not os.path.isfile(path) or sha256(path) != digest:
            print(f'fetching {name}')
            urllib.request.urlretrieve(url, path + '.part')
            if sha256(path + '.part') != digest:
                os.unlink(path + '.part')
                sys.exit(f'{name}: SHA-256 does not match {LOCK}')
            os.replace(path + '.part', path)
        os.chmod(path, os.stat(path).st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
        tools[name] = path
    return tools


def cache_value(build, key):
    for line in open(os.path.join(build, 'CMakeCache.txt'), encoding='utf-8'):
        if line.startswith(key + ':'):
            return line.split('=', 1)[1].strip()
    return ''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--build', required=True)
    ap.add_argument('--out')
    a = ap.parse_args()
    build = os.path.abspath(a.build)
    out = os.path.abspath(a.out or os.path.join(build, 'packages'))
    tools = fetch_tools(os.path.join(os.path.dirname(build), 'tools', 'appimage'))
    version = cache_value(build, 'CMAKE_PROJECT_VERSION') or '0'

    subprocess.run(['cmake', '--build', build, '--target', 'linscp', 'linscp-cli', 'core-translations'], check=True)
    appdir = os.path.join(build, 'AppDir')
    shutil.rmtree(appdir, ignore_errors=True)
    env = dict(os.environ, DESTDIR=appdir)
    subprocess.run(['cmake', '--install', build, '--prefix', '/usr', '--strip'], check=True, env=env)

    usr = os.path.join(appdir, 'usr')
    real = os.path.join(usr, 'lib', 'linscp')
    env = dict(os.environ, APPIMAGE_EXTRACT_AND_RUN='1', QMAKE=shutil.which('qmake6') or 'qmake6',
               # The icon sets are SVGs; Qt's SVG icon engine and image format.
               EXTRA_QT_PLUGINS='svg;iconengines',
               EXTRA_PLATFORM_PLUGINS='libqwayland-generic.so;libqoffscreen.so',
               LDAI_VERBOSE='0', PATH=os.path.dirname(tools['linuxdeploy-plugin-qt-x86_64.AppImage']) + os.pathsep + os.environ['PATH'])
    # (The plugin is found on PATH by its name, without the architecture.)
    plugin_link = os.path.join(os.path.dirname(tools['linuxdeploy-plugin-qt-x86_64.AppImage']), 'linuxdeploy-plugin-qt')
    if not os.path.exists(plugin_link):
        os.symlink(os.path.basename(tools['linuxdeploy-plugin-qt-x86_64.AppImage']), plugin_link)
    # Wayland's integration plugins, which the Qt plugin deploys only for
    # programs linking Qt's Wayland client: copied in, their libraries
    # brought by linuxdeploy (--deploy-deps-only).
    qt_plugins = subprocess.run([env['QMAKE'], '-query', 'QT_INSTALL_PLUGINS'], check=True, capture_output=True,
                                text=True).stdout.strip()
    wayland = []
    for name in ('wayland-shell-integration', 'wayland-decoration-client', 'wayland-graphics-integration-client'):
        source = os.path.join(qt_plugins, name)
        if os.path.isdir(source):
            target = os.path.join(usr, 'plugins', name)
            shutil.copytree(source, target, dirs_exist_ok=True)
            wayland += ['--deploy-deps-only', target]
    subprocess.run([tools['linuxdeploy-x86_64.AppImage'], '--appdir', appdir] + wayland + [
                    '--executable', os.path.join(real, 'linscp'),
                    '--executable', os.path.join(real, 'linscp-cli'),
                    '--desktop-file', os.path.join(usr, 'share', 'applications', APP_ID + '.desktop'),
                    '--icon-file', os.path.join(usr, 'share', 'icons', 'hicolor', '256x256', 'apps', APP_ID + '.png'),
                    '--plugin', 'qt'], check=True, env=env)
    # Qt's paths for the real executable, which lives beside core's
    # Translations rather than in usr/bin: the same qt.conf.
    conf = os.path.join(usr, 'bin', 'qt.conf')
    if os.path.isfile(conf):
        with open(conf) as f:
            text = f.read()
        with open(os.path.join(real, 'qt.conf'), 'w') as f:
            f.write(text.replace('Prefix = ../', 'Prefix = ../../'))
    os.makedirs(out, exist_ok=True)
    target = os.path.join(out, f'linscp-{version}-x86_64.AppImage')
    subprocess.run([tools['appimagetool-x86_64.AppImage'], '--no-appstream', appdir, target], check=True,
                   env=dict(env, ARCH='x86_64'))
    print(target)
    return 0


if __name__ == '__main__':
    sys.exit(main())
