#!/bin/sh
# Build linscp's Ubuntu source packages, one per series, from the release
# tarball, for a Launchpad PPA (RELEASE-PLAN, Ubuntu - Launchpad PPA; R2-13).
#
#   port/tools/ppa_source_packages.sh TARBALL OUTDIR SERIES:VERSION...
#   port/tools/ppa_source_packages.sh dist/linscp-0.7.2.tar.xz out resolute:26.04 stonking:26.10
#
# Every series gets the same orig tarball: mk-origtargz makes it once from
# TARBALL, applying debian/copyright's Files-Excluded (a +dfsg repack, only
# when something is actually excluded; otherwise it is TARBALL itself), and
# each source package is X.Y.Z-1~ppa1~ubuntuNN.NN.1 for its series. Writes
# OUTDIR/linscp_<orig version>.orig.tar.xz and, per series, the .dsc,
# .debian.tar.xz and _source.changes, unsigned. Signing (debsign) and the
# upload (dput) are left to the caller.
#
# Needs devscripts (mk-origtargz, dch), dpkg-dev and debhelper.
set -eu

if [ $# -lt 3 ]; then
    sed -n '2,17p' "$0" | sed 's/^# \{0,1\}//'
    exit 2
fi
tarball=$(realpath "$1")
out=$2
shift 2
root=$(realpath "$(dirname "$0")/../..")
debian="$root/port/packaging/debian"

case "$(basename "$tarball")" in
    linscp-*.tar.xz) version=$(basename "$tarball" .tar.xz); version=${version#linscp-} ;;
    *) echo "ppa_source_packages: $tarball is not linscp-X.Y.Z.tar.xz" >&2; exit 2 ;;
esac

mkdir -p "$out"
out=$(realpath "$out")
rm -f "$out"/linscp_*.orig.tar.xz
mk-origtargz --package linscp --version "$version" --repack-suffix +dfsg \
    --compression xz --copyright-file "$debian/copyright" --directory "$out" "$tarball"
orig=$(ls "$out"/linscp_*.orig.tar.xz)
upstream=$(basename "$orig" .orig.tar.xz)
upstream=${upstream#linscp_}

# The changelog entry's author: the source package's maintainer.
maintainer=$(sed -n 's/^Maintainer: //p' "$debian/control")
DEBFULLNAME=${maintainer% <*}
DEBEMAIL=${maintainer##*<}
DEBEMAIL=${DEBEMAIL%>}
export DEBFULLNAME DEBEMAIL

for spec in "$@"; do
    series=${spec%%:*}
    release=${spec#*:}
    work="$out/$series"
    rm -rf "$work"
    mkdir -p "$work"
    tar -C "$work" -xf "$orig"
    src=$(ls -d "$work"/linscp-*/)
    cp -a "$debian" "$src/debian"
    ln -s "$orig" "$work/"
    (cd "$src" && dch -b -v "$upstream-1~ppa1~ubuntu$release.1" -D "$series" --force-distribution \
        "Build for Ubuntu $release ($series).")
    (cd "$src" && dpkg-buildpackage -S -sa -d -us -uc)
    mv "$work"/linscp_*~ubuntu"$release".1* "$out"/
    rm -rf "$work"
done
ls -1 "$out"
