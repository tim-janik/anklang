#!/bin/bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail #-x

SCRIPTNAME=`basename $0`
function die  { [ -n "$*" ] && echo "$SCRIPTNAME: $*" >&2; exit 127 ; }

# config for /opt/:
: make default prefix=/opt pkgprefix=/opt sharedir=/usr/share bindir=/usr/bin

# paths
BUILDDIR=out
DROOT=$BUILDDIR/mkdeb/
PKGDIR=$(sed -rn '/^ *"pkgdir":/{ s/.*:.*"([^"]+)", *$/\1/; p; q; }' out/package.json)
DEBIAN=$DROOT/DEBIAN
DEBDOCDIR=$DROOT/$PKGDIR/doc
ANKLANGLAUNCHER=$PKGDIR/bin/anklang

# start from scratch
rm -rf $DROOT

# create 0755 dirs by default
umask 022

# install into $DROOT
#make && make check
make install "DESTDIR=$DROOT"
#make installcheck "DESTDIR=$DROOT"

# Meta info
NAME="anklang"
VERSION=$(misc/version.sh | cut -d\  -f1)
GITCOMMIT=$(git rev-parse --verify HEAD)
DUSIZE=$(cd $DROOT && du -k -s .)
ARCH=$(dpkg --print-architecture)

# Dependencies
D="libc6 (>= 2.31)"
D="$D, libstdc++6:amd64 (>= 10.2.0), libboost-system1.71.0"
D="$D, zlib1g, libzstd1:amd64, python3"
D="$D, libasound2, libflac8 (>= 1.3.3), libfluidsynth2 (>= 2.1.1), libmad0"
D="$D, libogg0, libvorbis0a, libvorbisenc2, libvorbisfile3 (>= 1.3.5)"
D="$D, libglib2.0-0 (>= 2.64.6), libgtk2.0-0 (>= 2.24.32)"

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
	 It is released as free software under the MPL-2.0
	 and includes plugins under the GNU LGPL-2.1+.
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
DEBCHANGELOG=$DEBDOCDIR/changelog.Debian
dch_msg "$NAME" "$VERSION" unstable medium \
	"* ${NAME^} build: https://github.com/tim-janik/anklang/" \
	"* git commit $GITCOMMIT" \
	> $DEBCHANGELOG
gzip -9 $DEBCHANGELOG

# /usr/share/doc/PACKAGE/ -> ..PACKAGE/doc/ - provide mandatory Debian package docs
(cd $DROOT && mkdir -p usr/share/doc/ && ln -s ../../..$PKGDIR/doc usr/share/doc/$NAME)

# postinst, postrm - generate header
write_pkgscript_header()
{
  cat <<-\__EOF
	#!/bin/bash
	set -e -o pipefail
	set_perms() {
	  USER="$1"; GROUP="$2"; MODE="$3"; FILE="$4"
	  # https://www.debian.org/doc/debian-policy/ch-files.html#s10.9.1
	  if ! dpkg-statoverride --list "$FILE" > /dev/null ; then
	    chown "$USER:$GROUP" "$FILE"
	    chmod "$MODE" "$FILE"
	  fi
	}
__EOF
}

# DEBIAN/postinst
write_pkgscript_header >$DEBIAN/postinst
cat <<-\__EOF |
	# https://www.debian.org/doc/debian-policy/ch-maintainerscripts.html#s-mscriptsinstact
	case "$1" in
	  configure)
	    # https://www.debian.org/doc/debian-policy/ch-files.html#s-permissions-owners
	    #set_perms root root 4755 @ANKLANGLAUNCHER@	# wrapper which does renice -20
	    # https://www.debian.org/doc/debian-policy/ch-sharedlibs.html#s-ldconfig
	    #ldconfig
	    ;;
	  abort-upgrade|abort-remove|abort-deconfigure) ;;
	  *) exit 1 ;; # unkown action
	esac
	exit 0
__EOF
sed "s|@ANKLANGLAUNCHER@|$ANKLANGLAUNCHER|g" >> $DEBIAN/postinst
chmod +x $DEBIAN/postinst

# https://wiki.debian.org/ReleaseGoals/LAFileRemoval
find $DEBIAN/../ -name '*.la' -delete

# https://lintian.debian.org/tags/package-installs-python-bytecode
find $DEBIAN/../ -name '*.py[co]' -delete

# create binary deb
fakeroot dpkg-deb -b $DROOT $DROOT/.. # -Zgzip
ls -al $BUILDDIR/$NAME''_$VERSION''_$ARCH.deb
echo lintian -i --no-tag-display-limit $BUILDDIR/$NAME''_$VERSION''_$ARCH.deb
