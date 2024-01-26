#!/usr/bin/env bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail #-x

SCRIPTNAME=${0##*/} ; die() { [ -z "$*" ] || echo "$SCRIPTNAME: $*" >&2; exit 9 ; }
ABSPATHSCRIPT=`readlink -f "$0"`

# Determine latest version from NEWS.md file
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
  sed -nre "$SEDSCRIPT" "${ABSPATHSCRIPT%/*}"/../NEWS.md
}
NEWS_TAG="v$(fetch_news_version 1)"

# Work around repos without tags
git rev-parse --verify "$NEWS_TAG" >/dev/null 2>&1 || NEWS_TAG=`git log --reverse --format=%H | sed -n '1p' `

# Just print topmost NEWS.md version
test " ${1:-}" == " --version" && {
  echo "$NEWS_TAG"
  exit 0
}

# When generating news for release tags...
if RELEASETAG=$(git describe --match='v[0-9]*.[0-9]*.[0-9]*' --exact-match 2>/dev/null) ; then
  # Copy release version news section from NEWS.md
  SEDSEC1="
    0,/^##?\s/n;		# Read ahead until first heading
    /^##?\s/Q;			# Quit processing before printing second heading
  "
  sed -re "$SEDSEC1" < NEWS.md |
    sed -z 's/\n\+$/\n/'	# Strip newlines at EOF
else
  # Generate pre-release news from git history
  CURRENT_VERSION=$("${ABSPATHSCRIPT%/*}"/version.sh | (read v h d && echo $v))
  echo "## Anklang $CURRENT_VERSION"
  echo
  echo 'Development version - may contain bugs or compatibility issues.'
  echo
  echo '``````````````````````````````````````````````````````````````````````````````````````'
  git log --pretty='%s    # %cd %an %h%n%w(0,4,4)%b' \
      --first-parent --date=short "$NEWS_TAG..HEAD" |
    sed -e '/^\s*Signed-off-by:.*<.*@.*>/d' |
    sed '/^\s*$/{ N; /^\s*\n\s*$/D }'
  echo '``````````````````````````````````````````````````````````````````````````````````````'
  echo
fi

echo "git log --first-parent --date=short --pretty=\"%s    # %cd %an %h%n%w(0,4,4)%b\" '$NEWS_TAG..HEAD' | sed -r 's/\s+[A-Z][a-zA-Z0-9-]*:.*<.*@.*>\s*$//' " >&2
