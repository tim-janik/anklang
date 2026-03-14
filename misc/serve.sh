#!/usr/bin/env bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail

test "${1-}" == -x && { shift; set -x; }

# Dir to find Anklang, run mkdocs, used by vite.config.ts
export BUILDDIR=out/
# Note1: For development, TCP ports are hard coded, keep in sync with vite.config.ts
export DEVPORT_ANKLANG=1776
export DEVPORT_VITE=1777
export DEVPORT_MKDOCS=1778

# Signals to relay to children or process group
SIGNALS="HUP INT QUIT ILL TRAP ABRT BUS SEGV USR2 PIPE TERM SYS"

# Upon exit, kill process group, so all (grand) children are cleaned up
pglead=$(ps -o pgid= -p "$$")		# Process group leader, e.g. $$ or Make
if test $$ -eq $pglead ; then		# Kill child processes *and* group
  trap "trap - TERM EXIT; kill -TERM -- \$(jobs -p) -$pglead" $SIGNALS ERR EXIT
else					# Kill child processes upon exit
  trap "trap - TERM EXIT; kill -TERM -- \$(jobs -p)" $SIGNALS ERR EXIT
fi

# Note2: Anklang uses an authentication token, stored under $HOME to restrict WebUI access
# to one user. For development, we could read the ~/.../anklang*.html redirect, fetch the
# auth URL and copy the set-cookie reply into the vite server headers.
# It is just pointless as it still leaves vite open, so we just use --unauth-dev instead.

# Anklang
echo -e "\n+ $BUILDDIR/lib/AnklangSynthEngine --unauth-dev=$DEVPORT_ANKLANG $*" >&2
$BUILDDIR/lib/AnklangSynthEngine --unauth-dev=$DEVPORT_ANKLANG "$@" &	# does setpgid()
sleep 0.5

# Vite
echo -e "\n+ node_modules/.bin/vite --strictPort --host localhost --port $DEVPORT_VITE -c vite.config.ts -l info" >&2
node_modules/.bin/vite --strictPort --host localhost --port $DEVPORT_VITE -c vite.config.ts -l info &
sleep 0.5

# MkDocs
cd $BUILDDIR/mkdocs/
echo -e "\n+ uv run mkdocs serve -a localhost:$DEVPORT_MKDOCS" >&2
uv run mkdocs serve --livereload -a localhost:$DEVPORT_MKDOCS &
cd - 2>/dev/null
#sleep 1

# Wait until first child exits
wait -n
