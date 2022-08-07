#!/bin/bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail

SCRIPTNAME=${0##*/} ; die() { [ -z "$*" ] || echo "$SCRIPTNAME: $*" >&2; exit 128 ; }

# == defaults ==
PROJECT=anklang
DOCKERFILE=misc/Dockerfile-apt
DIST=ubuntu:20.04
DOCKEROPTIONS="--cap-add SYS_PTRACE"
EXEC_CMD=
INITIALIZE=false
OOTBUILD_ARGS=
NOCACHE=
NO_TEX=
IMGTAG=$PROJECT-dbuild

echo DEBUGGING:
set -x ; PS4='> '
echo INITIALIZE=$INITIALIZE

docker images | grep -q "^$IMGTAG " || INITIALIZE=true

echo IMGTAG="$IMGTAG"
echo grep IMGTAG:
docker images | grep "^$IMGTAG " && echo status=$? || echo status=$?
echo 'grep -q' IMGTAG:
docker images | grep -q "^$IMGTAG " && echo status=$? || echo status=$?
echo INITIALIZE=$INITIALIZE
echo Aborting... ; exit -7

# == usage ==
usage() {
  #     12345678911234567892123456789312345678941234567895123456789612345678971234567898
  echo "Usage: $0 [OPTIONS] [--] [COMMAND...]"
  echo "Run COMMAND in $PWD inside docker environment."
  echo "  -d <DIST>         Set DIST build arg [$DIST]"
  echo "  -f <DOCKERFILE>   Choose a Dockerfile [$DOCKERFILE]"
  echo "  -h                Display command line help"
  echo "  -i                Initialize docker build environment [$INITIALIZE]"
  echo "  -o <directory>    Mount <directory> as /ootbuild (\$OOTBUILD)"
  echo "  --no-cache        Build docker with --no-cache"
  echo "  --no-tex          Build with \$NO_TEX=true"
  echo "  shell             COMMAND: Run shell"
  echo "  root              COMMAND: Run root shell"
}

# == parse args ==
while test $# -ne 0 ; do
  case "$1" in \
    -d)		shift; DIST="$1" ;;
    -f)		shift; DOCKERFILE="$1" ;;
    -h|--help)	usage ; exit 0 ;;
    -i)		INITIALIZE=true ;;
    -o)		shift
		OOTBUILD_ARGS="-v `realpath $1`:/ootbuild/ -e CCACHE_DIR=/ootbuild/.dlcache/ccache"
		;;
    --no-cache)	NOCACHE=--no-cache ;;
    --no-tex)	NO_TEX=true ;;
    --)		shift ; break ;;
    *)		break ;;
  esac
  shift
done
test $# -ne 0 || { usage ; exit 0 ; }
EXEC_CMD="$1" && shift

# == Detect User Settings ==
TUID=`id -u`
TGID=`id -g`
PROJECT_VOLUME="--user $TUID:$TGID -v `pwd`:/$PROJECT/"
export PODMAN_USERNS=keep-id # keep UID of files in volume mounts under podman

# == Initialize Build Environment ==
$INITIALIZE && {
  mkdir -p misc/.dbuild/.cache/anklang/
  test ! -d ~/.cache/electron/. || cp --reflink=auto --preserve=timestamps ~/.cache/electron -r misc/.dbuild/.cache/
  test ! -d ~/.cache/anklang/downloads/. || cp --reflink=auto --preserve=timestamps ~/.cache/anklang/downloads -r misc/.dbuild/.cache/anklang/
  ( set -x
    docker build -f "$DOCKERFILE" --build-arg DIST="$DIST" --build-arg NO_TEX="$NO_TEX" --build-arg USERGROUP="$TUID:$TGID" -t $IMGTAG $NOCACHE misc/
  )
  rm -f -r misc/.dbuild/
}

# == Keep interactive tty ==
tty >/dev/null && TI=-ti || TI=-t

# == Copy CWD to temporary volume ==
setup_ROOT_TMPVOLUME() {
  test -n "${TMPVOLUME:-}" || {
    TMPVOLUME=$PROJECT-tmpvol
    trap "docker volume rm $TMPVOLUME >/dev/null" 0 HUP INT QUIT TRAP USR1 PIPE TERM ERR EXIT
    docker run $DOCKEROPTIONS -v `pwd`:/pwd_rovolume/:ro -v $TMPVOLUME:/$PROJECT/ $TI --rm $IMGTAG \
	   cp -a --reflink=auto /pwd_rovolume/. /$PROJECT/
  }
  VOLUME_ARGS="-v $TMPVOLUME:/$PROJECT/"
}

# == EXEC_CMD ==
VOLUME_ARGS="$PROJECT_VOLUME"
test root != "$EXEC_CMD" || setup_ROOT_TMPVOLUME # let root operate on a temporary copy
test root != "$EXEC_CMD" -a shell != "$EXEC_CMD" || EXEC_CMD=/bin/bash
test -z "$EXEC_CMD" || (
  set -x
  docker run $DOCKEROPTIONS $VOLUME_ARGS $OOTBUILD_ARGS $TI --rm -w /$PROJECT/ $IMGTAG "$EXEC_CMD" "$@"
) # avoid 'exec' for TRAP(1) to take effect
exit $?
