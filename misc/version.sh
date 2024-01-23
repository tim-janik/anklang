#!/usr/bin/env bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail

ABSPATHSCRIPT=`readlink -f "$0"`

# Commit information provided by git-archive in export-subst format string, see gitattributes(5)
read VHASH VDESCRIBE VDATE <<<' $Format: %H %(describe:match=v[0-9]*.[0-9]*.[0-9]*) %ci $ '
[[ "$VDESCRIBE" =~ ^% ]] &&
  VDESCRIBE='$Format:%(describe:match=v[0-9]*.[0-9]*.[0-9]*)$'	# <-- line altered by 'make dist'

# When no version was baked in to the script
[[ "$VHASH" =~ ^[0-9a-f]+$ ]] || {
  # Fallback
  read VHASH VDESCRIBE VDATE <<<' 0000000000000000000000000000000000000000 v0.0.0-unversioned0 2001-01-01 01:01:01 +0000 '
  # Fetch version from live git
  test -e "${ABSPATHSCRIPT%/*}"/../.git && {
    VHASH=$(git log -1 --pretty="tformat:%H" || echo "$VHASH")
    VDATE=$(git log -1 --pretty="tformat:%ci" || echo "$VDATE")
    # Look for light version tag for exact matches (e.g. nighty), or annotated tag in ancestry
    VDESCRIBE=$(git describe --tags --match='v[0-9]*.[0-9]*.[0-9]*' --exact-match 2>/dev/null ||
		  git describe --match='v[0-9]*.[0-9]*.[0-9]*' 2>/dev/null  ||
		  echo "$VDESCRIBE")
  }
}

# Produce: VERSION HASH DATE
VERSIONNUMBER=$(echo "${VDESCRIBE#v}" |
		  sed -e 's/-g[0-9a-f]\+$//i' \
		      -e 's/-\([0-9]\+\)$/.dev\1/')	# strip ^v, -gCOMMIT, enforce .devNN
echo "$VERSIONNUMBER" "$VHASH" "$VDATE"
