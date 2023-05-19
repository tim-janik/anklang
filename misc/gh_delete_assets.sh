#!/bin/bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail #-x

SCRIPTNAME=${0##*/} ; die() { [ -z "$*" ] || echo "$SCRIPTNAME: $*" >&2; exit 128 ; }

TAG="$1"

# List all Github release assets of TAG in shell format
ASSETS=$(gh release view $TAG --json assets | jq -r '.assets[] | [.name] | @sh')
# Parse into array
eval ASSETS="( $ASSETS )"

# Delete each asset
for asset in "${ASSETS[@]}" ; do
  (set -x && gh release delete-asset $TAG "$asset" -y)
done
