#!/usr/bin/env bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail

SCRIPTNAME=${0##*/} ; die() { [ -z "$*" ] || echo "$SCRIPTNAME: $*" >&2; exit 9 ; }
L=${VERSION_HASHLENGTH:-7}

# == NEWS.md versions ==
# Determine version from NEWS.md file
fetch_news_version() # fetch_news_version {1|2}
{
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
  test -r NEWS.md || die "Missing NEWS.md file"
  sed -nre "$SEDSCRIPT" NEWS.md
}
# Usage: version.sh --news-tag1
test "${1:-}" == "--news-tag1" && {
  NEWS_VERSION="$(fetch_news_version 1)"
  test -n "$NEWS_VERSION" || die "ERROR: failed to extract release tag from NEWS.md"
  echo "v${NEWS_VERSION#v}"
  exit
}
# Usage: version.sh --news-tag2
test "${1:-}" == "--news-tag2" && {
  NEWS_VERSION="$(fetch_news_version 2)"
  test -n "$NEWS_VERSION" || die "ERROR: failed to extract release tag from NEWS.md"
  echo "v${NEWS_VERSION#v}"
  exit
}

# == Git describe ==
# Commit information provided by git-archive in export-subst format string, see gitattributes(5)
COMMITINFO=(
  '$Format:%H$'						# HASH:     [0-9a-f]+
  '$Format:%(describe:match=v[0-9]*.[0-9]*.[0-9]*)$'	# DESCRIBE: vN.N.N-3-g123abc
  '$Format:%ci$'					# DATE:     2001-01-01 01:01:01 +0100
  '$Format:%D,$'					# REFS:     HEAD -> next, tag: v0.0.0, tag: Daily,
)
# If unset, assign Format:PLACEHOLDERS from .git/
if test "${COMMITINFO[0]}" == "${COMMITINFO[0]/:/}" ; then
  ARCHIVE_VERSION=true
else
  ARCHIVE_VERSION=false
  for i in "${!COMMITINFO[@]}" ; do
    [[ ${COMMITINFO[$i]} =~ ^[$]Format[:]([^\$]+)\$$ ]] || die 'missing $Format$ in' "COMMITINFO[$i]"
    COMMITINFO[$i]=`git log -1 --pretty="tformat:${BASH_REMATCH[1]}" `
    [[ ${COMMITINFO[$i]} =~ describe:match ]] && # git 2.25.1 cannot describe:match
      COMMITINFO[$i]=`git describe --match='v[0-9]*.[0-9]*.[0-9]*' 2>/dev/null || :`
  done
fi

# == Git COMMIT-HASH ==
# Usage: version.sh --commit-hash
test "${1:-}" == "--commit-hash" && {
  echo "${COMMITINFO[0]}"
  exit
}

# == Git COMMIT-DATE ==
# Usage: version.sh --commit-date
test "${1:-}" == "--commit-date" && {
  echo "${COMMITINFO[2]}"
  exit
}

# == build_id ==
# Output: <VerionName> <BuildId> <VersionDate>
# - <VerionName> is used for package names
# - <BuildId> uniquely identifies the build version and commit (like git describe --long)
# - <VersionDate> is used in e.g. manual pages
build_id() { # build_id [POSTFIX]
  echo "v$VERSIONTAG${1:-}+g${COMMITINFO[0]:0:$L}"					# Build id has metadata hash, similar to git describe --long
}

# == RELEASE_VERSION ==
if test "$ARCHIVE_VERSION" = true -o "${RELEASE_VERSION:-}" == true &&
    [[ ${COMMITINFO[1]} =~ ^v([0-9]+\.[0-9]+\.[0-9]+(-[a-zA-Z]+[0-9.]*)?)$ ]] &&	# Version tag without -0-gcommit
    test v"$(fetch_news_version 1)" == "${COMMITINFO[1]}"				# Version tag matches release NEWS
then
  VERSIONTAG="${COMMITINFO[1]#v}"							# Strip 'v' prefix if any
  echo "$VERSIONTAG" "$(build_id '-0')" "${COMMITINFO[2]}" && exit
fi

# == VERSIONTAG ==
if [[ ${COMMITINFO[1]} =~ ^v([0-9]+\.[0-9]+\.[0-9]+(-[a-zA-Z]+[0-9.]*)?)$ ]] ; then	# Version tag without hash
  VERSIONTAG="${BASH_REMATCH[1]}.dev0"							# Force .dev0 postfix
elif [[ ${COMMITINFO[1]} =~ ^v([0-9]+\.[0-9]+\.[0-9]+(-[a-zA-Z]+[0-9.]*)?)-([0-9]+)-g[0-9a-f]+$ ]]
then											# Version tag with hash
  VERSIONTAG="${BASH_REMATCH[1]}.dev${BASH_REMATCH[3]}"					# Force .dev000 postfix
else
  die "ERROR: failed to detect project version"
fi

# == Development ==
echo "$VERSIONTAG" "$(build_id)" "${COMMITINFO[2]}" && exit
