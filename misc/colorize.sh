#!/usr/bin/env bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail

# avoid colorize if stdout is not a tty
test -t 1 || exec cat

# avoid colorize if $TERM=dumb and not in Emacs
test "${TERM:-dumb}${INSIDE_EMACS:-}" == dumb && exec cat

# color shortcuts
B= C= F= G= R= T= Y= Z=
e=$'\e'		# Escape
B=$'\e[1m'	# bold
C=$'\e[36m'	# cyan
F=$'\e[39m'	# foreground
G=$'\e[32m'	# green
K=$'\e[30m'	# black
R=$'\e[31m'	# red
T=$'\e[36m'	# turquoise
Y=$'\e[33m'	# yellow
M=$'\e[35m'	# magenta
Z=$'\e[0m'	# reset

# colorization patterns
PAT=(
  "s/(\[Warning\/[^]]*])/$B$M\1$Z/;"		# eslint warning
  "s/ (warning:) / $B$M\1$Z /;"			# cc warning
  "s/(\[Error\/[^]]*])/$B$R\1$Z/;"		# eslint error
  "s/ (error:) / $B$R\1$Z /;"			# cc error
  "s/ (note:[^$e]+)/ $B$Y\1$Z/;"		# lint note
)

# Emacs does its own line coloring
test "${INSIDE_EMACS:-}" == "" && PAT+=(
    "s/^([^ :$e]+):([0-9:]+:)? /$B\1:\2$Z /;"	# file name
  )

stdbuf -eL -oL \
       sed -Ee "${PAT[*]}"
