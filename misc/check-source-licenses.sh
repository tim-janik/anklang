#!/usr/bin/env bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -euo pipefail

sources=()
for file; do
  case "$file" in
    *.g.*) ;;
    *.c|*.cc|*.cpp|*.h|*.hh|*.hpp|*.js|*.jsx|*.ts|*.tsx|*.mjs|*.cjs|*.py|*.sh|\
    *.css|*.scss|*.htm|*.html|*.mk|*.yml|*.yaml|*.tex|*/Makefile|*/AppRun|*/blame-lines)
      sources+=("$file") ;;
  esac
done
((${#sources[@]})) || exit 0
missing=$(grep -LE 'MPL-2\.0|https://unlicense.org/UNLICENSE|This code is public domain' "${sources[@]}") || test $? = 1
if test -n "$missing"; then
  printf '%s\n' "$missing" | sed 's/$/: error: missing source license header/' >&2
  exit 1
fi
