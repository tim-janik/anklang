#!/usr/bin/env bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail && SCRIPTNAME=${0##*/} && die() { [ -z "$*" ] || echo "$SCRIPTNAME: $*" >&2; exit 127 ; }
SCRIPTPATH="$(readlink -f "$0")" && SCRIPTDIR=${SCRIPTPATH%/*}

# == Options, Setup ==
ASE_VERSION=anklang-$(git describe)
BUILDDIR=$(readlink -f "${BUILDDIR:-out}")
DOXYDIR="$BUILDDIR"/doxygen/
mkdir -p $DOXYDIR/doxy/ && rm -rf $DOXYDIR/*/ # leaves parent dir + files

# == Tagfile ==
( cd $DOXYDIR
  test -r cppreference-doxygen-20250209.tag.xml || {
    wget -c https://github.com/tim-janik/anklang/releases/download/buildassets-v0/cppreference-doxygen-20250209.tag.xml.gz
    rm -f cppreference-doxygen-20250209.tag.xml
    gunzip cppreference-doxygen-20250209.tag.xml.gz
  }
  test -r susv4-doxygen-2013.tag.xml || {
    wget -c https://github.com/tim-janik/anklang/releases/download/buildassets-v0/susv4-doxygen-2013.tag.xml.gz
    rm -f susv4-doxygen-2013.tag.xml
    gunzip susv4-doxygen-2013.tag.xml.gz
    sed 's|\.html</|.html.html</|' -i susv4-doxygen-2013.tag.xml # Doxygen strips mandatory .html extension
  }
)

# == Doxygen Config ==
cat <<-__EOF > $DOXYDIR/Doxyfile
LOOKUP_CACHE_SIZE = 3
NUM_PROC_THREADS = 0
FILE_PATTERNS = *.c *.h *.cc *.hh *.cpp *.hpp *.tcc *.thh *.md *.mm *.js *.dox
RECURSIVE = YES
STRIP_FROM_PATH = $DOXYDIR/doxy
ALLOW_UNICODE_NAMES = YES
VERBATIM_HEADERS = YES
MARKDOWN_SUPPORT = YES
MARKDOWN_ID_STYLE = GITHUB
AUTOLINK_SUPPORT = YES
BUILTIN_STL_SUPPORT = NO
INLINE_SIMPLE_STRUCTS = YES
# EXTRACT_ALL = YES
EXTRACT_ANON_NSPACES =  NO
INTERNAL_DOCS = NO
HIDE_UNDOC_MEMBERS = NO
HIDE_UNDOC_CLASSES = NO
HIDE_FRIEND_COMPOUNDS = NO
HIDE_COMPOUND_REFERENCE = NO
HIDE_UNDOC_RELATIONS = NO
# SHOW_HEADERFILE = NO
# SHOW_INCLUDE_FILES = NO
# SORT_MEMBER_DOCS = NO
# SORT_BRIEF_DOCS = YES
FORCE_LOCAL_INCLUDES = YES
INLINE_INFO = NO
XML_PROGRAMLISTING = NO
MAX_INITIALIZER_LINES  = 0
GENERATE_LATEX = NO

# ALPHABETICAL_INDEX = NO
# SHOW_USED_FILES = NO
SOURCE_BROWSER = NO
INLINE_SOURCES = NO
REFERENCED_BY_RELATION = YES
REFERENCES_RELATION = YES
REFERENCES_LINK_SOURCE = NO

HAVE_DOT = YES
DOT_WRAP_THRESHOLD = 32
DOT_GRAPH_MAX_NODES = 64
DOT_COMMON_ATTR = fontname=Helvetica,fontsize=14
DOT_IMAGE_FORMAT = svg
INTERACTIVE_SVG = YES
CLASS_GRAPH = YES
GRAPHICAL_HIERARCHY = NO
COLLABORATION_GRAPH = NO
GROUP_GRAPHS = NO
INCLUDE_GRAPH = NO
INCLUDED_BY_GRAPH = NO
CALL_GRAPH = NO
CALLER_GRAPH = NO
DIRECTORY_GRAPH = NO
# DIR_GRAPH_MAX_DEPTH = 1
# DOT_GRAPH_MAX_NODES = 50
# MAX_DOT_GRAPH_DEPTH = 0

DISABLE_INDEX = NO
GENERATE_TREEVIEW = NO
FULL_SIDEBAR = NO
HTML_DYNAMIC_SECTIONS = NO
HTML_INDEX_NUM_ENTRIES = 9999
HTML_FORMULA_FORMAT = svg
HTML_COLORSTYLE = TOGGLE
HTML_COLORSTYLE_HUE = 201
HTML_COLORSTYLE_SAT    = 255
HTML_COLORSTYLE_GAMMA  = 120
HTML_EXTRA_STYLESHEET = $PWD/doc/style/doxyextra.css
HTML_HEADER = $PWD/doc/doxyheader.htm
HTML_FOOTER = $PWD/doc/doxyfooter.htm
FORMULA_FONTSIZE = 16

PREDEFINED = ASE_ALWAYS_INLINE= ASE_CONST=
MACRO_EXPANSION = YES
EXCLUDE_SYMBOLS = ASE_CLASS_DECLS ASE_DEFINE_ENUM_EQUALITY ASE_STRUCT_DECLS \
			ASE_CONST ASE_ALWAYS_INLINE __attribute__

GENERATE_TAGFILE = html/tagfile.xml
TAGFILES  = "$DOXYDIR/susv4-doxygen-2013.tag.xml=https://pubs.opengroup.org/onlinepubs/9799919799/"
TAGFILES += "$DOXYDIR/cppreference-doxygen-20250209.tag.xml=https://en.cppreference.com/w/"
__EOF

# == Helper ==
mark_doxygen_inputs()
(
  cd "$1"
  mapfile -d $'\0' -t SOURCES < <(find . -type f -name '*.h*' -print0)
  for file in "${SOURCES[@]}"; do
    sed "1,+0s,^,/** @file $file */ ," -i "$file"
  done
  # find -type d -exec bash -c 'echo -e "/** @dir\n@brief $1 */" >> $1/dir.dox' sh {} \;
)
replace_backlink()
(
  DIR="$1"
  find "$DIR" -type f -print0 |
    xargs -0 sed 's|_project_brief_a_href_backlink_|<a href="../../index.html">« « « Anklang Documentation</a>|' -i
)
run_doxygen()
(
  OUTPUT=$(readlink -f "$4")
  cd $DOXYDIR/doxy
  mark_doxygen_inputs .
  cp ../Doxyfile .
  echo "PROJECT_NAME = \"$1\""   >> Doxyfile
  echo "PROJECT_NUMBER = \"$2\"" >> Doxyfile
  echo "PROJECT_BRIEF = \"$3\""  >> Doxyfile
  rm -rf html/
  doxygen
  mkdir -p "$OUTPUT" && rm -rf "$OUTPUT" # leaves parent dir
  replace_backlink html/
  mv -v html "$OUTPUT"
)

# == Ase Docs ==
rm -rf $DOXYDIR/doxy && mkdir -p $DOXYDIR/doxy
cp -a ase devices jsonipc $DOXYDIR/doxy/
PROJECT_NAME="${ASE_VERSION%-v*}" && PROJECT_NAME="${PROJECT_NAME^}"
PROJECT_NUMBER="${ASE_VERSION#*-v}" # && PROJECT_NUMBER="${PROJECT_NUMBER%%-*}"
PROJECT_BRIEF="ASE — Anklang Sound Engine (C++)"
run_doxygen "$PROJECT_NAME" "$PROJECT_NUMBER" "$PROJECT_BRIEF" $DOXYDIR/ase/
echo "TAGFILES += \"$BUILDDIR/docs/ase/tagfile.xml=../ase/\"" >> $DOXYDIR/Doxyfile
