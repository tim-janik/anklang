#!/bin/bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail

B= C= F= G= R= T= Y= Z=

# use colors if stdout is a tty
test -t 1 && {
  B=$'\e[1m'	# bold
  C=$'\e[36m'	# cyan
  F=$'\e[39m'	# foreground
  G=$'\e[32m'	# green
  R=$'\e[31m'	# red
  T=$'\e[36m'	# turquoise
  Y=$'\e[33m'	# yellow
  Z=$'\e[0m'	# reset
}

# colorization patterns
PAT=(
  "s/^([^ :]+):([^ ]+:)? /$B$C\1$F:\2$Z /;"	# file name
  "s/(\[Warning\/[^]]*])/$B$Y\1$Z/;"		# eslint warning
  "s/(\[Error\/[^]]*])/$B$R\1$Z/;"		# eslint error
)

sed -r -e "${PAT[*]}"
