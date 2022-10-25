#!/bin/bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail #-x

SCRIPTNAME=`basename $0`
function die  { [ -n "$*" ] && echo "$SCRIPTNAME: $*" >&2; exit 127 ; }
umask 022	# create 0755 dirs by default
ORIG_PWD=$PWD

# paths & commands
TARBALL=`readlink -f $1`
TARDIR=`dirname $TARBALL`
ASSETLIST=$TARDIR/build-assets
UID_=`id -u`
OOTBUILD="${OOTBUILD:-/tmp/anklang-ootbuild$UID_}"
MAKE="make -w V=${V:-}"

# prepare build dir
mkdir -p $OOTBUILD/
( cd $OOTBUILD
  rm -rf * .[^.]* ..?*
)

# link .dlcache
test -d .dlcache/ && {
  rm -rf $OOTBUILD/.dlcache
  ln -s $PWD/.dlcache $OOTBUILD/.dlcache
}

# extract tarball, extract release infos
tar xf $TARBALL -C $OOTBUILD --strip-components=1
( cd $OOTBUILD
  misc/version.sh
)		> $TARDIR/build-version
cp $OOTBUILD/NEWS.md $TARDIR/build-news
OOTBUILD_VERSION=`cut '-d ' -f1 $TARDIR/build-version`

# build and check
( cd $OOTBUILD
  $MAKE -j`nproc` \
	MODE=production \
	INSN=sse \
	all pdf
  $MAKE check
)
cp $OOTBUILD/out/doc/anklang-manual.pdf $OOTBUILD/out/anklang-manual-$OOTBUILD_VERSION.pdf
cp $OOTBUILD/out/doc/anklang-internals.pdf $OOTBUILD/out/anklang-internals-$OOTBUILD_VERSION.pdf

# make Deb
( cd $OOTBUILD
  BUILDDIR=out V=$V misc/mkdeb.sh
)

# make AppImage
( cd $OOTBUILD
  BUILDDIR=out V=$V misc/mkAppImage.sh
  test ! -r /dev/fuse || time out/anklang-$OOTBUILD_VERSION-x64.AppImage --quitstartup
)

# Gather assets
echo ${TARDIR##*/}/${TARBALL##*/}	>  $ASSETLIST.tmp
for f in $OOTBUILD/out/anklang_${OOTBUILD_VERSION}_amd64.deb \
	 $OOTBUILD/out/anklang-$OOTBUILD_VERSION-x64.AppImage \
	 $OOTBUILD/out/anklang-manual-$OOTBUILD_VERSION.pdf \
	 $OOTBUILD/out/anklang-internals-$OOTBUILD_VERSION.pdf \
	 ; do
  cp $f $TARDIR
  echo ${TARDIR##*/}/${f##*/}		>> $ASSETLIST.tmp
done
du -hs `cat $ASSETLIST.tmp`

# done
mv $ASSETLIST.tmp $ASSETLIST
