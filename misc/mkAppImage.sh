#!/usr/bin/env bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail #-x

SCRIPTNAME=`basename $0`
function die  { [ -n "$*" ] && echo "$SCRIPTNAME: $*" >&2; exit 127 ; }
umask 022	# create 0755 dirs by default
ORIG_PWD=$PWD

# paths & options
BUILDDIR="${BUILDDIR:-out}"
VERSION=$(misc/version.sh | cut -d\  -f1)
APPINST=$BUILDDIR/appinst/		# install dir
APPBASE=$BUILDDIR/appbase/		# dir for packaging
APPTOOLS=$BUILDDIR/appimagetools	# AppImage build tools
PKGDIR=$(sed -rn '/^ *"pkgdir":/{ s/.*:.*"([^"]+)", *$/\1/; p; q; }' $BUILDDIR/package.json)
MAKE="make -w V=${V:-}"

# AppImage tooling
echo 'AppImage tooling...'
$MAKE out/appimagetools/appimage-runtime-zstd
test -f $APPTOOLS/appimage-runtime-zstd || die "missing appimage-runtime-zstd"
test -x $APPTOOLS/linuxdeploy-x86_64.AppImage || die "missing linuxdeploy-x86_64.AppImage"

# make install
echo 'Installing...'
rm -fr $APPINST $APPBASE
$MAKE install DESTDIR=$APPINST V=1

# Populate appinst/, linuxdeploy expects libraries under usr/lib, binaries under usr/bin, etc
# We achieve that by treating the anklang-$MAJOR-$MINOR/ installation directory as /usr/.
# Also, we hand-pick extra libs for Anklang to keep the AppImage small.
APPIMAGEPKGDIR=$APPBASE/${PKGDIR##*/}
mkdir $APPBASE
cp -a $APPINST/$PKGDIR $APPIMAGEPKGDIR
rm -f Anklang-x86_64.AppImage

# linuxdeploy
echo '  RUN     ' linuxdeploy...
if test -e /usr/lib64/libc_nonshared.a # Fedora
then
  LIB64=/usr/lib64/
else
  LIB64=/usr/lib/x86_64-linux-gnu/
fi
# Avoid `linuxdeploy {-e|-l}`, these options change the binary locations by copying.
# The copies mess up ELF location detection and restoring the original locations
# causes wrong relative $ORIGIN paths. We just use --deploy-deps-only now, which
# keeps the binaries in place and correctly adjusts the relative $ORIGIN path.
LD_LIBRARY_PATH=$APPIMAGEPKGDIR/lib \
  DISABLE_COPYRIGHT_FILES_DEPLOYMENT=1 \
  $APPTOOLS/linuxdeploy-x86_64.AppImage -v1 --appimage-extract-and-run \
  --appdir=$APPBASE \
  --deploy-deps-only $APPIMAGEPKGDIR/bin/anklang \
  --deploy-deps-only $APPIMAGEPKGDIR/lib/AnklangSynthEngine \
  --deploy-deps-only $APPIMAGEPKGDIR/lib/AnklangSynthEngine-fma \
  --deploy-deps-only $APPIMAGEPKGDIR/lib/gtk2wrap.so \
  --deploy-deps-only $APPIMAGEPKGDIR/lib/jackdriver.so \
  -i $APPIMAGEPKGDIR/ui/anklang.png \
  -d $APPIMAGEPKGDIR/share/applications/anklang.desktop \
  -l $LIB64/libXss.so.1 \
  -l $LIB64/libXtst.so.6 \
  --exclude-library="libnss3.so" \
  --exclude-library="libnssutil3.so" \
  --custom-apprun=misc/AppRun
# skip jackdriver.so, it is loaded only if the target system has all dependencies

# Provide /usr/bin/anklang entry
ln -v -s -r $APPIMAGEPKGDIR/bin/anklang $APPBASE/usr/bin/
test -x $APPBASE/usr/bin/anklang || die "$APPBASE/usr/bin/anklang: file is not executable"

# Create AppImage executable
echo '  BUILD   ' appimage-runtime...
mkdir -p assets/
mksquashfs $APPBASE $BUILDDIR/Anklang-x86_64.sqfs \
	   -root-owned -noappend -mkfs-time 0 \
	   -no-exports -no-recovery -noI \
	   -always-use-fragments -b 1048576 \
	   -comp zstd -Xcompression-level 22
cat $APPTOOLS/appimage-runtime-zstd $BUILDDIR/Anklang-x86_64.sqfs > assets/anklang-$VERSION-x64.AppImage
chmod +x assets/anklang-$VERSION-x64.AppImage

# done
ls -l -h assets/anklang-$VERSION-x64.AppImage

