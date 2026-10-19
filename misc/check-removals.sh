#!/usr/bin/env bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail

# Commits not in trunk that delete a block of at least 10 lines of code, with nothing added in its place, need an "Evidence:" paragraph.
mapfile -t bases < <(git for-each-ref --format='%(refname)' refs/heads/trunk refs/remotes/origin/trunk 2>/dev/null)
test ${#bases[@]} -gt 0 || exit 0
status=0
warned=false
for commit in $(git rev-list --no-merges HEAD --not "${bases[@]}") ; do
  removed=$(git diff -U0 -M "$commit^!" -- \
              '*.c' '*.cc' '*.cpp' '*.h' '*.hh' '*.hpp' '*.js' '*.mjs' '*.cjs' '*.jsx' '*.ts' '*.tsx' '*.css' '*.py' '*.sh' '*.mk' \
              ':!*/tests/*' ':!*_test.*' ':!*/test_*' ':!*.g.*' |
            awk '/^@@ / && $3 ~ /,0$/ { split ($2, old, ","); if (old[2] + 0 > n) n = old[2] + 0 } END { print n + 0 }')
  test "$removed" -ge 10 || continue
  git log -1 --format=%B "$commit" | grep -q '^Evidence:' && continue
  $warned || { warned=true
	       echo "$0: some commits delete too many lines of code without an 'Evidence:' explanation in the commit message" >&2 ; }
  printf "Deletion of %4d lines: %s\n" "$removed" "$(git log -1 --format='%h %s' "$commit")" >&2
  status=1
done
exit "$status"
