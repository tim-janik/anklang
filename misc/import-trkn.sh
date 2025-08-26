#!/usr/bin/env bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail && SCRIPTNAME=${0##*/} && die() { [ -z "$*" ] || echo "$SCRIPTNAME: $*" >&2; exit 127 ; }
SCRIPTPATH="$(readlink -f "$0")" && SCRIPTDIR=${SCRIPTPATH%/*}

# === Versions ===
# tracktion-engine 3.0 + build fixes
TRACKTION_HEAD="034fdde4aa5a5d3e7c8f546ce7f0541dc5595296"
# JUCE, last GPL-3.0 version
JUCE_HEAD="7.0.12"
# name of pristine vendor branch
BRANCH="trkn"

# === Options ===
# Usage: $0 [-x]
KEEP=
while test $# -ne 0 ; do
  case "$1" in \
    -x) set -x ;;
    -k) KEEP=true ;;
    -*) die "unknown option: $1" ;;
    *)  die "unknown option: $1" ;;
  esac
  shift
done
TDIR=/tmp/import-trkn`id -u`/
MAKEJ="make -j`nproc`"

# === Helpers ===
e() { Q1="$1"; shift; QR="$*"; QOUT=$(printf '  %-8s ' "$Q1" ; echo "$QR"); echo "$QOUT"; }

# == Prepare tracktion_engine build ===
e MKDIR $TDIR
test -n "$KEEP" ||
  rm -rf $TDIR
mkdir -p $TDIR/tracktion/
test -w $TDIR || die "failed to write to $TDIR"

# == Fetch ==
e FETCH ~/.cache/tracktion_engine.git ~/.cache/JUCE.git
if grep -sq 'github.com.*Tracktion/tracktion_engine.git' ~/.cache/tracktion_engine.git/config ;
then	test -n "$KEEP" || git --git-dir ~/.cache/tracktion_engine.git fetch ;
else	git clone --bare git@github.com:Tracktion/tracktion_engine.git ~/.cache/tracktion_engine.git ;
fi
if grep -sq 'github.com.*juce-framework/JUCE.git' ~/.cache/JUCE.git/config ;
then	test -n "$KEEP" || git --git-dir ~/.cache/JUCE.git fetch ;
else	git clone --bare git@github.com:juce-framework/JUCE.git ~/.cache/JUCE.git ;
fi

# == Checkout ==
e CHECKOUT $TDIR/tracktion/
git --work-tree=$TDIR/tracktion -c advice.detachedHead=false \
    --git-dir ~/.cache/tracktion_engine.git checkout -f $TRACKTION_HEAD
git --work-tree=$TDIR/tracktion/modules/juce -c advice.detachedHead=false \
    --git-dir ~/.cache/JUCE.git checkout -f $JUCE_HEAD

# == Build DemoRunner ==
e BUILD DemoRunner in $TDIR/tracktion/build
test -d $TDIR/tracktion/build || (
  cd $TDIR/tracktion
  cmake -B build -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
)
e BUILD $TDIR/tracktion/build
test -r $TDIR/tracktion/build/compile_commands.json || (
  cd $TDIR/tracktion/build
  nice bear -- $MAKEJ DemoRunner
)
e EXTRACT defs via $SCRIPTDIR/ccj2make.js
( cd $TDIR/tracktion
  $SCRIPTDIR/ccj2make.js build/compile_commands.json > trkn.g.mk
)

# == Build engine sources ==
e BUILD cxxtest with tracktion_engine
cat > $TDIR/tracktion/cxxtest.mk <<-'__EOF'
	include trkn.g.mk
	TRKN_DEFS := -I modules -I modules/juce/modules $(TRACKTION_EXTERNAL_INCLUDES)
	TRKN_DEFS += $(patsubst -Ijuce_%, -I modules/juce/modules/juce_%, $(TRACKTION_INTERNAL_INCLUDES))
	TRKN_DEFS += $(TRACKTION_HEADER_DEFINES) $(TRACKTION_CCBODY_DEFINES)
	TRKN_DEFS += -DJUCE_INCLUDE_OGGVORBIS_CODE=0
	TRKN_DEFS += -DJUCE_INCLUDE_PNGLIB_CODE=0 -DJUCE_INCLUDE_JPEGLIB_CODE=0
	TRKN_DEFS += -DJUCE_INCLUDE_ZLIB_CODE=0 -DJUCE_ZLIB_INCLUDE_PATH='<zlib.h>'
	SOURCES  := $(TRACKTION_SOURCES:%=modules/%) $(JUCE_SOURCES:%=modules/juce/modules/%)
	OBJECTS  := $(SOURCES:%.cpp=%.o)
	CXX      := ccache clang++
	CXXFLAGS := -std=gnu++20 -pthread -pipe -march=sandybridge # -march=native
	%.o: %.cpp ; $(CXX) $(CXXFLAGS) $(TRKN_DEFS) -c $< -o $@
	cxxtest: $(OBJECTS) ; rm $(OBJECTS)
__EOF
# disable rpmalloc, tests
( e DISABLE rpmalloc use
  cd $TDIR/tracktion/modules
  sed -r '/^#include.*rpmalloc/s|^|//|' -i tracktion_graph/tracktion_graph.cpp
)
( e PRUNE Tracktion_Engine-tests
  cd $TDIR/tracktion/modules	# cd "$TDIR/trkn-import"/
  find tracktion_*/ -name \*.test.cpp -exec rm -v {} +
  grep -P '^#include.*\.test\.cpp' -r -l tracktion_*/ |
    xargs sed -r '/^#include.*\.test\.cpp/s|^#|//#|' -i
  # fix dependency on tracktion_AudioFile.test.cpp including tracktion_graph.h
  sed -r 's|(//#include *"audio_files/tracktion_AudioFile.test.cpp")|#include <tracktion_graph/tracktion_graph.h> \1|' \
      -i tracktion_engine/tracktion_engine_audio_files.cpp
)
# move file access into past
find $TDIR/tracktion/ -print0 | xargs -0 touch -mat 199901110101
# Compile, CXX accesses what is interesting for us
$MAKEJ -C $TDIR/tracktion/ -f cxxtest.mk cxxtest
( cd $TDIR/tracktion/modules/juce/modules/juce_dsp/native
  # include AVX SSE NEON
  cat *.cpp *.h >/dev/null
)

