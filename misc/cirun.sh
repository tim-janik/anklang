#!/bin/bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail && SCRIPTNAME=${0##*/} && die() { [ -z "$*" ] || echo "$SCRIPTNAME: $*" >&2; exit 127 ; }

# Chdir to project root
SCRIPTDIR="${0%/*}"
cd $SCRIPTDIR/..
test -r ase/api.hh || die "cd: failed to change to project root"

help() {
  echo "Usage: $0 [-h] [-x] [--assets] [--build] [--check] [--clang-tidy] [--pdf] [--poxy] [--poxy-key SshId] [--x11] [V=1]"
  exit 0
}

# Parse OPTIONS
CITAG="${CITAG:-cifocal:latest}"
WITH_BUILD=false
WITH_CHECK=false
WITH_CLANG_TIDY=false
WITH_MKASSETS=false
WITH_X11TEST=false
WITH_POXY=false
NEEDPDF=
SSH_IDENTITY=
test $# -eq 0 && help
while test $# -ne 0 ; do
  case "$1" in \
    -x)			set -x ;;
    --asset*)		WITH_MKASSETS=true ;;
    --build)		WITH_BUILD=true ;;
    --check)		WITH_BUILD=true ; WITH_CHECK=true ;;
    --clang-tidy)	WITH_CLANG_TIDY=true ;;
    --pdf)		NEEDPDF=true ;;
    --poxy)		WITH_POXY=true ;;
    --poxy-key)		WITH_POXY=true ; shift ; SSH_IDENTITY="$1" ;;
    --x11)		WITH_X11TEST=true ;;
    V=*)		V="${1#V=}" ;;
    -h|--help)		help ;;
  esac
  shift
done
$WITH_POXY && NEEDPDF=true
V="${V:-}"

# Properly fetch all of the repo history
test -r `git rev-parse --git-dir`/shallow &&
  git fetch --unshallow
# Re-fetch all tags, because actions/checkout messes up annotation of the fetched tag: actions/checkout#290
git fetch -f --tags
git describe --tags --match='v[0-9]*.[0-9]*.[0-9]*' --exact-match 2>/dev/null || git describe
misc/version.sh

# Build Docker image
# docker build -t $CITAG -f misc/Dockerfile.CIDIST misc

# RUN - setup alias `docker run`...
tty -s && INTERACTIVE=-i || INTERACTIVE=
RUN="docker run $INTERACTIVE -t --rm -v $PWD:/anklang -w /anklang $CITAG"
$RUN c++ --version

# Provide ssh key file for docu upload to https://tim-janik.github.io/docs/anklang/
test -z "$SSH_IDENTITY" || (
  umask 0077
  cp "$SSH_IDENTITY" .git/.ssh_id_ghdocs4anklang	# used by doc/poxy.sh
)

# Configure Build, move file modifications here, before running chown
cat <<__EOF		>  config-defaults.mk
prefix=/
CC=clang
CXX=clang++
CLANG_TIDY=clang-tidy
__EOF
cat config-defaults.mk

# Adjust file permissions, needed because the caller of this script might have $UID != 1000
$RUN sudo chown 1000:1000 -R /anklang/
trap "$RUN sudo chown `id -u`:`id -g` -R /anklang/" 0 HUP INT QUIT TRAP USR1 PIPE TERM ERR EXIT

# Show repo in docker
$RUN misc/version.sh

# Build binaries, docs and check
$WITH_BUILD && $RUN make V="$V" -j`nproc` all ${NEEDPDF:+ pdf }
$WITH_CHECK && $RUN make V="$V" check

# Artifact upload from make install
test -z "$NEEDPDF" || {
  $RUN make V="$V" -j`nproc` pdf
  $RUN make V="$V" install DESTDIR=inst
  $RUN cp -a inst/share/doc/anklang/ out/anklang-docs/
}
# actions/upload-artifact: { name: docs, path: out/anklang-docs/ }

# Build binaries and packages
$WITH_MKASSETS && $RUN misc/mkassets.sh
# actions/upload-artifact: { name: assets, path: assets/ }

# Build and upload API docs
$WITH_POXY && {
  test -r .git/.ssh_id_ghdocs4anklang && POXY_UPLOAD=-u || POXY_UPLOAD=
  test -r html/poxy/poxy.css || $RUN doc/poxy.sh -b $POXY_UPLOAD
}

# X11 GUI tests, run and record
$WITH_X11TEST && $RUN make V="$V" x11test-v
# actions/upload-artifact@v3 { name: x11test, path: out/x11test/ }

# Create clang-tidy logs
$WITH_CLANG_TIDY && $RUN make V="$V" -j`nproc` clang-tidy
# actions/upload-artifact@v3 { name: clang-tidy, path: out/clang-tidy/ }

# Clear exit code
exit 0
