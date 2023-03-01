#!/bin/bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail #-x

SCRIPTNAME=${0##*/} ; die() { [ -z "$*" ] || echo "$SCRIPTNAME: $*" >&2; exit 128 ; }

# Must run in anklang/ root
test -e ase/api.hh || die "must run in anklang/"

# Parse args
SERVE=false
while test $# -ne 0 ; do
  case "$1" in \
    -s)         SERVE=true ;;
    -h|--help)  echo "Usage: $0 [-s] [-h]"; exit 0 ;;
  esac
  shift
done


# Poxy config: https://github.com/marzer/poxy/wiki/Configuration-options
cat <<-__EOF >poxy.toml
name            = 'Anklang'
author          = 'Tim Janik'
description     = 'Anklang - Programming Reference'
cpp             = 17
github          = 'tim-janik/anklang'
license         = [ 'MPL-2.0', 'https://mozilla.org/MPL/2.0' ]
show_includes   = false
internal_docs   = true
changelog       = false
favicon         = 'ui/assets/favicon.svg'
logo            = 'ui/assets/favicon.svg'
navbar          = 'all'	# [ 'namespaces', 'annotated' ]
theme           = 'dark'

[warnings]
enabled         = true
undocumented    = true
treat_as_errors = false

[sources]
extract_all	= false
patterns        = [ '*.hh', '*.cc', '*.md', '*.js', '*.dox' ]
# paths           = [ 'ase', 'ui', 'ui/b' ]
recursive_paths = 'poxy'
strip_paths     = [ '`pwd`/poxy' ]

[examples]
paths           = '.'

#[images]
#paths           = '.'

[autolinks]
'(?:Ase\.)?server' = 'class_ase_1_1_server.html'
__EOF

# Prepare sources
rm -rf poxy/ html/ && mkdir -p poxy/ase/ poxy/ui/b/

# Add NEWS.md
sed '1 s/^/# Anklang Release NEWS\n\n/' NEWS.md > poxy/NEWS.md

# Add C++ files
for f in ase/*.hh ; do
  fgrep -q ' @file ' "$f" ||
    sed "1s|^|/** @file $f \\\\n */ |" < "$f" > poxy/"$f"
done
cp ase/*.cc poxy/ase/

# Hack around Doxygen generating overlong file name from template
sed -r 's|REQUIRESv< (.*&&)$|REQUIRESv<//\1|' -i poxy/ase/serialize.hh

# Add directories
echo -e "/** @dir\n@brief"	"Anklang Sound Engine API (C++)"	"*/"	> poxy/ase/dir.dox
echo -e "/** @dir\n@brief"	"Anklang UI (Javascript)"		"*/"	> poxy/ui/dir.dox
echo -e "/** @dir\n@brief"	"Anklang Web Components (Javascript)"	"*/"	> poxy/ui/b/dir.dox

# Extract JS from Vue files
for f in ui/b/*.vue ; do
  sed ' 0,/^<script\b/s/.*//; /^<\/script> *$/{d;q}; ' < "$f" > "$f".js
done

# Extract JS docs
make out/node_modules/.npm.done
MARKDOWN_FLAVOUR="-f markdown+autolink_bare_uris+emoji+lists_without_preceding_blankline-smart"
HTML_FLAGS="--highlight-style doc/highlights.theme --html-q-tags --section-divs --email-obfuscation=references"
for f in ui/*.js ui/b/*.js ; do
  grep '^\s*///\|/\*[!*] [^=]' -q "$f" || continue
  echo "Parsing $f..."
  out/node_modules/.bin/jsdoc -c ui/jsdocrc.json -X	"$f"			> poxy/"$f".jsdoc
  node doc/jsdoc2md.js -d 2 -e Export			poxy/"$f".jsdoc		> poxy/"$f".js-md
  pandoc $MARKDOWN_FLAVOUR -p -t html5 $HTML_FLAGS	poxy/"$f".js-md		> poxy/"$f".html
  touch poxy/"$f"	# Doxygen needs foo.js to exist, but has no default JS EXTENSION_MAPPING
  echo -e "/** @file $f\n @htmlinclude[block] poxy/$f.html */"			> poxy/"$f".dox
done
cp ui/b/ch-vue.md poxy/ui/b/
cp doc/ch-scripting.md poxy/ui/

# Generate via Doxygen and poxy and m.css
poxy # --verbose

# Fix missing accesskey="f" for Search
sed -r '0,/<a class="m-doc-search-icon" href="#search" /s/(<a class="m-doc-search-icon")/\1 accesskey="f"/' -i html/*.html

# Serve documentation
$SERVE && {
  set -x
  cd html/
  python3 -m http.server
  exit
}
