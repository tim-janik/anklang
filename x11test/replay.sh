#!/usr/bin/env bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail #-x

SCRIPTNAME="$0" ; die() { [ -z "$*" ] || echo "${SCRIPTNAME##*/}: $*" >&2; exit 128 ; }

X11TEST="${SCRIPTNAME%/*}"

# parse CLI
OPTS=-w
while [[ "${1:-}" = -* ]] ; do
  case "$1" in
    -v)		OPTS=-v ;;
    -w)		OPTS=-w ;;
    -h)		cat <<-_EOF
	Usage: replay.sh [OPTIONS] <play.json>
	Replay a DevTools recording in Electron via Puppetteer
	with AnklangSynthEngine running as server.
	  -h		Display brief usage
	  -v		Use virtual X11 server for headless recording
	  -w		Use nested X11 window as server
_EOF
		exit 0 ;;
    *)		die "unknown option: $1"
  esac
  shift
done
[[ "${1:-}," = ?*.json, ]] || die 'missing <play.json>'
JSONFILE=$(readlink -f "$1")
ONAME=$PWD/$(basename "${JSONFILE%.json}")

# kill all child processes at exit
trap 'pkill -P $$ || true'	0 HUP INT QUIT TRAP USR1 PIPE TERM ERR EXIT

# change dir to project build dir which has node_modules/
# find node_modules/ relative to SCRIPTNAME
export NODE_PATH=$X11TEST/../out/node_modules/
cp $X11TEST/ereplay.cjs .

# Replay in ELectron and record X11 session
EXITSTATUS=0
$X11TEST/x11rec.sh \
  -o $ONAME $OPTS \
  $NODE_PATH/.bin/electron --disable-gpu --disable-dev-shm-usage --no-sandbox --enable-logging \
  ./ereplay.cjs x11rec.sh $JSONFILE ||
  EXITSTATUS=$?

# preserve exist status
echo exit $EXITSTATUS
exit $EXITSTATUS
