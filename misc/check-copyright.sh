#!/usr/bin/env bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail #-x
die() { echo "${BASH_SOURCE[0]##*/}: **ERROR**: ${*:-aborting}" >&2; exit 127 ; }

# Usage: check-copyright.sh [COPYRIGHT-FILE...]
BUILDDIR="${BUILDDIR:-out}"
test -x misc/check-copyright.sh ||
  die "script must be run from project root"

# File list from VCS
mkdir -p $BUILDDIR/
if   jj workspace root >/dev/null 2>&1 ; then
  jj --no-pager file list
elif git rev-parse --is-inside-work-tree >/dev/null 2>&1 ; then
  git ls-tree -r --name-only HEAD
else
  die "failed to list VCS files"
fi > $BUILDDIR/check-copyright.sh.lst
trap "rm -f $BUILDDIR/check-copyright.sh.lst" 0 HUP INT QUIT TRAP USR1 PIPE TERM ERR EXIT

misc/checkcrlist.py -e $BUILDDIR/check-copyright.sh.lst "$@"
