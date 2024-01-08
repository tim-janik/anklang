#!/usr/bin/env bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail && SCRIPTNAME=${0##*/} && die() { [ -z "$*" ] || echo "$SCRIPTNAME: $*" >&2; exit 127 ; }

help() {
  echo "Usage: $0 <outputdir>"
  echo "Script to build liblilv from external/ to output directory <outputdir>"
  exit 0
}

test $# -ne 1 && help

SRC="$PWD"
OUT="$SRC/$1"
export PKG_CONFIG_PATH="$OUT/lv2-libs/lib/x86_64-linux-gnu/pkgconfig/:${PKG_CONFIG_PATH:-}"
export PKG_CONFIG_PATH="$OUT/lv2-libs/lib/pkgconfig/:${PKG_CONFIG_PATH:-}"

for LIB in lv2 zix serd sord sratom lilv
do
  rm -rf "$OUT/lv2-build/$LIB"
  mkdir -p "$OUT/lv2-build/$LIB"
  pushd "$OUT/lv2-build/$LIB"
  meson setup $SRC/external/$LIB --prefix=$OUT/lv2-libs -Ddefault_library=static
  ninja
  meson install
  popd
done

for LIB_A in $(find $OUT/lv2-libs/lib -type f -iname 'lib*.a')
do
  cp "$LIB_A" "$OUT/lib"
done
