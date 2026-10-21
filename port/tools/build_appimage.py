#!/usr/bin/env python3
"""Build linscp's AppImage from a configured Linux build tree.

  build_appimage.py --build build/linux [--out build/linux/packages]

Fetches the pinned tools (port/packaging/appimage/tools.lock) into
<build>/../tools/appimage, checking each one's SHA-256; installs linscp into
an AppDir (cmake --install, prefix /usr); has linuxdeploy and its Qt plugin
bring in the libraries and Qt's plugins; and packs it with appimagetool.
The tools run extracted (APPIMAGE_EXTRACT_AND_RUN), so no FUSE is needed.

The AppImage runs on distributions whose glibc is at least the build
machine's: build it on the oldest system to support (CI builds it on Debian
trixie, as the port's toolchain needs).

What linuxdeploy brings in comes from the build machine's Debian packages.
Only the Qt plugins PLUGINS lists are kept, and the libraries they and linscp
need. Before packing, usr/share/licenses/linscp/ gets linscp's own licences,
and third-party/<package>/ the Debian copyright file of every package a
bundled file comes from (dpkg -S), with the /usr/share/common-licenses texts
it refers to; THIRD-PARTY-NOTICES.txt lists them. The licence of what is
shipped from each source package is in port/packaging/appimage/licences.json;
SOURCE-OFFER.txt names the copyleft ones, whose source code the release
carries (linscp-<version>-third-party-sources.tar, which
port/tools/fetch_sources.py collects from third-party.json). A bundled file
no package owns, a package without a copyright file, or a source package
licences.json does not list fails the build.
"""
import argparse, hashlib, json, os, re, shutil, stat, subprocess, sys, urllib.parse, urllib.request

import licences

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
LOCK = os.path.join(ROOT, 'port', 'packaging', 'appimage', 'tools.lock')
APP_ID = 'org.linscp.linscp'


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


# The Qt plugins the AppImage ships, by folder. The Qt plugin deploys every
# plugin of a kind it finds on the build machine (a desktop with KDE installed
# has two dozen more image formats, each with its libraries); the rest are
# removed, with the libraries only they needed. Image formats: those the file
# panels' thumbnails read beyond Qt's built-in PNG, BMP and XPM.
PLUGINS = {
    'iconengines': {'libqsvgicon.so'},
    'imageformats': {'libqgif.so', 'libqico.so', 'libqjpeg.so', 'libqsvg.so'},
    'platforminputcontexts': {'libcomposeplatforminputcontextplugin.so', 'libibusplatforminputcontextplugin.so'},
    'platforms': {'libqxcb.so', 'libqwayland-generic.so', 'libqoffscreen.so'},
    'tls': {'libqopensslbackend.so'},
    'wayland-decoration-client': {'libadwaita.so', 'libbradient.so'},
    'wayland-graphics-integration-client': {'libqt-plugin-wayland-egl.so'},
    'wayland-shell-integration': {'libxdg-shell.so'},
    'xcbglintegrations': {'libqxcb-egl-integration.so', 'libqxcb-glx-integration.so'},
}
# Without these linscp does not start, or draws no icons.
REQUIRED = {'iconengines/libqsvgicon.so', 'imageformats/libqsvg.so', 'platforms/libqxcb.so',
            'platforms/libqwayland-generic.so', 'wayland-shell-integration/libxdg-shell.so'}
NEEDED = re.compile(r'\(NEEDED\)\s+Shared library: \[([^\]]+)\]')


