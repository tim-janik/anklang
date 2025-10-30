#!/usr/bin/env bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail

(
  # Commit information provided by git-archive in export-subst format string, see gitattributes(5)
  read VDESCRIBE VHASH VDATE <<<' $Format: %(describe:match=v[0-9]*.[0-9]*.[0-9]*) %H %ci $ '

  # Use baked-in version info if present
  ! [[ "$VDATE" =~ % ]] &&
    echo "${VDESCRIBE#v} $VHASH $VDATE" && exit

  # Use version info from repository containing script
  cd $(dirname $(readlink -f "$0"))

  # Export GIT_DIR from JJ workspace
  WORKHEAD=HEAD		# revision for `git describe`
  git rev-parse --git-dir >/dev/null 2>&1 || {
    GIT_DIR=$(jj git root --ignore-working-copy --no-pager 2>/dev/null) && {
      export GIT_DIR
      WORKHEAD=$(jj show --ignore-working-copy --no-pager --no-patch -T commit_id @-)
    }
  }

  # Prefer exact tags (even if light, like nightly) over annotated tags, needs non-shallow clones
  VDESCRIBE=$(git describe --exact-match --tags --match='v[0-9]*.[0-9]*.[0-9]*' $WORKHEAD 2>/dev/null ||
		git describe --match='v[0-9]*.[0-9]*.[0-9]*' $WORKHEAD 2>/dev/null) &&
    echo "${VDESCRIBE#v} `git -P log -1 --pretty='%H %ci' $WORKHEAD`" && exit

  # Fallback, unversioned
  echo "0.0.0-g000000000 0000000000000000000000000000000000000000 2001-01-01 01:01:01 +0000"
) |
  # Enfore .dev<NN> and +g<hash> postfix
  sed 's/^\([^ -]\+\)-\([0-9]\+\)/\1.dev\2/; s/^\([^ -]\+\)-g/\1+g/'
