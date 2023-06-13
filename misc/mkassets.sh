#!/bin/bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail && SCRIPTNAME=${0##*/} && die() { [ -z "$*" ] || echo "$SCRIPTNAME: $*" >&2; exit 127 ; }

# Usage: mkassets.sh <TARDIR>/<TARBALL>
# Build assets from TARBALL in a separate directory and copy those assets
# together with `build-assets` into TARDIR.

umask 022	# create 0755 dirs by default

# paths & commands
TARBALL=`readlink -f $1`
TARDIR=`dirname $TARBALL`
ASSETLIST=$TARDIR/build-assets
SCRATCHDIR="/tmp/anklang-mkassets/"
MAKE="make -w V=${V:-}"

# Clear build dir, but keep it in place in case it is a mount point
mkdir -p $SCRATCHDIR/
test -O $SCRATCHDIR -a -w $SCRATCHDIR || die "SCRATCHDIR: build directory not writable"
( cd $SCRATCHDIR
  rm -rf * .[^.]* ..?*
)

# Link .dlcache to reduce network IO
test -d .dlcache/ && {
  rm -rf $SCRATCHDIR/.dlcache
  ln -s $PWD/.dlcache $SCRATCHDIR/.dlcache
}

# extract tarball, extract release infos
tar xf $TARBALL -C $SCRATCHDIR --strip-components=1
( cd $SCRATCHDIR
  misc/version.sh
)		> $TARDIR/build-version
cp $SCRATCHDIR/NEWS.md $TARDIR/build-news
TAR_VERSION=`cut '-d ' -f1 $TARDIR/build-version`

# Change to $SCRATCHDIR to build various assets
( cd $SCRATCHDIR

  # Build and check, a `production+sse` build also triggers FMA builds in ui/Makefile.mk
  $MAKE -j`nproc` \
	MODE=production \
	INSN=sse \
	all pdf
  $MAKE check
  cp out/doc/anklang-manual.pdf out/anklang-manual-$TAR_VERSION.pdf
  cp out/doc/anklang-internals.pdf out/anklang-internals-$TAR_VERSION.pdf

  # Make Deb
  BUILDDIR=out V=$V misc/mkdeb.sh

  # Make AppImage
  BUILDDIR=out V=$V misc/mkAppImage.sh
  test ! -r /dev/fuse || time out/anklang-$TAR_VERSION-x64.AppImage --quitstartup
)

# Gather assets
echo ${TARDIR##*/}/${TARBALL##*/}	>  $ASSETLIST.tmp
for f in $SCRATCHDIR/out/anklang_${TAR_VERSION}_amd64.deb \
	 $SCRATCHDIR/out/anklang-$TAR_VERSION-x64.AppImage \
	 $SCRATCHDIR/out/anklang-manual-$TAR_VERSION.pdf \
	 $SCRATCHDIR/out/anklang-internals-$TAR_VERSION.pdf \
	 ; do
  cp $f $TARDIR
  echo ${TARDIR##*/}/${f##*/}		>> $ASSETLIST.tmp
done
du -hs `cat $ASSETLIST.tmp`

# done
mv $ASSETLIST.tmp $ASSETLIST
