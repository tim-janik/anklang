#!/usr/bin/env bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail && SCRIPTNAME=${0##*/} && die() { [ -z "$*" ] || echo "$SCRIPTNAME: $*" >&2; exit 127 ; }

# Usage: gh-release.sh [-x] <TITLE>
TITLE=
while test $# -ne 0 ; do
  case "$1" in \
    -x)	set -x ;;
    -*)	die "unknown options" ;;
    *)	TITLE="$1" ;;
  esac
  shift
done
test ! -z "$TITLE" || die "missing project title"

# Artifacts
ARTIFACTS=./artifacts/
test -d $ARTIFACTS || die "missing release artifacts"

# Release version from current tag
VV=$(git describe --tags --match='v[0-9]*.[0-9]*.[0-9]*' --exact-match 2>/dev/null ||
       git describe --match='v[0-9]*.[0-9]*.[0-9]*' 2>/dev/null) || die "missing current version tag"
V="${VV#v}"

# NEWS, extract first entry
F_NOTES=
grep -s -m1 '^#' NEWS.md | grep -qE "\bv?${V//./\\.}($|[^a-z0-9-])" && {
  sed -rn '/^##? / { p; :BEGIN ; n ; /^##? /q ; p ; bBEGIN ; }' NEWS.md >$ARTIFACTS/.notes
  F_NOTES="-F $ARTIFACTS/.notes"
}

# PRERELEASE for lightweight tag
KIND=--prerelease

# DRAFT release for annotated tag
git tag -l --format='%(objecttype)' "$VV" | grep -q '^tag' && {
  KIND=		# immediate release
  KIND=--draft
}

# INFO
TITLE="$TITLE $V"
echo "TITLE: $TITLE"
echo "CURRENT_TAG: $VV"
echo "VERSION: $V"
echo "ARTIFACTS:" &&
  ls -l ${F_NOTES#-F} $ARTIFACTS/*

# Create Github release for remote tag
set -x
gh release create \
   --title "$TITLE" \
   $F_NOTES \
   --generate-notes \
   $KIND \
   --verify-tag \
   "$VV" \
   $ARTIFACTS/* </dev/null

# Related links:
# https://cli.github.com/manual/gh_help_environment
