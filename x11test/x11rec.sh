#!/usr/bin/env bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail #-x

SCRIPTNAME="$0" ; die() { [ -z "$*" ] || echo "${SCRIPTNAME##*/}: $*" >&2; exit 128 ; }

RES=1920x1080
DEPTH=16
XDISPLAY=87

XSERVER=Xephyr	# Xephyr Xvfb Xnest
ONAME=x11rec

while [[ "${1:-}" = -* ]] ; do
  case "$1" in
    -o)		shift ; ONAME="$1" ;;
    -v)		XSERVER=Xvfb ;;
    -w)		XSERVER=Xephyr ;;
    -h)		cat <<-_EOF
	Usage: x11rec.sh [OPTIONS] <command>
	Run and record <command> in a virtual or nested X11 session.
	  -h		Display brief usage
	  -o filename	Basename for output files
	  -v		Use virtual X11 server for headless recording
	  -w		Use nested X11 window as server
_EOF
		exit 0 ;;
    *)		die "unknown option: $1"
  esac
  shift
done
TEMPTMPL=/tmp/`basename $ONAME`.$$

# kill all child processes
trap 'pkill -P $$ ; rm -f $TEMPTMPL.* '		0 HUP INT QUIT TRAP USR1 PIPE TERM ERR EXIT

# start virtual X11 and get its $DISPLAY
if [[ $XSERVER = Xvfb ]] ; then
  Xvfb -help >/dev/null 2>&1 ||  die "missing executable:" Xvfb
  XTMP=$(mktemp $TEMPTMPL.XXXXXX)
  $XSERVER -displayfd 7 -screen 0 "$RES"x$DEPTH 7>$XTMP 2>/dev/null &
  ATTEMPT=1
  while ! grep -q "[0-9]" $XTMP ; do
    [ "$ATTEMPT" -gt 10 ] && die "started Xvfb: missing DISPLAY"
    echo "${SCRIPTNAME##*/}: $XSERVER: waiting for DISPLAY ($ATTEMPT)"
    sleep 0.5
    ((ATTEMPT = $ATTEMPT + 1))
  done
  XDISPLAY=$(cat $XTMP)
  rm -f $XTMP
elif [[ $XSERVER = Xephyr ]] ; then
  Xephyr -help >/dev/null 2>&1 ||  die "missing executable:" Xephyr
  # 'X11 -displayfd 1 1>display faisl to work with Xnest and Xephyr
  test ! -e /tmp/.X11-unix/X$XDISPLAY || die "starting $XSERVER: DISPLAY exists already: :$XDISPLAY"
  $XSERVER -br -ac -noreset -screen "$RES"x$DEPTH :$XDISPLAY 2>/dev/null &
  sleep 1
  test -e /tmp/.X11-unix/X$XDISPLAY || die "failed to start $XSERVER"
fi

# start video recording
ffmpeg_stdin=$(mktemp $TEMPTMPL.XXXXXX)
echo > $ffmpeg_stdin
tail -f $ffmpeg_stdin |
  ffmpeg -loglevel error -hide_banner \
	 -f x11grab -video_size $RES -i :$XDISPLAY \
	 -codec:v libx264 -preset fast -r 17 -crf 27 -movflags +faststart \
	 -y $ONAME.mp4 &
ffmpeg_pid=$!

# Use twm as simple window manager
grep -s RandomPlacement ~/.twmrc ||
  echo RandomPlacement >> ~/.twmrc	# force auto placement
DISPLAY=:$XDISPLAY twm &
wm_pid=$!

# Start Anklang engine
test -x ../lib/AnklangSynthEngine || die "missing executable:" AnklangSynthEngine
DISPLAY=:$XDISPLAY ../lib/AnklangSynthEngine &
anklang_pid=$!

# run test
EXITSTATUS=0
(
  export DISPLAY=:$XDISPLAY
  set +e -x
  "$@"
) |& tee $ONAME.log ||
  EXITSTATUS=$?

# end video, Anklang and WM
echo q >> $ffmpeg_stdin		# sends ffmpeg exit
test -z "$wm_pid" || kill $wm_pid || :
test -z "$anklang_pid" || kill $anklang_pid || :
wait $ffmpeg_pid || :
rm -f $ffmpeg_stdin

# preserve exist status
#echo exit $EXITSTATUS
exit $EXITSTATUS
