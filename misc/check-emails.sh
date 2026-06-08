#!/usr/bin/env bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail

# Pre-push hook: ensure all commits being pushed have author emails from a whitelist.
die() { echo "${BASH_SOURCE[0]##*/}: **ERROR**: $*" >&2; exit 1; }
test -x misc/check-emails.sh || die "must be run from project root"

# --- Whitelist: one email per line ---
read -r -d '' WHITELIST <<'EOF' || true
timj@gnu.org
stefan@space.twc.de
EOF
# --- End whitelist ---

bad=()
# When run via jj-pre-push -> pre-commit, refs come as env vars.
if [[ -n "${PRE_COMMIT_FROM_REF:-}" && -n "${PRE_COMMIT_TO_REF:-}" ]]; then
  remote_sha="$PRE_COMMIT_FROM_REF"
  local_sha="$PRE_COMMIT_TO_REF"
  [[ "$local_sha" =~ ^0+$ ]] && exit 0		# branch deletion
  [[ "$remote_sha" =~ ^0+$ ]] && remote_sha=""
  for e in $(git log --format="%ae" ${remote_sha:+"${remote_sha}.."}${local_sha} | sort -u); do
    grep -qxF "$e" <<<"$WHITELIST" || bad+=("$e")
  done
else  # Read from stdin (git pre-push hook format)
  while read -r local_ref local_sha remote_ref remote_sha; do
    [[ "$local_sha" =~ ^0+$ ]] && continue	# branch deletion
    [[ "$remote_sha" =~ ^0+$ ]] && remote_sha=""
    for e in $(git log --format="%ae" ${remote_sha:+"${remote_sha}.."}${local_sha} | sort -u); do
      grep -qxF "$e" <<<"$WHITELIST" || bad+=("$e")
    done
  done
fi

if (( ${#bad[@]} )); then
  { echo 'check-emails: unauthorized email(s):'
    printf '%s\n' "${bad[@]}" | sort -u | sed 's/^/  /'; } >&2
  exit 1
fi

# Test with two hashes (refs are ignored):
# echo "refs/heads/dummy1 d6c033ee refs/heads/dummy2 8bb6911c" | bash misc/check-emails.sh
