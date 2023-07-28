#!/usr/bin/env bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail

SCRIPTNAME=${0##*/} ; die() { [ -z "$*" ] || echo "$SCRIPTNAME: $*" >&2; exit 9 ; }
ABSPATHSCRIPT=`readlink -f "$0"`

# == Version baked in ==
# Commit information provided by git-archive in export-subst format string, see gitattributes(5)
DESCRIBE='$Format:%(describe:match=v[0-9]*.[0-9]*.[0-9]*)$'
HASH='$Format:%H$'
VDATE='$Format:%ci$'

# == Version from git ==
if ! [[ "$HASH" =~ ^[0-9a-f]+$ ]] ; then		# checks proper hash
  DESCRIBE=
  if test -e "${ABSPATHSCRIPT%/*}"/../.git ; then	# fetch version from live git
    HASH=$(git log -1 --pretty="tformat:%H")
    DESCRIBE=$(git log -1 --pretty="tformat:%(describe:match=v[0-9]*.[0-9]*.[0-9]*)")
    VDATE=$(git log -1 --pretty="tformat:%ci")
  fi
fi

# == Fallback version ==
if test -z "$DESCRIBE" ; then
  HASH=0000000000000000000000000000000000000000
  DESCRIBE=v0.0.0-snapshot0
  VDATE="2001-01-01 01:01:01 +0000"
fi

# == Produce: VERSION HASH DATE ==
VERSIONNUMBER=$(echo "${DESCRIBE#v}" |
		  sed -e 's/-g[0-9a-f]\+$//i' \
		      -e 's/-\([0-9]\+\)$/.dev\1/')	# strip ^v, -gCOMMIT, enforce .devXX
echo "$VERSIONNUMBER" "$HASH" "$VDATE"
