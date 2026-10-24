#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Builds the third-party libraries the payload links against into build/prefix:
# libmicrohttpd and SQLite. The payload has no internet client, so there is no TLS library
# and no CA bundle.
#
# The versions, checksums and build steps follow the recipes in
# https://github.com/ps5-payload-dev/pacbrew-repo (libmicrohttpd, sqlite), which are the
# libraries tested on PS5. Each recipe builds with the SDK's own toolchain file,
# prospero.sh. Every tarball is checked against a pinned SHA-256 before it is unpacked.
set -euo pipefail

SDK="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build"

if [ ! -x "$SDK/bin/prospero-clang" ]; then
  echo "error: prospero-clang not found under $SDK/bin (set PS5_PAYLOAD_SDK)" >&2
  exit 1
fi

# The SDK's toolchain: CC, AR, RANLIB, CMAKE, MAKE and the rest, as the recipes use them.
export PS5_PAYLOAD_SDK="$SDK"
source "$SDK/toolchain/prospero.sh"
# prospero.sh points DESTDIR at the SDK's target. Unset it, so make install never writes
# into the SDK.
unset DESTDIR
# A project-local prefix, so the SDK itself is never written to. The payload Makefile adds
# build/prefix/include and build/prefix/lib to its search paths.
PREFIX="$BUILD/prefix"

# Skip a rebuild when the same pinned dependency versions are already installed. CI caches
# this directory, so this is what makes a warm run fast.
DEPS_STAMP="$PREFIX/.built"
DEPS_VERSION="libmicrohttpd-1.0.1 sqlite-3.46.1"
if [ -f "$DEPS_STAMP" ] && [ "$(cat "$DEPS_STAMP")" = "$DEPS_VERSION" ]; then
  echo "Dependencies are already built in $PREFIX"
  exit 0
fi

LIBMICROHTTPD_URL="https://ftp.gnu.org/gnu/libmicrohttpd/libmicrohttpd-1.0.1.tar.gz"
LIBMICROHTTPD_SHA256="a89e09fc9b4de34dde19f4fcb4faaa1ce10299b9908db1132bbfa1de47882b94"
SQLITE_URL="https://www.sqlite.org/2024/sqlite-src-3460100.zip"
SQLITE_SHA256="def3fc292eb9ecc444f6c1950e5c79d8462ed5e7b3d605fd6152d145e1d5abb4"

if command -v nproc >/dev/null 2>&1; then
  JOBS="$(nproc)"
else
  JOBS="$(sysctl -n hw.ncpu)"
fi

sha256_of() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | cut -d' ' -f1
  else
    shasum -a 256 "$1" | cut -d' ' -f1
  fi
}

fetch() {
  local url="$1" expected="$2" out="$3"
  if [ -f "$out" ] && [ "$(sha256_of "$out")" = "$expected" ]; then
    return 0
  fi
  curl -fsSL -o "$out" "$url"
  local actual
  actual="$(sha256_of "$out")"
  if [ "$actual" != "$expected" ]; then
    echo "error: checksum mismatch for $url" >&2
    echo "  expected $expected" >&2
    echo "  got      $actual" >&2
    rm -f "$out"
    exit 1
  fi
}

mkdir -p "$BUILD/deps" "$PREFIX/include" "$PREFIX/lib"
cd "$BUILD/deps"

echo "=== libmicrohttpd 1.0.1 ==="
fetch "$LIBMICROHTTPD_URL" "$LIBMICROHTTPD_SHA256" libmicrohttpd.tar.gz
rm -rf libmicrohttpd-1.0.1 && tar xzf libmicrohttpd.tar.gz
(
  cd libmicrohttpd-1.0.1
  ./configure --prefix="$PREFIX" --host=x86_64-pc-freebsd \
    --enable-static --disable-shared --disable-doc \
    --disable-curl --disable-examples
  make -j"$JOBS"
  make install
)

echo "=== SQLite 3.46.1 ==="
fetch "$SQLITE_URL" "$SQLITE_SHA256" sqlite.zip
rm -rf sqlite-src-3460100 && unzip -q sqlite.zip
(
  cd sqlite-src-3460100
  ./configure --prefix="$PREFIX" --host=x86_64-pc-freebsd \
    --enable-static --disable-shared --disable-tcl
  make sqlite3.h libsqlite3.la -j"$JOBS"
  make lib_install
  install -m 0644 sqlite3.h "$PREFIX/include/"
)

printf '%s\n' "$DEPS_VERSION" > "$DEPS_STAMP"
echo "All dependencies built into $PREFIX"