def prune(appdir):
    """Removes the plugins PLUGINS does not list, then the bundled libraries
    nothing left needs."""
    plugins = os.path.join(appdir, 'usr', 'plugins')
    for kind in sorted(os.listdir(plugins)):
        for name in sorted(os.listdir(os.path.join(plugins, kind))):
            if name not in PLUGINS.get(kind, ()):
                os.unlink(os.path.join(plugins, kind, name))
        if not os.listdir(os.path.join(plugins, kind)):
            os.rmdir(os.path.join(plugins, kind))
    missing = sorted(p for p in REQUIRED if not os.path.isfile(os.path.join(plugins, p)))
    if missing:
        sys.exit(f'Qt plugins missing from the AppImage: {", ".join(missing)}')
    lib = os.path.join(appdir, 'usr', 'lib')

    def needed(path):
        out = subprocess.run(['readelf', '-dW', path], capture_output=True, text=True).stdout
        return NEEDED.findall(out)

    # Everything but usr/lib's libraries is a root: the executables and plugins.
    queue = [os.path.join(appdir, f) for f in tree(appdir)
             if os.path.dirname(f) != 'usr/lib' and f.endswith(('.so', 'linscp', 'linscp-cli'))]
    reached = set()
    while queue:
        for name in needed(queue.pop()):
            if name not in reached and os.path.exists(os.path.join(lib, name)):
                reached.add(name)
                queue.append(os.path.join(lib, name))
    for name in sorted(os.listdir(lib)):
        path = os.path.join(lib, name)
        if name not in reached and os.path.isfile(path) and '.so' in name:
            os.unlink(path)
    # linuxdeploy's copies of the packages' copyright files, the removed ones'
    # among them: notices() gathers those of what is left into LICENSES.
    doc = os.path.join(appdir, 'usr', 'share', 'doc')
    for name in sorted(os.listdir(doc)) if os.path.isdir(doc) else []:
        if name != 'linscp':
            shutil.rmtree(os.path.join(doc, name))


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
    # linscp's own files; everything else in the AppDir is bundled.
    installed = set(tree(appdir))

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
            os.makedirs(target, exist_ok=True)
            for plugin in sorted(PLUGINS[name] & set(os.listdir(source))):
                shutil.copy2(os.path.join(source, plugin), target)
            wayland += ['--deploy-deps-only', target]
    subprocess.run([tools['linuxdeploy-x86_64.AppImage'], '--appdir', appdir] + wayland + [
                    '--executable', os.path.join(real, 'linscp'),
                    '--executable', os.path.join(real, 'linscp-cli'),
                    '--desktop-file', os.path.join(usr, 'share', 'applications', APP_ID + '.desktop'),
                    '--icon-file', os.path.join(usr, 'share', 'icons', 'hicolor', '256x256', 'apps', APP_ID + '.png'),
                    '--plugin', 'qt'], check=True, env=env)
    prune(appdir)
    # Qt's paths for the real executable, which lives beside core's
    # Translations rather than in usr/bin: the same qt.conf.
    conf = os.path.join(usr, 'bin', 'qt.conf')
    if os.path.isfile(conf):
        with open(conf) as f:
            text = f.read()
        with open(os.path.join(real, 'qt.conf'), 'w') as f:
            f.write(text.replace('Prefix = ../', 'Prefix = ../../'))
    qt_translations = subprocess.run([env['QMAKE'], '-query', 'QT_INSTALL_TRANSLATIONS'], check=True,
                                     capture_output=True, text=True).stdout.strip()
    used = notices(appdir, installed, version, {'usr/plugins': qt_plugins, 'usr/translations': qt_translations})
    print(f'{appdir}: licences and the source offer of {used} Debian packages')
    os.makedirs(out, exist_ok=True)
    target = os.path.join(out, f'linscp-{version}-x86_64.AppImage')
    # The pinned runtime, not the one appimagetool would download (an unpinned
    # "continuous" build); its notices are in the licences folder above.
    subprocess.run([tools['appimagetool-x86_64.AppImage'], '--no-appstream',
                    '--runtime-file', tools['runtime-x86_64'], appdir, target], check=True,
                   env=dict(env, ARCH='x86_64'))
    print(target)
    return 0


CONTACT = 'ecorreia@apliant.com.br'
SNAPSHOT = 'https://snapshot.debian.org/package/'
LICENSES = 'usr/share/licenses/linscp'
COMMON = '/usr/share/common-licenses'
# What linuxdeploy writes itself, not bundled from a package: its AppRun and
# hooks, the desktop entry and icon copied to the top, Qt's qt.conf, and the
# packages' copyright files (gathered again, with their licences, below).
GENERATED = re.compile(r'^([^/]+|apprun-hooks/[^/]+\.sh|usr/(bin|lib/linscp)/qt\.conf|usr/share/doc/[^/]+/copyright)$')
HOST_LIBS = '/usr/lib/x86_64-linux-gnu'
REFERENCE = re.compile(r'/usr/share/common-licenses/(\{[^}]*\}|[A-Za-z0-9][A-Za-z0-9.+_-]*)')

