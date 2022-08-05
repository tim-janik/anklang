#!/usr/bin/env bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail

SCRIPTNAME=${0##*/} ; die() { [ -z "$*" ] || echo "$SCRIPTNAME: $*" >&2; exit 9 ; }
L=${VERSION_HASHLENGTH:-7}

# Determine project version.
#
# Version info sources:
# 1) COMMITINFO substitutions of $Format$ during git-archive
# 2) COMMITINFO assignments using .git/ and git-describe
# 3) HASH and date in COMMITINFO from shallow .git/ repos (e.g. during Github CI)
# 4) Release news and version in NEWS.md, used to cross-check release tags
# Consider:
# - Release versions can be full .git/ repos or git-archive tarballs
# - Release tags must match the most recent NEWS.md update
# - Non-release versions *must* have a version number postfix
# - Nightly versions include date and hash
#
# Output: <VerionName> <BuildId> <VersionDate>
# - <VerionName> is used for package names
# - <BuildId> uniquely identifies the build version and commit (like git describe --long)
# - <VersionDate> is used in e.g. manual pages

NIGHTLY=false

# exit_with_version <version> <buildid> <releasedate>
exit_with_version() {
  V="${1#v}"	# strip 'v' prefix if any
  shift
  if $NIGHTLY ; then
    [[ $V =~ nightly ]] || die "Not a nightly tag: $V"
    echo "$V"
  else
    echo "$V" "$@"
  fi
  exit 0
}

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

# Make nightly version from git describe
make_nightly() # make_nightly <Format:describe> <Format:date>
{
  local VDESCRIBE VDATE V
  VDESCRIBE="$1" && VDATE="$2"
  VDATE="${VDATE%% *}" && VDATE="${VDATE#20}" && VDATE="${VDATE//-/}" # strip down to %y%m%d
  V=$(echo "$VDESCRIBE" | sed -r "s/^v//;    s/-.*-g/-nightly$VDATE-g/")
  [[ "$V" =~ ^[0-9.]+-nightly[0-9]+-g[a-f0-9]+$ ]] || die "Failed to parse Nightly tag description"
  echo "$V"
}

# Commit information provided by git-archive in export-subst format string, see gitattributes(5)
COMMITINFO=(
  '$Format:%H$'						# [0-9a-f]+
  '$Format:%(describe:match=v[0-9]*.[0-9]*.[0-9]*)$'	# vN.N.N-3-g123abc
  '$Format:%ci$'					# 2001-01-01 01:01:01 +0100
  '$Format:%D,$'					# HEAD -> next, tag: v0.0.0, tag: Daily,
)
# If unset, assign from .git/
test "${COMMITINFO[0]}" == "${COMMITINFO[0]/:/}" ||
  for i in "${!COMMITINFO[@]}" ; do
    [[ ${COMMITINFO[$i]} =~ ^[$]Format[:]([^\$]+)\$$ ]] || die 'missing $Format$ in' "COMMITINFO[$i]"
    COMMITINFO[$i]=`git log -1 --pretty="tformat:${BASH_REMATCH[1]}" `
  done

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
    --commit-hash)
      echo "${COMMITINFO[0]}" && exit ;;
    --make-nightly)
      NIGHTLY=true
      ;;
    --help)
      cat <<-__EOF
	Usage: $0 [OPTIONS]
	  --help         Show brief usage info
	  --make-nightly Print nightly version without checking NEWS.md
	  --news-tag1    Extract latest version from NEWS.md
	  --news-tag1    Extract next to latest version from NEWS.md
__EOF
      exit 0 ;;
    *)
      true ;;
  esac
  shift
done

# Fetch NEWS.md version to compare against releast tags
NEWS_VERSION="$(fetch_news_version 1)"

# Handle releases
if [[ ${COMMITINFO[1]} =~ ^v([0-9]+\.[0-9]+\.[0-9]+(-[a-zA-Z]+[0-9.]*)?)$ ]] && # match version tag
     test v"$NEWS_VERSION" == "${COMMITINFO[1]}"				# version tag matches release NEWS
then
  VERSIONTAG="${COMMITINFO[1]}"
  BUILD_ID="$VERSIONTAG-0-g${COMMITINFO[0]:0:$L}"				# add hash as in git describe --long
  # Regular release
  exit_with_version "$VERSIONTAG" "$BUILD_ID" "${COMMITINFO[2]}"
fi

# Handle Nightly versions
if [[ ${COMMITINFO[3]} =~ tag:\ Nightly, ]] ; then				# on Nightly tag
  NIGHTLY_ID=`make_nightly "${COMMITINFO[1]}" "${COMMITINFO[2]}"`
  test "$NIGHTLY" == true -o "$NIGHTLY_ID" = "$NEWS_VERSION" && {
    # Nightly release version ID already contains shorthash
    exit_with_version "$NIGHTLY_ID" "v$NIGHTLY_ID" "${COMMITINFO[2]}"
  } # fall throughh
fi

# Handle development versions
if [[ ${COMMITINFO[1]} =~ ^v([0-9]+\.[0-9]+\.[0-9]+(-[a-zA-Z]+[0-9.]*)?)-([0-9]+-g[0-9a-f]+)$ ]] # match commit ahead of tag
then
  if test -z "${BASH_REMATCH[2]}" ; then
    BUILD_NAME="${BASH_REMATCH[1]}-devel0"	# force postfix for non-releases
  else
    BUILD_NAME="${BASH_REMATCH[1]}"		# has postfix
  fi
  BUILD_ID="v$BUILD_NAME-${BASH_REMATCH[3]}"
  # A -devel version
  exit_with_version "$BUILD_NAME" "$BUILD_ID" "${COMMITINFO[2]}"
fi

# Shallow git repo, resorts to NEWS.md
if test "${COMMITINFO[0]}" == "${COMMITINFO[0]/:/}" &&		# have commit hash
    [[ $NEWS_VERSION =~ ^([0-9]+\.[0-9]+\.[0-9]+(-[a-zA-Z]+[0-9.]*)?)$ ]]
then
  if test -z "${BASH_REMATCH[2]}" ; then
    BUILD_NAME="${BASH_REMATCH[1]}-devel0"			# enforce version postfix
  else
    BUILD_NAME="${BASH_REMATCH[1]}"				# has postfix
  fi
  BUILD_ID="v$BUILD_NAME-shallow-g${COMMITINFO[0]:0:$L}"	# add hash as in git describe --long
  # A -devel or -shallow version (shallow git repo)
  exit_with_version "$BUILD_NAME" "$BUILD_ID" "${COMMITINFO[2]}"
fi

# bail out
test -d .git || die "ERROR: failed to find git repository"
die "ERROR: failed to find git commit or project version in NEWS.md"
