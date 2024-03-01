#!/usr/bin/env bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail #-x

SCRIPTNAME=${0##*/} ; die() { [ -z "$*" ] || echo "$SCRIPTNAME: $*" >&2; exit 128 ; }

# Must run in anklang/ root
test -e ase/api.hh || die "must run in anklang/"
test -e out/doc/anklang-manual.html || die "a fully build project is required"

# Find node_modules
test -d node_modules/jsdoc-api || die "a fully build project is required"
export NODE_PATH=node_modules/

# Parse args
VERSION=(`misc/version.sh`)
BUILD=false
SERVE=false
UPLOAD=false
while test $# -ne 0 ; do
  case "$1" in \
    -b)         BUILD=true ;;
    -s)         SERVE=true ;;
    -u)         UPLOAD=true ;;
    -h|--help)  echo "Usage: $0 [-b] [-h] [-s] [-u]"; exit 0 ;;
  esac
  shift
done

# Build docs with poxy et al
poxy_build()
{
  # Poxy config: https://github.com/marzer/poxy/wiki/Configuration-options
  cat <<-__EOF >poxy.toml
name            = 'Anklang ${VERSION[0]}'
author          = 'Tim Janik'
description     = 'Anklang Programming Reference'
cpp             = 17
github          = 'tim-janik/anklang'
license         = [ 'MPL-2.0', 'https://mozilla.org/MPL/2.0' ]
show_includes   = false
internal_docs   = true
changelog       = false
favicon         = 'ui/assets/favicon.svg'
logo            = 'ui/assets/favicon.svg'
navbar          = 'all'	# [ 'namespaces', 'annotated' ]
theme           = 'light'
stylesheets	= [ 'doc/poxystyle.css' ]

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

  # MAIN page
  sed '0,/^<!--\s*HEADING\s*-->/d' README.md	> index.md
  sed '1 s/^ANKLANG$'/"Anklang ${VERSION[1]}"/ -i index.md

  # Add NEWS.md
  sed '1 s/^/# Anklang Release NEWS\n\n/' NEWS.md > poxy/NEWS.md

  # Add C++ files
  for f in ase/*.hh ; do
    grep -Fq ' @file ' "$f" ||
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
    grep '^\s*///\|/\*[!*] [^=]' -q "$f" || continue
    sed ' 0,/^<script\b/s/.*//; /^<\/script> *$/{d;q}; ' < "$f" > "$f".js
  done

  # Extract JS docs
  make node_modules/.npm.done
  MARKDOWN_FLAVOUR="-f markdown+compact_definition_lists+autolink_bare_uris+emoji+lists_without_preceding_blankline-smart-raw_html"
  HTML_FLAGS="--highlight-style doc/highlights.theme --html-q-tags --section-divs --email-obfuscation=references"
  for f in ui/*.js ui/b/*.js ; do
    grep '^\s*///\|/\*[!*] [^=]' -q "$f" || continue
     if test "$(jobs | wc -l)" -ge `nproc`; then
      wait -n
     fi
    {
      echo "Parsing $f..."
      node doc/jsdoc2md.js -d 2				"$f"			> poxy/"$f".js-md
      pandoc $MARKDOWN_FLAVOUR -p -t html5 $HTML_FLAGS	poxy/"$f".js-md		> poxy/"$f".html
      touch poxy/"$f"	# Doxygen needs foo.js to exist, but has no default JS EXTENSION_MAPPING
      echo -e "/** @file $f\n @htmlinclude[block] poxy/$f.html */"			> poxy/"$f".dox
    } & # speed up via parallel execution, synchronize with wait
  done
  wait

  cp ui/ch-*.md poxy/ui/
  cp doc/ch-scripting.md poxy/ui/

  # Generate via Doxygen and poxy and m.css
  poxy # --verbose

  # Fix missing accesskey="f" for Search
  sed -r '0,/<a class="m-doc-search-icon" href="#search" /s/(<a class="m-doc-search-icon")/\1 accesskey="f"/' -i html/*.html
  # Text/border/etc colors are entagled in unfortunate ways, leave colors entirely to poxystyle.css.
  sed -r 's/(^color:| color:)/ --unused-color:/g' -i html/poxy/poxy.css

  # Extend searchdata-v2.js with token list from JS docs
  test -r html/searchdata-v2.js || die 'missing searchdata-v2.js'
  ( cd html/
    echo 'Search.__extra_tokens = ['
    for f in *.html ; do
      grep -qF ' data-4search="' $f || continue
      sed -nr \
          '/\bdata-4search=.*<\/[span]+>/{ s|.* data-4search="([^"]+);([^"]+)" id="([^"]+)">.*|{name:"\1",typeName:"\2",url:"'"$f"'#\3"},|; T SKP; p; :SKP; }' \
	  $f
    done
    echo '];'
  ) >> html/searchdata-v2.js

  # Support searching in __extra_tokens list
  test -r html/search-v2.js || die 'missing search-v2.js'
  cat >> xsearch.in <<-__EOF
	if (results?.length > 0 && Array.isArray (results[0])) {
	  const key = value.toLowerCase().replace (/^\s+/gm, '');
	  for (let xt of Search.__extra_tokens) {
	    let name = xt.name.toLowerCase();
	    if (!name.startsWith (key)) {
	      name = name.replace (/^.*[. :]/gm, '');
	      if (!name.startsWith (key))
	        continue;
	    }
	    results[0].push (Object.assign ({
	      flags: 0,
	      cssClass: "m-default",
	      suffixLength: name.length - key.length,
	    }, xt));
	  }
	}
__EOF
  sed '/let results = this.search(value);/r xsearch.in' -i html/search-v2.js
  rm xsearch.in

  # Add doc/ with manuals
  TMPINST=`pwd`/tmpinst/
  rm -rf $TMPINST
  make DESTDIR=$TMPINST prefix=/ doc/install -j
  cp -ru $TMPINST/share/doc/anklang/. html/
  rm -rf $TMPINST
}

# Conditional build
if $BUILD ; then
  poxy_build
  rm -v ui/b/*.vue.js poxy.toml index.md
else
  test -e html/poxy/poxy.css || die "Failed to detect exiting build in html/"
  echo "  OK  " "Docs exist in html/"
fi

# Serve documentation
$SERVE && {
  set -x
  cd html/
  python3 -m http.server
  exit
}

# Upload docs to https://tim-janik.github.io/docs/anklang/
$UPLOAD && {
  rm -rf poxy/iodocs/
  (
    # Support for CI uploads
    test -z "`git config user.email`" && {
      export GIT_AUTHOR_EMAIL="anklang-team@testbit.eu" GIT_AUTHOR_NAME="Anklang CI Action" # "github.com/tim-janik/anklang/actions/"
      export GIT_COMMITTER_EMAIL="$GIT_AUTHOR_EMAIL" GIT_COMMITTER_NAME="$GIT_AUTHOR_NAME"
      GIT_SSH_COMMAND="ssh -o BatchMode=yes -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no"
      # Add secret for github.com/tim-janik/docs/settings/keys ANKLANG_SSH_KEY
      test -r .git/.ssh_id_ghdocs4anklang && GIT_SSH_COMMAND="$GIT_SSH_COMMAND -i `pwd`/.git/.ssh_id_ghdocs4anklang"
      export GIT_SSH_COMMAND
    }
    # Clone docs/ repo and upload latest build
    git clone --depth 1 git@github.com:tim-janik/docs.git --branch publish poxy/iodocs/
    rm -rf poxy/iodocs/anklang/
    cp -r html/ poxy/iodocs/anklang/
    cd poxy/iodocs/anklang/
    git add --all
    git commit --all -m "Anklang Programming Reference - ${VERSION[1]}"
    git push
  )
  exit
}

exit 0
