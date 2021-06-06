#!/bin/bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail

SCRIPTNAME=${0##*/} ; die() { [ -z "$*" ] || echo "$SCRIPTNAME: $*" >&2; exit 128 ; }

# == defaults ==
DOCKEROPTIONS="--cap-add SYS_PTRACE"
DOCKERDIST=focal
INITIALIZE=
EXEC_CMD=

# == usage ==
usage() {
  #     12345678911234567892123456789312345678941234567895123456789612345678971234567898
  echo "Usage: $0 [OPTIONS] [--] [COMMAND...]"
  echo "  -f <DIST>         Choose a distribution"
  echo "  -h                Display command line help"
  echo "  -i                Initialize docker build environment"
  echo "  shell             COMMAND: Run shell"
  echo "  root              COMMAND: Run root shell"
}

# == parse args ==
while test $# -ne 0 ; do
  case "$1" in \
    -f)		shift; DOCKERDIST="$1" ;;
    -h)		usage ; exit 0 ;;
    -i)		INITIALIZE=true ;;
    --)		shift ; break ;;
    *)		break ;;
  esac
  shift
done
test $# -ne 0 || { usage ; exit 0 ; }
EXEC_CMD="$1" && shift

# == Detect User Settings ==
DOCKERFILE=misc/Dockerfile-$DOCKERDIST
TUID=`id -u`
TGID=`id -g`
ANKLANG_USER_VOLUME="--user $TUID:$TGID -v `pwd`:/usr/src/anklang/"

# == Initialize Build Environment ==
test -z "$INITIALIZE" || {
  mkdir -p misc/.cicache/electron/
  test ! -d ~/.cache/electron/. || cp --reflink=auto --preserve=timestamps ~/.cache/electron/. -r misc/.cicache/electron/
  ( set -x
    docker build -f "$DOCKERFILE" \
	   --build-arg USERGROUP="$TUID:$TGID" \
	   -t anklang-cbuild misc/
  )
  rm -r misc/.cicache/
}

# == Keep interactive tty ==
tty >/dev/null && TI=-ti || TI=-t

# == Copy CWD to temporary volume ==
setup_ANKLANG_TEMP_VOLUME() {
  test -n "${TMPVOL:-}" || {
    TMPVOL=anklang-cbuild-tmpvol
    trap "docker volume rm $TMPVOL >/dev/null" 0 HUP INT QUIT TRAP USR1 PIPE TERM ERR EXIT
    docker run $DOCKEROPTIONS -v `pwd`:/usr_src_anklang/:ro -v $TMPVOL:/usr/src/anklang/ $TI --rm anklang-cbuild \
	   cp -a --reflink=auto /usr_src_anklang/. /usr/src/anklang/
  }
  ANKLANG_TEMP_VOLUME="-v $TMPVOL:/usr/src/anklang/"
}

# == Shell ==
ANKLANG_TEMP_VOLUME="$ANKLANG_USER_VOLUME"
test root != "$EXEC_CMD" || setup_ANKLANG_TEMP_VOLUME
test root != "$EXEC_CMD" -a shell != "$EXEC_CMD" ||
  EXEC_CMD=/bin/bash
test -z "$EXEC_CMD" || (
  set -x
  docker run $DOCKEROPTIONS $ANKLANG_TEMP_VOLUME $TI --rm anklang-cbuild \
	 "$EXEC_CMD" "$@"
) # avoid 'exec' to carry out the setup_ANKLANG_TEMP_VOLUME trap
exit $?
