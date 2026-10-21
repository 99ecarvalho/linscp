# linscp's RPM package, one spec for Fedora (Copr) and openSUSE (OBS):
# the differences are under %%if 0%%{?suse_version}. Built from the release
# tarball, which carries every bundled tree (doc/qt/bundled-libraries.md).

# linscp builds with clang only: GCC has no __declspec(property), which the
# property shim depends on, so upstream does not build with GCC at all.
%global toolchain clang

# LTO stays off until a clang LTO build has passed the test suite.
%global _lto_cflags %{nil}

%if 0%{?suse_version}
# Ninja, and the build directory openSUSE's %%cmake uses.
%define __builder ninja
%define linscp_builddir %{__builddir}
%else
%define linscp_builddir %{__cmake_builddir}
%endif
%{!?_metainfodir:%global _metainfodir %{_datadir}/metainfo}

Name:           linscp
Version:        0.7.2
Release:        1%{?dist}
Summary:        SFTP, SCP, FTP, WebDAV and S3 file-transfer client

# linscp's own code and the transfer core: GPL-3.0-or-later.
# The FTP/FTPS engine (3rd-party/winscp/source/filezilla): GPL-2.0-or-later.
# neon (3rd-party/winscp/libs/neon/src): LGPL-2.0-or-later.
# libs3 (3rd-party/winscp/libs/libs3): LGPL-3.0-or-later OR GPL-2.0-or-later.
# PuTTY (3rd-party/winscp/source/putty): MIT.
# Toolbar icons: Lucide (ISC), Tabler (MIT), Fluent (MIT); the application
# icon is linscp's own around a Lucide glyph (ISC).
# OpenSSL and expat come from the system, so their licences are not listed.
License:        GPL-3.0-or-later AND GPL-2.0-or-later AND LGPL-2.0-or-later AND (LGPL-3.0-or-later OR GPL-2.0-or-later) AND MIT AND ISC
URL:            https://linscp.org
Source0:        https://github.com/99ecarvalho/linscp/releases/download/v%{version}/%{name}-%{version}.tar.xz
# The tarball's signature and the release-signing key
# (A679B8C9915BB5ACDD45F91E207FB492B5B214A9, doc/release-signing-key.asc).
Source1:        https://github.com/99ecarvalho/linscp/releases/download/v%{version}/%{name}-%{version}.tar.xz.asc
Source2:        linscp-release-key.asc

# Bundled because the core uses API only these modified copies have
# (doc/qt/bundled-libraries.md); there is no system library to use instead.
Provides:       bundled(putty) = 0.85
Provides:       bundled(neon) = 0.37.1
Provides:       bundled(libs3) = 4.1
# An FTP engine from the FileZilla 2 series (2002-2004), with no release
# number of its own; it is not a library any distribution ships.
Provides:       bundled(filezilla)

BuildRequires:  clang
%if 0%{?fedora} || 0%{?rhel}
BuildRequires:  gnupg2
%endif
BuildRequires:  cmake >= 3.25
BuildRequires:  python3 >= 3.9
BuildRequires:  cmake(Qt6Widgets) >= 6.8
BuildRequires:  cmake(Qt6Svg) >= 6.8
BuildRequires:  cmake(Qt6Network) >= 6.8
BuildRequires:  cmake(Qt6DBus) >= 6.8
BuildRequires:  pkgconfig(expat) >= 2.4
BuildRequires:  desktop-file-utils
%if 0%{?suse_version}
# Not pkgconfig(openssl): LibreSSL provides it too.
BuildRequires:  libopenssl-devel >= 3.0
BuildRequires:  ninja
BuildRequires:  qt6-tools-linguist
BuildRequires:  appstream-glib
BuildRequires:  hicolor-icon-theme
%else
BuildRequires:  pkgconfig(openssl) >= 3.0
BuildRequires:  ninja-build
BuildRequires:  qt6-linguist
BuildRequires:  libappstream-glib
%endif

# Kerberos (GSSAPI) is loaded at run time, not linked; Wayland comes
# through a Qt plugin.
%if 0%{?suse_version}
# The icon directories' owner is a BuildRequires only (rpmlint's branding check).
Recommends:     krb5
Recommends:     qt6-wayland
%else
Requires:       hicolor-icon-theme
Recommends:     krb5-libs
Recommends:     qt6-qtwayland
%endif

%description
linscp transfers files between your computer and remote servers over SFTP,
SCP, FTP (with TLS), WebDAV and S3. Its two-panel interface shows local and
remote files side by side, with the same sessions, settings and scripting on
Linux and Windows.

It synchronizes and compares directories, keeps a remote directory up to
date, edits remote files, and runs commands on the server. A command-line
program runs the same transfers from scripts.

