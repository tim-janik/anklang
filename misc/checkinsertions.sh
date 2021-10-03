#!/bin/bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail #-x

RANGE="${1:-origin/trunk..HEAD}"	# HEAD~9..HEAD
DPATT=TO"DO|FIX"ME
WORDS="${2:-$DPATT}"			# keyword extended regex

git diff --name-only -z "$RANGE" | (
  while IFS= read -r -d $'\0' FILE; do
    git diff "$RANGE" -U0 -- "$FILE" |
      sed -rne $'s\5^\5'"$FILE"$':\5' -e "/:\+.*([#*/]|<!--).*\b($WORDS)\b/p"
  done) |
  grep -E "($WORDS).*" --color
