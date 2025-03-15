#!/usr/bin/env bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail

# Dir to find Anklang, run mkdocs, used by ui/vite.config.ts
export BUILDDIR=out/

# Signals to relay to children or process group
SIGNALS="HUP INT QUIT ILL TRAP ABRT BUS SEGV USR2 PIPE TERM SYS"

# Upon exit, kill process group, so all (grand) children are cleaned up
pglead=$(ps -o pgid= -p "$$")		# Process group leader, e.g. $$ or Make
if test $$ -eq $pglead ; then		# Kill child processes *and* group
  trap "trap - TERM EXIT; kill -TERM -- \$(jobs -p) -$pglead" $SIGNALS ERR EXIT
else					# Kill child processes upon exit
  trap "trap - TERM EXIT; kill -TERM -- \$(jobs -p)" $SIGNALS ERR EXIT
fi

# Note1: For development, TCP ports are hard coded, keep in sync with vite.config.ts

# Note2: Anklang uses an authentication token, stored under $HOME to restrict WebUI access
# to one user. For development, we could read the ~/.../anklang*.html redirect, fetch the
# auth URL and copy the set-cookie reply into the vite server headers.
# It is just pointless as it still leaves vite open, so we just use --no-auth instead.

# Anklang
echo -e "\n+ $BUILDDIR/lib/AnklangSynthEngine --unauth-dev=1776 $*" >&2
$BUILDDIR/lib/AnklangSynthEngine --unauth-dev=1776 "$@" &	# does setpgid()
sleep 0.5

# Vite
echo -e "\n+ node_modules/.bin/vite --strictPort --host localhost --port 1777 -c ui/vite.config.ts -l info" >&2
node_modules/.bin/vite --strictPort --host localhost --port 1777 -c ui/vite.config.ts -l info &
sleep 0.5

# MkDocs
pushd $BUILDDIR >/dev/null
echo -e "\n+ uv run mkdocs serve -a localhost:1778" >&2
uv run mkdocs serve -a localhost:1778 &
popd >/dev/null
#sleep 1

# Wait until first child exits
wait -n
