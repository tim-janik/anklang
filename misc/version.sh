#!/usr/bin/env bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail

# Usage: version.sh [--last]		# print project version
while test $# -ne 0 ; do
  case "$1" in
    --last)		exec git describe --match 'v[0-9]*.[0-9]*.*[0-9a]' --abbrev=0 --first-parent HEAD ;;
    *)                  : ;;
  esac
  shift
done
UNOFFICIAL=snapshot

# Notes on scenario variants.
# 1) The project tree may be created as a checkout with temporary GIT_DIR. Then, the only way
#    to guess a version number is peeking at the top NEWS.md entry. This is never a release build.
# 2) The project tree may be the result of extracting a 'git archive', in which case $Format...$
#    extraction has occoured. The extracted refs may or may not contain a release tag. A tag
#    will be present if the archive was a release tarball, or downloaded from release-tag
#    version as Github archive. A release tag matching the top NEWS.md entry is a release build.
# 3) We are operating on a normal Git repository, the current checkout may be referenced
#    by a release tag. A release tag matching the top NEWS.md entry is a release build.

# Output for release versions may look like:
#    1.2.3 v1.2.3-tarball 2001-01-01 01:01:01 +0000
# And for non-releases:
#    1.2.3-alpha-3-ga1b2c3d4-dirty v1.2.3-alpha-3-ga1b2c3d4-dirty 2001-01-01 00:00:00 +0000
# The first term is the version number and always starts with [0-9][0-9.]+.
# The second term is the build ID and may actually contain any kind of string.
# The third term is the last commit date in ISO 8601-like format.
exit_with_version() { # exit_with_version <version> <buildid> <releasedate>
  V="${1#v}"	# strip 'v' prefix if any
  shift
  echo "$V" "$@"
  exit 0
}

# Determine version from NEWS.md file
peek_news_version() {
  test -r NEWS.md || {
    echo "$0: ERROR: Failed to find NEWS.md" >&2
    exit 96
  }
  sed -nr '/^##\s.*[0-9]+\.[0-9]+\.[0-9]/ { s/.*\s([0-9]+\.[0-9]+\.[0-9]+[a-z0-9A-Z-]+)\b.*/\1/; p; q }' NEWS.md
}

# Determine version from ./.git if present
if COMMIT_DATE=$(git log -1 --format='%ci' 2>/dev/null) &&
    LAST_TAG=$(git describe --match v'[0-9]*.[0-9]*.*[0-9a]' --abbrev=0 --first-parent 2>/dev/null) &&
    BUILD_ID=$(git describe --match "$LAST_TAG" --abbrev=8 --long --dirty) &&
    test -n "$BUILD_ID"
then
  if [[ "$BUILD_ID" =~ ^v?[0-9]+\..*-0-g[a-z0-9]+$ ]] ; then
    NEWS_VERSION="$(peek_news_version)"
    if test "${LAST_TAG#v}" = "$NEWS_VERSION" ; then
      exit_with_version "$LAST_TAG" "$BUILD_ID" "$COMMIT_DATE"	# release version
    fi
  fi
  # not a release
  exit_with_version "$LAST_TAG" "$BUILD_ID" "$COMMIT_DATE"
fi

# Find 'tag:...' in 'git archive' substitution (see man gitattributes / export-subst)
GITARCHIVE_REFS='$Format:%D,$'
GITARCHIVE_DATE='$Format:%ci$'
[[ "$GITARCHIVE_REFS$GITARCHIVE_DATE" =~ '$'Format:% ]] && LAST_TAG= ||
    LAST_TAG=$(echo " $GITARCHIVE_REFS" | tr , '\n' | \
		 sed -nr '/\btag: v?[0-9]\.[0-9]+\.[0-9]+/ { s/^[^0-9]*\b(v?[0-9]+\.[0-9]+\.[0-9]+[a-z0-9A-Z-]*).*/\1/; p; q }')
test -n "$LAST_TAG" && {
  NEWS_VERSION="$(peek_news_version)"
  if test "${LAST_TAG#v}" = "$NEWS_VERSION" ; then
    exit_with_version "$LAST_TAG" "$LAST_TAG-tarball" "$GITARCHIVE_DATE" # release version
  else
    BUILD_ID="$LAST_TAG-$UNOFFICIAL"
    exit_with_version "$BUILD_ID" "$BUILD_ID-archive" "$GITARCHIVE_DATE"
  fi
}

# Use NEWS_VERSION as last resort
if NEWS_VERSION="$(peek_news_version)" ; then
  test -n "$COMMIT_DATE" &&
    NEWS_DATE="$COMMIT_DATE" ||
      NEWS_DATE="$(stat -c %y NEWS.md | sed 's/\.[0-9]\+//')" ||
      NEWS_VERSION=
fi
test -n "$NEWS_VERSION" &&
  exit_with_version "$NEWS_VERSION-$UNOFFICIAL" "$NEWS_VERSION-$UNOFFICIAL$(date +-%y%m%d)" "$NEWS_DATE"

echo "$0: ERROR: Failed to find project version, use a valid git repository or release tarball" >&2
exit 97
