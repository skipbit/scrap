#!/bin/bash
#
# Packs a built scrap binary into the archive a release attaches, with its
# checksum beside it.
#
#   scripts/package.sh <binary> <os>-<arch> <output-dir>
#
# The archive is scrap-<version>-<os>-<arch>.tar.gz and holds one directory of
# the same name with scrap, LICENSE and README.md. The version is the git
# description without its leading v: 0.3.0 on a release tag, 0.3.0-2-g<hash>
# after it. The checksum file is in the format `sha256sum -c` reads.
#
# Fails when the binary does not report the version the tree describes, and
# checks the archive by unpacking it and asking the copy inside.

set -euo pipefail

if [ "$#" -ne 3 ]; then
  echo "usage: $0 <binary> <os>-<arch> <output-dir>" >&2
  exit 2
fi

binary="$1"
platform="$2"
out="$3"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

cmake -D "SOURCE_DIR=$root" -D "NAME=scrap" -D "OUTPUT=$work/version" -P "$root/cmake/PrintVersion.cmake"
describe="$(sed -n 1p "$work/version")"
expected="$(sed -n 2p "$work/version")"

check_banner() {
  local banner
  banner="$("$1" --version)"
  if [ "$banner" != "$expected" ]; then
    echo "error: $1 reports '$banner', expected '$expected'" >&2
    exit 1
  fi
}

check_banner "$binary"

name="scrap-${describe#v}-$platform"
mkdir -p "$work/$name" "$out"
cp "$binary" "$work/$name/scrap"
cp "$root/LICENSE" "$root/README.md" "$work/$name/"
tar -C "$work" -czf "$out/$name.tar.gz" "$name"
(cd "$out" && cmake -E sha256sum "$name.tar.gz" > "$name.tar.gz.sha256")

mkdir "$work/unpacked"
tar -C "$work/unpacked" -xzf "$out/$name.tar.gz"
check_banner "$work/unpacked/$name/scrap"

echo "$out/$name.tar.gz"
