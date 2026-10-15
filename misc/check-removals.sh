#!/usr/bin/env bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail

# Commits not in trunk that delete at least 10 more lines of code than they add need an "Evidence:" paragraph.
mapfile -t bases < <(git for-each-ref --format='%(refname)' refs/heads/trunk refs/remotes/origin/trunk 2>/dev/null)
test ${#bases[@]} -gt 0 || exit 0
status=0
for commit in $(git rev-list --no-merges HEAD --not "${bases[@]}") ; do
  removed=$(git diff --numstat -M "$commit^!" -- \
              '*.c' '*.cc' '*.cpp' '*.h' '*.hh' '*.hpp' '*.js' '*.mjs' '*.cjs' '*.jsx' '*.ts' '*.tsx' '*.css' '*.py' '*.sh' '*.mk' \
              ':!*/tests/*' ':!*_test.*' ':!*/test_*' ':!*.g.*' |
            awk '$1 != "-" { n += $2 - $1 } END { print n + 0 }')
  test "$removed" -ge 10 || continue
  git log -1 --format=%B "$commit" | grep -q '^Evidence:' && continue
  echo "$(git log -1 --format='%h %s' "$commit"): removes $removed lines of code without an Evidence: paragraph" >&2
  status=1
done
exit "$status"