%prep
# Fedora checks the tarball's signature here; openSUSE's OBS checks it with
# the package's linscp.keyring instead (it has no %%{gpgverify}).
%if 0%{?fedora} || 0%{?rhel}
%{gpgverify} --keyring='%{SOURCE2}' --signature='%{SOURCE1}' --data='%{SOURCE0}'
%endif
%autosetup -p1
# The package builds against the system OpenSSL and expat, so their bundled
# copies are deleted before the build (Fedora's Packaging Guidelines,
# "Bundling and Duplication of System Libraries"). The tarball has nothing
# that may not be shipped as source, so it needs no repack.
rm -r 3rd-party/openssl 3rd-party/winscp/libs/openssl 3rd-party/winscp/libs/expat

%build
# LINSCP_SYSTEM_OPENSSL=ON also leaves TLS to the system crypto policy: no
# cipher list is set, and a TLS version bound only when a session changes it.
# LINSCP_REPRODUCIBLE=OFF: its -ffile-prefix-map would hide the sources
# from debugedit (an empty debugsource package); rpm does that part, and
# clang dates __DATE__ by rpm's SOURCE_DATE_EPOCH itself.
%if 0%{?suse_version}
# openSUSE has no %%toolchain: pick clang here.
export CC=clang CXX=clang++
%if 0%{?suse_version} < 1650
# Leap's debugedit does not read clang's DWARF 5 (DW_FORM_strx1).
export CFLAGS="%{optflags} -gdwarf-4" CXXFLAGS="%{optflags} -gdwarf-4"
%endif
# lupdate6 and lrelease6 are found by name from 0.7.2 on; Qt's own here.
%cmake \
    -DLINSCP_LUPDATE=%{_libdir}/qt6/bin/lupdate \
    -DLINSCP_LRELEASE=%{_libdir}/qt6/bin/lrelease \
%else
%cmake -G Ninja \
%endif
    -DLINSCP_RELEASE=ON \
    -DBUILD_TESTING=OFF \
    -DLINSCP_UPDATE_CHECK=OFF \
    -DLINSCP_SYSTEM_OPENSSL=ON \
    -DLINSCP_SYSTEM_EXPAT=ON \
    -DLINSCP_REPRODUCIBLE=OFF
%if 0%{?suse_version}
# openSUSE's %%cmake leaves the shell in the build directory.
cd ..
%endif
# The programs and their translations are not in ALL: build them by name.
cmake --build %{linscp_builddir} --parallel %{_smp_build_ncpus} --verbose \
    --target linscp linscp-cli core-translations

%install
DESTDIR=%{buildroot} cmake --install %{linscp_builddir}
# The licence texts go under %%{_defaultlicensedir}, the rest stays as documentation.
install -d %{buildroot}%{_defaultlicensedir}/%{name}
for f in LICENSE WinSCP-license.txt PuTTY-LICENCE FileZilla-license.txt neon-COPYING.LIB \
         libs3-LICENSE libs3-COPYING-LGPLv3 libs3-COPYING-GPLv2 \
         icons-lucide-LICENSE icons-tabler-LICENSE icons-fluent-LICENSE; do
    mv %{buildroot}%{_datadir}/doc/%{name}/$f %{buildroot}%{_defaultlicensedir}/%{name}/
done
# CMake installs into share/doc/linscp; openSUSE's %%{_docdir} is share/doc/packages.
if [ %{_datadir}/doc != %{_docdir} ]; then
    install -d %{buildroot}%{_docdir}
    mv %{buildroot}%{_datadir}/doc/%{name} %{buildroot}%{_docdir}/
fi

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/org.linscp.linscp.desktop
appstream-util validate-relax --nonet %{buildroot}%{_metainfodir}/org.linscp.linscp.metainfo.xml
%{buildroot}%{_bindir}/linscp-cli /help >/dev/null

%files
%license %{_defaultlicensedir}/%{name}
%doc %{_docdir}/%{name}
%{_bindir}/linscp
%{_bindir}/linscp-cli
%{_libdir}/linscp/
%{_datadir}/applications/org.linscp.linscp.desktop
%{_metainfodir}/org.linscp.linscp.metainfo.xml
%{_datadir}/icons/hicolor/*/apps/org.linscp.linscp.png
%{_datadir}/icons/hicolor/scalable/apps/org.linscp.linscp.svg
%{_mandir}/man1/linscp.1*
%{_mandir}/man1/linscp-cli.1*

%changelog
* Wed Oct 21 2026 Eduardo Correia <ecorreia@apliant.com.br> - 0.7.2-1
- Update to 0.7.2

* Tue Oct 20 2026 Eduardo Correia <ecorreia@apliant.com.br> - 0.7.1-1
- First RPM package
