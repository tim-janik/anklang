#!/usr/bin/env bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail

SCRIPTNAME=${0##*/} ; die() { [ -z "$*" ] || echo "$SCRIPTNAME: $*" >&2; exit 9 ; }
L=17

# Notes on scenario variants.
# 1) git-archive snapshots, version info is embedded via $Format$ strings
# 2) Full git repo, use git describe
# 3) Shallow git repo, use NEWS.md version info
#
# Version output may look like:
#    1.2.3-rc4       v1.2.3-rc4-git4a3b2c1d          2001-01-01 00:00:00 +0000		# release
#    1.2.3-rc4-dirty v1.2.3-rc4-789-ga1b2c3d4-dirty  2001-01-01 00:00:00 +0000		# unreleased (with .git/)
#    1.2.3-rc4-dirty v1.2.3-rc4-ga1b2c3d4-dirty      2001-01-01 00:00:00 +0000		# unreleased (via git-archive)
# The first term is the version number and always starts with [0-9][0-9.]+,
# it may be used to construct package names.
# The second term is the build ID and may actually contain any kind of string.
# The third term is the last commit date in ISO 8601-like format.

# exit_with_version <version> <buildid> <releasedate>
exit_with_version() {
  V="${1#v}"	# strip 'v' prefix if any
  shift
  echo "$V" "$@"
  exit 0
}

# Determine version from NEWS.md file
fetch_news_version() { # fetch_news_version {1|2}
  if	test "${1:-1}" == 1; then
    IGNORE1='!'		# NOT ignore first
  elif	test "$1" == 2; then
    IGNORE1=		# ignore first
  fi
  SEDSCRIPT="
    0,/^##\s.*\s[0-9]+\.[0-9]+[.a-z0-9A-Z-]*\b(:|$)/ $IGNORE1 D;	# Match start until first version tag and Delete
    /^##\s.*\s[0-9]+\.[0-9]+[.a-z0-9A-Z-]*\b(:|$)/ {			# Match version tag
      s/.*\s([0-9]+\.[0-9]+[.a-z0-9A-Z-]*)\b(:.*|$)/\1/;		# Extract version tag
      p; q; }								# Print, Quit
  "
  if test -r NEWS.md ; then
    sed -nre "$SEDSCRIPT" NEWS.md
  fi
}

# Usage: version.sh [--news-tag|--last-tag]	# print project versions
while test $# -ne 0 ; do
  case "$1" in
    --news-tag1)
      NEWS_VERSION="$(fetch_news_version 1)"
      test -n "$NEWS_VERSION" || die "ERROR: failed to extract release tag from NEWS.md"
      echo "v${NEWS_VERSION#v}" && exit ;;
    --news-tag2)
      NEWS_VERSION="$(fetch_news_version 2)"
      test -n "$NEWS_VERSION" || die "ERROR: failed to extract release tag from NEWS.md"
      echo "v${NEWS_VERSION#v}" && exit ;;
    *)
      true ;;
  esac
  shift
done

# Yield abbreviated commit hash for version string
shorthash() {
  git log -1 --format='%h' --abbrev=$L
}

# Tarball or git-archive: Find 'tag:...' in export-subst format string, see gitattributes(5)
GITARCHIVE_HASH='$Format:%H$' && GITARCHIVE_DATE='$Format:%ci$' && GITARCHIVE_REFS='$Format:%D,$'
if test "$GITARCHIVE_HASH" = "${GITARCHIVE_HASH/:/}" ; then
  GITARCHIVE_TAG=$(echo " $GITARCHIVE_REFS" | tr , '\n' |
		     sed -nr '/\btag: v?[0-9]+\.[0-9]+/ { s/^[^0-9]*\b(v?[0-9]+\.[0-9]+[.a-z0-9A-Z-]*).*/\1/; p; q }')
  NEWS_VERSION="$(fetch_news_version 1)"
  if test -n "$GITARCHIVE_TAG" -a "${GITARCHIVE_TAG#v}" = "$NEWS_VERSION" ; then
    # release version (from tarball)
    exit_with_version "$NEWS_VERSION" "v$NEWS_VERSION-git${GITARCHIVE_HASH:0:$L}" "$GITARCHIVE_DATE"
  else
    # not a release (git-archive)
    exit_with_version "$NEWS_VERSION-dirty" "v$NEWS_VERSION-g${GITARCHIVE_HASH:0:$L}-dirty" "$GITARCHIVE_DATE"
  fi
fi

# Git repository with history
if COMMIT_DATE=$(git log -1 --format='%ci' 2>/dev/null) &&
    LAST_TAG=$(git describe --match v'[0-9]*.[0-9]*' --abbrev=0 --first-parent 2>/dev/null) &&
    BUILD_ID=$(git describe --match "$LAST_TAG" --abbrev=$L --long --dirty)
then
  [[ "$BUILD_ID" =~ ^v?[0-9]+\.[0-9].*-0-g[a-z0-9]+$ ]] &&
    NEWS_VERSION="$(fetch_news_version 1)" &&
    test "${LAST_TAG#v}" = "$NEWS_VERSION" && {
      # release version
      exit_with_version "$NEWS_VERSION" "v$NEWS_VERSION-git`shorthash`" "$COMMIT_DATE"
    }
  # not a release (frequent case during development)
  exit_with_version "$LAST_TAG-dirty" "${BUILD_ID%-dirty}-dirty" "$COMMIT_DATE"
fi

# Shallow git repo, resorts to NEWS.md
test -n "$COMMIT_DATE" && {
  NEWS_VERSION="$(fetch_news_version 1)"
  BUILD_ID="v$NEWS_VERSION-g`shorthash`-dirty"
  # not a release (shallow git repo)
  exit_with_version "$NEWS_VERSION-dirty" "$BUILD_ID" "$COMMIT_DATE"
}

die "ERROR: failed to find git commit"
