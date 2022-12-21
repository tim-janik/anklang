#!/bin/bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail
# set -x

# == Config ==
S=1	# scaling
W=32	# width
H=32	# height
ORIGIN=$(pwd)

# == args ==
SCRIPTNAME=${0##*/} ; die() { [ -z "$*" ] || echo "$SCRIPTNAME: $*" >&2; exit 9 ; }
HOTSPOT=false
while test $# -ne 0 ; do
  case "$1" in
    --hotspot)          HOTSPOT=true ;;
    *)			die "unknown argument: '$1'" ;;
  esac
  shift
done

# == Create TMP ==
TMP=$(pwd) && TMP="$TMP/build.$UID"
test -e $TMP/node_modules/.bin/svgtofont || (
  rm -rf $TMP/
  mkdir -p $TMP/
  cd $TMP/
  npm i svgtofont svgo@2.8.0 )
rm -rf $TMP/svgs/ $TMP/font/
mkdir -p $TMP/svgs/

# == Dist basics ==
rm -fr anklangicons/
test "${1:-}" != clean || exit
mkdir -p anklangicons/
pandoc README.md -t markdown -o anklangicons/README
ANKLANGVERSION=$(git describe --match 'v[0-9]*.[0-9]*.*[0-9a]' --abbrev=8 --long)
echo >> anklangicons/README
echo "Build with anklangicons.sh from anklang-$ANKLANGVERSION" >> anklangicons/README

# == Optimize Glyph ==
optimize_glyph()
{
  F="$1"
  G="$TMP/svgs/${F##*/}"
  test "$F" = "$G" || cp "$F" "$G"
  $TMP/node_modules/.bin/svgo -i $G --multipass -o $G
}

# == Populate Glyphs ==
for SVG in icons/*.svg ; do
  cp "$SVG" $TMP/svgs/
done

# == Turn Glyphs into Paths ==
for SVG in $TMP/svgs/*.svg ; do
  inkscape $SVG --verb=EditSelectAll --verb=ObjectToPath --verb=StrokeToPath --verb=FileSave --verb=FileQuit
  inkscape $SVG --vacuum-defs --export-plain-svg $SVG
done

# == Optimize Glyphs ==
for SVG in $TMP/svgs/*.svg ; do
  optimize_glyph "$SVG"
done

# == Create Font ==
( cd $TMP/
  node_modules/.bin/svgtofont --sources svgs/ --output ./font --fontName AnklangIcons
  # Keep just woff2 reference in .css
  sed -r '/\burl/s/(AnklangIcons\.woff2)\?t=[0-9]+/\1/; s/[^ ]*url(.*woff2.*)[,;]/src:URL\1;/; /url/d; s/src:URL/src: url/' -i font/AnklangIcons.css
)
cp $TMP/font/AnklangIcons.css $TMP/font/AnklangIcons.woff2 anklangicons/

# == AnklangCursors.scss ==
mv anklangicons/cursors.tmp anklangicons/AnklangCursors.scss

# == dist ==
DATE=$(date +%y%m%d)
fgrep -s -q "# $DATE" $TMP/lastminor ||	echo "0" > $TMP/lastminor
MINOR=$(sed 's/ #.*//' $TMP/lastminor)
MINOR=$(( $MINOR + 1 )) &&		echo "$MINOR # $DATE" > $TMP/lastminor
TARBALL=anklangicons-$DATE.$MINOR.tgz
tar --mtime="$DATE" -cf $TARBALL --exclude '*.hotspot.png' anklangicons/
$HOTSPOT || rm -r anklangicons/
tar tvf $TARBALL
ls -l $TARBALL
sha256sum $TARBALL

# == upload cmd ==
echo "# hub release edit -m '' -a $ORIGIN/$TARBALL buildassets-v0"
