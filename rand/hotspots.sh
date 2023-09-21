#!/usr/bin/env bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail
#set -x

# == args ==
SCRIPTNAME=${0##*/} ; die() { [ -z "$*" ] || echo "$SCRIPTNAME: $*" >&2; exit 9 ; }
test -e ./hotspots.sh || die "must be called from anklang/rand"

# == Config ==
W=28	# width
H=28	# height

# == hotspots/ ==
rm -r -f hotspots/
mkdir hotspots/

# sed -nr '/cursors\/[^.]*\.svg/{ s/.*\(([^)]+)\)\s+([0-9]+)\s+([0-9]+)\b.*/ \1 \2 \3 /; p  }'  <cursors/cursors.css
sed -nr '/url.cursors\//{ s/.*url\(//; s/\)//; s/[,;].*//; p }' < ../ui/cursors/cursors.css |
  while read F X Y ; do
    N="${F#cursors/}" && N="${N%.svg}"					# bare name
    T=hotspots/"$N".tmpdot
    P=hotspots/"$N".png
    O=hotspots/"$N".hotspot.png
    echo $F $P $X $Y
    convert -background none ../ui/$F $P				# Render SVG to PNG
    convert -size $W'x'$H xc:none -draw "fill red point $X,$Y" $T.png	# Render a red hotspot
    convert $P $T.png -compose Atop -composite $O			# Compose hotspot
    rm -f $T.png $P
    echo $O
  done
