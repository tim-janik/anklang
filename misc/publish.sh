#!/bin/bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail && SCRIPTNAME=${0##*/} && die() { [ -z "$*" ] || echo "$SCRIPTNAME: $*" >&2; exit 127 ; }

# Chdir to project root
SCRIPTDIR="${0%/*}"
cd $SCRIPTDIR/..
test -r ase/api.hh || die "cd: failed to change to project root"

help() {
  echo "Usage: $0 [-h] [-x] <run-id>"
  exit 0
}

# Parse OPTIONS
test $# -eq 0 && help
RUNID=
while test $# -ne 0 ; do
  case "$1" in \
    -x)			set -x ;;
    -h|--help)		help ;;
    -*)			help ;;
    *)			RUNID="$1" ;;
  esac
  shift
done
test -n "$RUNID" || die "missing <run-id>"

# Properly fetch all of the repo history
test -r `git rev-parse --git-dir`/shallow &&
  git fetch --unshallow
# Re-fetch all tags, because actions/checkout messes up annotation of the fetched tag: actions/checkout#290
git fetch -f --tags
misc/version.sh
RELEASE_VERSION=$(misc/version.sh | cut -d\  -f1)
echo RELEASE_VERSION=$RELEASE_VERSION
RELEASE_TAG=$(git describe --tags --match='v[0-9]*.[0-9]*.[0-9]*' --exact-match)
echo RELEASE_TAG=$RELEASE_TAG
ISPRERELEASE=$(test tag != $(git cat-file -t $RELEASE_TAG) && echo true || echo false)
echo ISPRERELEASE=$ISPRERELEASE

# Download release assets from Github Action Run
rm -rf out/publish/
mkdir -p out/publish/
gh run download $RUNID -n assets -D out/publish/

# Extract initial NEWS section
tar xvf out/publish/anklang-*.tar.zst -C out/ --strip-components=1 --wildcards 'anklang-*/NEWS.md'
sed -r '0,/^##?\s/n; /^##?\s/Q;' out/NEWS.md  > out/changes.md

# Create release and upload assets
# --discussion-category Releases
gh release create --draft \
   -F out/changes.md \
   $($ISPRERELEASE && echo --prerelease) \
   -t "Anklang $RELEASE_VERSION" \
   $RELEASE_TAG \
   out/publish/*
# gh release edit --draft=false $RELEASE_TAG

# Clear exit code
exit 0