OFFER = """\
Source code of the third-party components of linscp for Linux (AppImage)

The linscp AppImage includes, in binary form, third-party components built by
the Debian project for {system}, all listed in
THIRD-PARTY-NOTICES.txt. Those below are licensed under the GNU Lesser General
Public License, the GNU General Public License or another licence that
entitles you to their complete corresponding source code.

That source code is published with this release of linscp, in the same place
as the AppImage:

  https://github.com/99ecarvalho/linscp/releases/tag/v{version}
  linscp-{version}-third-party-sources.tar

Each component is also listed below with the address where its exact version
is kept upstream. For a Debian source package, that is snapshot.debian.org,
the Debian project's permanent archive of every package version: the upstream
source, Debian's changes to it and the build instructions that produced the
binaries included here. If you cannot get it from either place, write to
{contact}, naming linscp {version} and the components you want.

linscp's own source code, for this exact version, is at
https://github.com/99ecarvalho/linscp (tag v{version}), and in
linscp-{version}.tar.xz with the release.

{components}
"""

NOTICES = """\
Third-party components of linscp for Linux (AppImage)

These components are included in binary form, as built by the Debian
project (https://www.debian.org) for
{system},
changed only by linuxdeploy setting their library search path (RUNPATH) to
the AppImage's own. Each one's copyright file, with the licence texts it
refers to, is in third-party/<component>/. Where a licence requires it, the
source code is offered in SOURCE-OFFER.txt.

{components}
"""


RUNTIME = os.path.join(ROOT, 'port', 'packaging', 'appimage', 'runtime')
# The licence of what is shipped from each Debian source package.
SHIPPED_LICENCES = os.path.join(ROOT, 'port', 'packaging', 'appimage', 'licences.json')


def runtime_notices(third_party):
    """The AppImage runtime's components (port/packaging/appimage/runtime/):
    their licence texts into THIRD_PARTY/appimage-runtime/; returns the
    sections added to the notices and to the source offer, and the
    third-party.json entries of the copyleft ones."""
    description = json.load(open(os.path.join(RUNTIME, 'runtime.json'), encoding='utf-8'))
    target = os.path.join(third_party, 'appimage-runtime')
    os.makedirs(target)
    listing, offer, entries = [], [], []
    for c in description['components']:
        for text in c['texts']:
            source = os.path.join(RUNTIME, text)
            if not os.path.isfile(source):
                sys.exit(f'{c["name"]}: licence text {source} is missing')
            shutil.copyfile(source, os.path.join(target, text))
        urls = ''.join(f'  Source: {s["url"]}\n' for s in c.get('sources', []))
        listing.append(f'{c["name"]} {c["version"]}\n'
                       f'  Licence: {c["licence"]}\n'
                       f'  Project: {c["project"]}\n'
                       f'  Licence texts: third-party/appimage-runtime/ ({", ".join(c["texts"])})\n' + urls)
        if licences.copyleft([c['licence']]):
            if not c.get('sources'):
                sys.exit(f'{c["name"]}: copyleft ({c["licence"]}), but runtime.json names no source for it')
            offer.append(f'{c["name"]} {c["version"]} ({c["licence"]})\n' + urls.replace('  Source: ', '  '))
            entries.append({'kind': 'archive', 'source': c['name'], 'version': c['version'].split(',')[0],
                            'licence': c['licence'], 'copyleft': True, 'files': c['sources']})
    head = ('\nThe AppImage runtime\n\n' + description['about'] + '\n' + description['release'] + '\n\n')
    return (head + '\n'.join(listing),
            '\nThe AppImage runtime (' + description['release'] + '):\n\n' + '\n'.join(offer),
            entries)


