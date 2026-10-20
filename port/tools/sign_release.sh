#!/bin/sh
# Sign a drafted release on this machine, for when the release workflow has no
# signing key: downloads its SHA256SUMS and source tarball, checks the tarball
# against the sums, signs both, and uploads the signatures to the draft.
#
#   port/tools/sign_release.sh v0.7.2 [KEY-ID]
#
# KEY-ID picks the signing key (default: gpg's default key). Needs gh, gpg and
# sha256sum; run it before publishing the draft.
set -eu
tag=${1:?usage: sign_release.sh vX.Y.Z [KEY-ID]}
key=${2:-}
version=${tag#v}
tarball="linscp-$version.tar.xz"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

gh release download "$tag" --dir "$work" --pattern SHA256SUMS --pattern "$tarball"
cd "$work"
sha256sum --check --ignore-missing SHA256SUMS
for file in SHA256SUMS "$tarball"; do
  gpg ${key:+--local-user "$key"} --armor --detach-sign "$file"
  gpg --verify "$file.asc" "$file"
done
gh release upload "$tag" SHA256SUMS.asc "$tarball.asc" --clobber
echo "signed $tag: SHA256SUMS.asc, $tarball.asc"