# == Import preparation ==
( e COPY tracktion_engine sources
  rm -rf "$TDIR/trkn-import"/
  mkdir "$TDIR/trkn-import"/
  cd $TDIR/tracktion/modules
  find tracktion_*/ 3rd_party/ -type f -atime -365 -print0 |
    xargs -0 cp --parents -t "$TDIR/trkn-import"/
  find tracktion_*/ -type f -name '*.md' '!' -path '*3rd_party*' -print0 |
    xargs -0 cp --parents -t "$TDIR/trkn-import"/
)
( e COPY juce sources
  cd $TDIR/tracktion/modules/juce/modules
  find juce_*/ -type f -atime -365 -print0 |
    xargs -0 cp --parents -t "$TDIR/trkn-import"/
)
( e PRUNE FLAC-copy
  cd "$TDIR/trkn-import"/juce_audio_formats/codecs/
  rm -r flac
)
( e INCLUDE external/choc
  cd "$TDIR/trkn-import"/3rd_party/choc/
  rm */*
  for f in audio/choc_MIDI.h audio/choc_MIDISequence.h audio/choc_SampleBuffers.h \
    containers/choc_FIFOReadWritePosition.h containers/choc_MultipleReaderMultipleWriterFIFO.h containers/choc_NonAllocatingStableSort.h \
    containers/choc_SingleReaderMultipleWriterFIFO.h containers/choc_SingleReaderSingleWriterFIFO.h containers/choc_Span.h \
    platform/choc_Assert.h platform/choc_DisableAllWarnings.h platform/choc_ReenableAllWarnings.h \
    text/choc_OpenSourceLicenseList.h text/choc_StringUtilities.h threading/choc_SpinLock.h ; do
    echo "#include \"choc/$f\"" > $f
  done
)
( e INCLUDE external/crill
 cd "$TDIR/trkn-import"/3rd_party/crill/
  echo '#include "crill/include/crill/seqlock_object.h"' > seqlock_object.h
)
( e INCLUDE external/magic_enum
  cd "$TDIR/trkn-import"/3rd_party/magic_enum/
  rm magic_enum*
  echo '#include "magic_enum/include/magic_enum/magic_enum.hpp"' > magic_enum.hpp
  echo '#include "magic_enum/include/magic_enum/magic_enum_utility.hpp"' > magic_enum_utility.hpp
)
( e INCLUDE external/libsamplerate
  cd "$TDIR/trkn-import"/3rd_party/libsamplerate/
  rm *
  echo '#include "libsamplerate/include/samplerate.h"' > samplerate.h
  for f in samplerate.c src_linear.c src_sinc.c src_zoh.c ; do
    echo "#include \"libsamplerate/src/$f\"" > $f
  done
)
( e INCLUDE external/Expected
  cd "$TDIR/trkn-import"/3rd_party/expected/
  echo '#include "expected/include/tl/expected.hpp"' > expected.hpp
)
( e INCLUDE external/nanorange
  cd "$TDIR/trkn-import"/3rd_party/nanorange/
  echo '#include "nanorange/single_include/nanorange.hpp"' > nanorange.hpp
)
( e INCLUDE external/mpmcqueue
  cd "$TDIR/trkn-import"/3rd_party/rigtorp
  echo '#include "mpmcqueue/include/rigtorp/MPMCQueue.h"' > MPMCQueue.h
)
( e INCLUDE external/soundtouch
  cd "$TDIR/trkn-import"/tracktion_engine/3rd_party/soundtouch/include/
  rm *
  for f in BPMDetect.h FIFOSampleBuffer.h FIFOSamplePipe.h soundtouch_config.h SoundTouch.h STTypes.h ; do
    echo "#include \"soundtouch/include/$f\"" > $f
  done
  cd ../source/SoundTouch/
  rm *
  for f in BPMDetect.cpp PeakFinder.cpp FIFOSampleBuffer.cpp AAFilter.cpp cpu_detect_x86.cpp \
	   FIRFilter.cpp InterpolateCubic.cpp InterpolateLinear.cpp InterpolateShannon.cpp \
	   mmx_optimized.cpp RateTransposer.cpp SoundTouch.cpp sse_optimized.cpp TDStretch.cpp ; do
    echo "#include \"soundtouch/source/SoundTouch/$f\"" > $f
  done
)
( e COPY trkn.g.mk
  cp $TDIR/tracktion/trkn.g.mk "$TDIR/trkn-import"/
)
( e CLEANUP CRLF
  cd "$TDIR/trkn-import"/
  find . -type f -exec sed -i 's/\r$//' {} +
)

# == Import README ==
e GEN trkn/README.md
TRACKTION_VERSION=tracktion-engine-$(git --git-dir ~/.cache/tracktion_engine.git describe --tags --long $TRACKTION_HEAD)
JUCE_VERSION=JUCE-$(git --git-dir ~/.cache/JUCE.git describe --tags --long $JUCE_HEAD)
cat > "$TDIR/trkn-import"/README.md <<-__EOF
	# Tracktion Engine Import

	This directory contains the core sources of Tracktion Engine
	(without examples), JUCE and bundled dependencies.

	  $TRACKTION_VERSION
	  $JUCE_VERSION

	# COPYRIGHT & LICENSE

	Effective license is the GNU GPL-3.0, for details refer to:
	  $BRANCH/copyright
__EOF

# == git-vendor-replay ==
e IMPORT Run git-vendor-replay trkn >/dev/null
(
  TDATE=$(git --git-dir ~/.cache/tracktion_engine.git log -1 --no-show-signature --pretty=%cd --date='format:%Y-%m-%d %H:%M:%S' $TRACKTION_HEAD)
  export GIT_AUTHOR_DATE="$TDATE" GIT_AUTHOR_NAME="Tracktion Import" GIT_AUTHOR_EMAIL="281887+tracktion@users.noreply.github.com"
  git-vendor-replay --rebase trkn "$TDIR/trkn-import"/ -b $BRANCH -t "Vendor Import of $TRACKTION_VERSION $JUCE_VERSION"
)
test -n "$KEEP" || rm -rf $TDIR/tracktion
