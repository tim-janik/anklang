#!/bin/bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail #-x

SCRIPTNAME=`basename $0`
function die  { [ -n "$*" ] && echo "$SCRIPTNAME: $*" >&2; exit 127 ; }

# paths
BUILDDIR="${BUILDDIR:-out}"
DROOT=$BUILDDIR/mkdeb/
PKGDIR=$(sed -rn '/^ *"pkgdir":/{ s/.*:.*"([^"]+)", *$/\1/; p; q; }' $BUILDDIR/package.json)
DEBIAN=$DROOT/DEBIAN
PKGDOCDIR=$DROOT/$PKGDIR/doc
ANKLANGSYNTHENGINE=$PKGDIR/lib/AnklangSynthEngine
MAKE="make -w V=${V:-}"

# config for /opt/:
: $MAKE default prefix=/opt pkgprefix=/opt sharedir=/usr/share bindir=/usr/bin

# start from scratch
rm -rf $DROOT

# create 0755 dirs by default
umask 022

# install into $DROOT
rm -f -r "$DROOT"
$MAKE install "DESTDIR=$DROOT"
ls "$DROOT"/$PKGDIR >/dev/null # check dir

# find prefix dir for MIME and .desktop files
PREFIXDIR=/$(realpath --relative-to="$DROOT" "$DROOT"/$PKGDIR/../../)
ls "$DROOT"$PREFIXDIR >/dev/null # check dir

# Meta info
NAME="anklang"
VERSION=$(misc/version.sh | cut -d\  -f1)
GITCOMMIT=$(misc/version.sh | (read v h d && echo $h))
DUSIZE=$(cd $DROOT && du -k -s .)
ARCH=$(dpkg --print-architecture)

# Dependencies
D="libc6 (>= 2.31)"
D="$D, libstdc++6:amd64 (>= 10.2.0)"
D="$D, zlib1g, libzstd1:amd64, python3"
D="$D, libasound2, libflac8 (>= 1.3.3), libmad0"
D="$D, libogg0, libvorbis0a, libvorbisenc2, libvorbisfile3 (>= 1.3.5)"
D="$D, libglib2.0-0 (>= 2.64.6), libgtk2.0-0 (>= 2.24.32)"
D="$D, libgtk-3-0 (>= 3.24.18), libnss3 (>= 2:3.49)"
# libfluidsynth2 (>= 2.1.1)

# DEBIAN/
mkdir -p $DEBIAN

# DEBIAN/control
cat >$DEBIAN/control <<-__EOF
	Package: $NAME
	Version: $VERSION
	License: MPL-2.0
	Section: sound
	Priority: optional
	Maintainer: Tim Janik <timj@gnu.org>
	Homepage: https://anklang.testbit.eu
	Installed-Size: ${DUSIZE%.}
	Architecture: $ARCH
	Depends: $D
	Description: Music Synthesizer and Composer
	 Anklang is a MIDI sequencer and synthesizer and DAW for Live Audio Sessions.
	 .
	 Anklang is a digital audio synthesis application for live
	 creation and composition of music and other audio material.
	 It is released as free software under the MPL-2.0.
	 .
	 This package contains the entire distribution.
__EOF
MAINTAINER=$(sed -rn '/^Maintainer:/{ s/.*: //; p; q; }' $DEBIAN/control)

# DEBIAN/conffiles
#echo -n >$DEBIAN/conffiles

# generate changelog.Debian entry
dch_msg() {
  PACKAGE="$1"; VERSION="$2"; RELEASE="$3"; URGENCY="$4"; shift 4
  echo "$PACKAGE ($VERSION) $RELEASE; urgency=$URGENCY" && echo
  while [ $# -gt 0 ] ; do
    echo -e "$1" | sed 's/^/  /'
    shift
  done
  echo -e "\n -- $MAINTAINER " $(date -R)
}

# changelog.Debian.gz (via dch)
DEBCHANGELOG=$PKGDOCDIR/changelog.Debian
dch_msg "$NAME" "$VERSION" unstable medium \
	"* ${NAME^} build: https://github.com/tim-janik/anklang/" \
	"* git commit $GITCOMMIT" \
	> $DEBCHANGELOG
gzip -9 $DEBCHANGELOG

# DEBIAN/postinst
cat <<\__EOF |
#!/bin/bash
set -e -o pipefail

# Setcap cap_sys_nice to allow renincing for low latency processing
if [ "$1" = configure ] && command -v setcap > /dev/null ; then
   setcap cap_sys_nice+ep @ANKLANGSYNTHENGINE@ ||
	echo "$0: failed to enable 'setcap cap_sys_nice' on @ANKLANGSYNTHENGINE@" >&2
fi

# Debian lacks triggers outside /usr/share/
# https://www.debian.org/doc/debian-policy/ch-maintainerscripts.html#s-mscriptsinstact
if [ "$1" = configure ] ; then
   # https://www.debian.org/doc/debian-policy/ch-sharedlibs.html#s-ldconfig
   #ldconfig
   which update-mime-database >/dev/null 2>&1 &&
	update-mime-database @PREFIXDIR@/share/mime/
   which update-desktop-database >/dev/null 2>&1 &&
	update-desktop-database @PREFIXDIR@/share/applications/
fi

exit 0
__EOF
sed "s|@ANKLANGSYNTHENGINE@|$ANKLANGSYNTHENGINE|g; s|@PREFIXDIR@|$PREFIXDIR|g" > $DEBIAN/postinst
chmod +x $DEBIAN/postinst

# DEBIAN/postrm
# Needed because Debian does not provide triggers for /usr/local/
cat <<\__EOF |
#!/bin/bash
set -e -o pipefail
which update-mime-database >/dev/null 2>&1 && {
	mkdir -p @PREFIXDIR@/share/mime/packages  # required by update-mime-database
	update-mime-database @PREFIXDIR@/share/mime/
}
__EOF
sed "s|@ANKLANGSYNTHENGINE@|$ANKLANGSYNTHENGINE|g; s|@PREFIXDIR@|$PREFIXDIR|g" > $DEBIAN/postrm
chmod +x $DEBIAN/postrm

# https://wiki.debian.org/ReleaseGoals/LAFileRemoval
find $DEBIAN/../ -name '*.la' -delete

# https://lintian.debian.org/tags/package-installs-python-bytecode
find $DEBIAN/../ -name '*.py[co]' -delete

# create binary deb
fakeroot dpkg-deb -Zzstd -z9 -b $DROOT $DROOT/..
ls -al $BUILDDIR/$NAME''_$VERSION''_$ARCH.deb
echo lintian -i --no-tag-display-limit $BUILDDIR/$NAME''_$VERSION''_$ARCH.deb