def tree(root):
    """The regular files under ROOT, as sorted relative paths."""
    for directory, dirs, files in os.walk(root):
        dirs.sort()
        for name in sorted(files):
            path = os.path.join(directory, name)
            if not os.path.islink(path):
                yield os.path.relpath(path, root).replace(os.sep, '/')


def host_paths(relative, roots, libraries):
    """Where a bundled file can come from on the build machine: linuxdeploy
    puts libraries in usr/lib and Qt's plugins and translations under
    ROOTS's keys."""
    for prefix, origin in sorted(roots.items()):
        if relative.startswith(prefix + '/'):
            return [os.path.join(origin, relative[len(prefix) + 1:])]
    parts = relative.split('/')
    if len(parts) == 3 and parts[:2] == ['usr', 'lib']:
        if not libraries:
            # By name below the multiarch directory (a few live in subfolders).
            for directory, dirs, files in os.walk(HOST_LIBS):
                dirs.sort()
                for name in sorted(files):
                    libraries.setdefault(name, []).append(os.path.join(directory, name))
        return [os.path.join(HOST_LIBS, parts[2])] + sorted(libraries.get(parts[2], []), key=len)
    return []


def owners(paths):
    """{path: package} for the paths dpkg knows; tries each path's /lib and
    resolved spellings too, as packages may list either."""
    variants = {}
    for path in paths:
        real = os.path.realpath(path)
        for v in (path, real, re.sub(r'^/usr/lib/', '/lib/', path), re.sub(r'^/usr/lib/', '/lib/', real)):
            variants.setdefault(v, path)
    result = subprocess.run(['dpkg', '-S'] + sorted(variants), capture_output=True, text=True)
    found = {}
    for line in result.stdout.splitlines():
        if line.startswith('diversion '):
            continue
        package, _, path = line.partition(': ')
        if path in variants and variants[path] not in found:
            found[variants[path]] = package.split(',')[0].strip()
    return found


def common_licenses(name, text):
    """The /usr/share/common-licenses texts TEXT refers to."""
    names = set()
    for m in REFERENCE.finditer(text):
        group = m.group(1)
        for ref in (group[1:-1].split(',') if group.startswith('{') else [group]):
            ref = ref.strip().rstrip('.')
            for candidate in (ref, ref.rstrip('+')):
                if candidate and os.path.isfile(os.path.join(COMMON, candidate)):
                    names.add(candidate)
                    break
            else:
                sys.exit(f'{name}: its copyright file refers to {COMMON}/{ref}, which does not exist')
    return sorted(names)


