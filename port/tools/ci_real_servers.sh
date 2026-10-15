#!/bin/bash
# CI's real-servers job, inside the Debian build container that the runner
# starts on its own network with its docker socket (.github/workflows/ci.yml):
# builds the programs the real-server tests need and runs those tests
# (ctest -L real-servers) as an unprivileged user. Their temporary
# directories are under $TMPDIR, mounted at the same path in the runner, so
# the servers' containers, started through the runner's docker, see them.
set -e
# (pipefail: ctest's own failure through tee)
set -o pipefail
apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
  ca-certificates git clang lld llvm cmake ninja-build make perl python3 \
  cpio curl unzip xz-utils libssl-dev zlib1g-dev shared-mime-info openssh-client openssl \
  binutils-mingw-w64-x86-64 python3-boto3 \
  qt6-base-dev qt6-svg-dev qt6-l10n-tools qt6-translations-l10n docker-cli
# The workspace is the runner's: given back to its owner when done.
owner=$(stat -c %u:%g "$PWD")
trap 'chown -R "$owner" "$PWD" "$TMPDIR"' EXIT
useradd --create-home ci
# The runner's docker socket, for ci too.
groupadd -o -g "$(stat -c %g /var/run/docker.sock)" dockerhost
usermod -aG dockerhost ci
chown -R ci:ci "$PWD" "$TMPDIR"
runuser -u ci -- python3 port/tools/fetch_llvm_mingw.py --dest build/tools
runuser -u ci -- cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
runuser -u ci -- cmake --build build --target linscp-cli test_qt_gui test_qt_session linscp
runuser -u ci -- env TMPDIR="$TMPDIR" LINSCP_CONTAINER_ENGINE=docker \
  ctest --test-dir build -L real-servers --output-on-failure --timeout 900 --no-tests=error | tee "$TMPDIR/ctest.log"
# This job is for these tests: one skipped (no engine, no image) fails it.
if grep -q "Skipped" "$TMPDIR/ctest.log"; then
  echo "real-server tests skipped" >&2
  exit 1
fi
