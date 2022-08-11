#!/bin/bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail #-x

SCRIPTNAME=`basename $0`
function die  { [ -n "$*" ] && echo "$SCRIPTNAME: $*" >&2; exit 127 ; }
umask 022	# create 0755 dirs by default
ORIG_PWD=$PWD

# paths
TARBALL=`readlink -f $1`
TARDIR=`dirname $TARBALL`
ASSETLIST=$TARDIR/build-assets
UID_=`id -u`
OOTBUILD="${OOTBUILD:-/tmp/anklang-ootbuild$UID_}"

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

# extract tarball, build
tar xf $TARBALL -C $OOTBUILD --strip-components=1
( cd $OOTBUILD
  make -j`nproc` \
       build-version \
       out/doc/anklang-manual.pdf \
       out/doc/anklang-internals.pdf \
       all
  VERSION=`cut '-d ' -f1 out/build-version`
  cp out/doc/anklang-manual.pdf out/anklang-manual-$VERSION.pdf
  cp out/doc/anklang-internals.pdf out/anklang-internals-$VERSION.pdf
  make check
)

# make packages
( cd $OOTBUILD
  BUILDDIR=out misc/mkdeb.sh
)
# TODO: AppImage

# Gather assets
cp $OOTBUILD/out/build-version $TARDIR/
VERSION=`cut '-d ' -f1 $TARDIR/build-version`
echo $TARBALL			>  $ASSETLIST.tmp
for f in $OOTBUILD/out/anklang_${VERSION}_amd64.deb \
	 $OOTBUILD/out/anklang-manual-$VERSION.pdf \
	 $OOTBUILD/out/anklang-internals-$VERSION.pdf \
	 ; do
  cp $f $TARDIR
  echo $TARDIR/${f##*/}		>> $ASSETLIST.tmp
done
du -hs `cat $ASSETLIST.tmp`

# done
mv $ASSETLIST.tmp $ASSETLIST