def notices(appdir, installed, version, roots):
    """Licence texts, notices and the source offer of every Debian package a
    file bundled in APPDIR (one not in INSTALLED) came from, and linscp's own
    licences; returns how many packages."""
    bundled = [f for f in tree(appdir) if f not in installed and not GENERATED.match(f)]
    libraries = {}
    candidates = {f: host_paths(f, roots, libraries) for f in bundled}
    for f, paths in candidates.items():
        if not any(os.path.exists(p) for p in paths):
            sys.exit(f'{f}: bundled, but not found on this machine (looked for {", ".join(paths) or "nothing"})')
    found = owners(p for paths in candidates.values() for p in paths if os.path.exists(p))
    used = {}
    for f, paths in sorted(candidates.items()):
        package = next((found[p] for p in paths if p in found), None)
        if package is None:
            sys.exit(f'{f}: bundled from {paths[0]}, which no Debian package owns')
        used.setdefault(package, []).append(f)
    fields = ['binary:Package', 'Package', 'Version', 'source:Package', 'source:Version', 'Homepage']
    query = subprocess.run(['dpkg-query', '-W', '-f', '\t'.join('${%s}' % f for f in fields) + '\n'] + sorted(used),
                           check=True, capture_output=True, text=True).stdout
    info = {}
    for line in query.splitlines():
        values = dict(zip(fields, line.split('\t')))
        info[values['binary:Package']] = values
    system = 'Debian'
    if os.path.exists('/etc/os-release'):
        for line in open('/etc/os-release', encoding='utf-8'):
            if line.startswith('PRETTY_NAME='):
                system = line.split('=', 1)[1].strip().strip('"')

    licenses = os.path.join(appdir, LICENSES)
    third_party = os.path.join(licenses, 'third-party')
    shutil.rmtree(third_party, ignore_errors=True)
    os.makedirs(third_party)
    # linscp's own (the install's usr/share/doc/linscp).
    own = os.path.join(appdir, 'usr', 'share', 'doc', 'linscp')
    for name in sorted(os.listdir(own)) if os.path.isdir(own) else []:
        if not os.path.exists(os.path.join(licenses, name)):
            shutil.copy2(os.path.join(own, name), licenses)
    shipped = json.load(open(SHIPPED_LICENCES, encoding='utf-8'))['sources']
    unknown = sorted({info[p]['source:Package'] for p in used} - set(shipped))
    if unknown:
        sys.exit(f'bundled from source packages {SHIPPED_LICENCES} does not list (review the licence of what '
                 f'is shipped from each, from its copyright file, and add it): {", ".join(unknown)}')
    listing, sources = [], {}
    for qualified in sorted(used, key=lambda p: info[p]['Package']):
        package = info[qualified]
        name = package['Package']
        copyright_file = os.path.join('/usr/share/doc', name, 'copyright')
        if not os.path.isfile(copyright_file):
            sys.exit(f'{name}: the package has no {copyright_file}')
        with open(copyright_file, encoding='utf-8', errors='replace') as f:
            text = f.read()
        target = os.path.join(third_party, name)
        os.makedirs(target)
        shutil.copyfile(copyright_file, os.path.join(target, 'copyright'))
        referenced = common_licenses(name, text)
        for licence in referenced:
            shutil.copyfile(os.path.join(COMMON, licence), os.path.join(target, licence))
        # A machine-readable copyright file names its licences.
        names = sorted({m.strip() for m in re.findall(r'^License:[ \t]*(\S.*)$', text, re.MULTILINE)}) \
            if text.startswith('Format:') else []
        src, src_version = package['source:Package'], package['source:Version']
        licence = shipped[src]
        url = f'{SNAPSHOT}{urllib.parse.quote(src, safe="")}/{urllib.parse.quote(src_version, safe="")}/'
        listing.append(f'{name} {package["Version"]}\n'
                       f'  Licence: {licence}\n'
                       + (f'  Project: {package["Homepage"]}\n' if package['Homepage'] else '')
                       + f'  Licence texts: third-party/{name}/ (copyright'
                       + ''.join(f', {r}' for r in referenced) + ')\n'
                       f'  Files: {", ".join(used[qualified])}\n'
                       f'  Debian source package: {src} {src_version}\n'
                       f'  Source: {url}\n')
        entry = sources.setdefault((src, src_version), {'kind': 'debian', 'source': src, 'version': src_version,
                                                         'url': url, 'licence': licence,
                                                         'copyleft': licences.copyleft([licence]), 'packages': []})
        entry['packages'].append({'package': name, 'version': package['Version'], 'licences': names,
                                  'files': used[qualified]})
    offer = [f'{src} {src_version} ({entry["licence"]}; for {", ".join(p["package"] for p in entry["packages"])})\n'
             f'  {entry["url"]}\n'
             for (src, src_version), entry in sorted(sources.items()) if entry['copyleft']]
    runtime_listing, runtime_offer, runtime_entries = runtime_notices(third_party)
    with open(os.path.join(licenses, 'THIRD-PARTY-NOTICES.txt'), 'w', encoding='utf-8') as f:
        f.write(NOTICES.format(system=system, components='\n'.join(listing)) + runtime_listing)
    with open(os.path.join(licenses, 'SOURCE-OFFER.txt'), 'w', encoding='utf-8') as f:
        f.write(OFFER.format(system=system, contact=CONTACT, version=version, components='\n'.join(offer))
                + runtime_offer)
    # For port/tools/fetch_sources.py.
    with open(os.path.join(licenses, 'third-party.json'), 'w', encoding='utf-8') as f:
        json.dump([entry for _, entry in sorted(sources.items())] + runtime_entries, f, indent=1, sort_keys=True)
        f.write('\n')
    return len(used)


if __name__ == '__main__':
    sys.exit(main())
