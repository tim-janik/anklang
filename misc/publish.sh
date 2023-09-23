#!/usr/bin/env bash
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
test $# -eq 0 -a ! -d assets/ && help
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
test -r "`echo assets/anklang-*.tar.zst`" -o -n "$RUNID" || die "need either $PWD/assets/anklang-*.tar.zst or <run-id>"

# Properly fetch all of the repo history
test -r `git rev-parse --git-dir`/shallow &&
  git fetch --unshallow
# Re-fetch all tags, because actions/checkout messes up annotation of the fetched tag: actions/checkout#290
git fetch -f --tags
echo -n 'Repository: ' && misc/version.sh

# Download release assets from Github Action Run
test -z "$RUNID" || {
  echo "Download assets, github.run_id=$RUNID:"
  rm -rf assets/
  gh run download $RUNID -n assets -D assets/
}

# Extract initial NEWS section
rm -rf out/publish/
mkdir -p out/publish/
tar xf assets/anklang-*.tar.zst -C out/publish/ --strip-components=1 --wildcards 'anklang-*/NEWS.md' 'anklang-*/misc/version.sh'
sed -r '0,/^##?\s/n; /^##?\s/Q;' out/publish/NEWS.md  > out/publish/changes.md

# Determine release version
echo "Tarball:" assets/anklang-*.tar.zst
echo -n 'Release Assets: ' && out/publish/misc/version.sh
RELEASE_VERSION=$(out/publish/misc/version.sh | cut -d\  -f1)
echo RELEASE_VERSION=$RELEASE_VERSION
RELEASE_TAG="v$RELEASE_VERSION"
echo RELEASE_TAG=$RELEASE_TAG
ISPRERELEASE=$(test tag != $(git cat-file -t $RELEASE_TAG) && echo true || echo false)
echo ISPRERELEASE=$ISPRERELEASE

# Create release and upload assets
# --discussion-category Releases
echo "Uploading release to Github:" $RELEASE_TAG
gh release create \
   -F out/publish/changes.md \
   $($ISPRERELEASE && echo --prerelease) \
   -t "Anklang $RELEASE_VERSION" \
   $RELEASE_TAG \
   assets/*
# gh release edit --draft=false $RELEASE_TAG

# Clear exit code
exit 0
