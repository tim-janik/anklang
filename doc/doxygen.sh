#!/usr/bin/env bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail && SCRIPTNAME=${0##*/} && die() { [ -z "$*" ] || echo "$SCRIPTNAME: $*" >&2; exit 127 ; }
SCRIPTPATH="$(readlink -f "$0")" && SCRIPTDIR=${SCRIPTPATH%/*}

# == Options, Setup ==
BUILDDIR=$(readlink -f "${BUILDDIR:-out}")
DOXYDIR="$BUILDDIR"/doxygen/
QUIET=false
WITH_ASE=false
while test $# -ne 0 ; do
  case "$1" in
    --quiet)		QUIET=true ;;
    --ase)		WITH_ASE=true ;;
    *)                  die "unknown argument: '$1'" ;;
  esac
  shift
done

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
JAVADOC_AUTOBRIEF = YES
QT_AUTOBRIEF = YES
FORCE_LOCAL_INCLUDES = YES
INLINE_INFO = NO
XML_PROGRAMLISTING = NO
MAX_INITIALIZER_LINES  = 0
GENERATE_LATEX = NO

# ALPHABETICAL_INDEX = NO
# SHOW_USED_FILES = NO

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

PREDEFINED =	DOXYGEN ASE_ALWAYS_INLINE= ASE_CONST=
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
  FIND_ARGS=( -name '*.h*' )
  test "${1-}" == +cc && {
    shift
    FIND_ARGS+=( -o -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.tcc' )
  }
  cd "$1"
  mapfile -d $'\0' -t SOURCES < <(find . -type f \( "${FIND_ARGS[@]}" \) -print0)
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
# Run doxygen in $DOXYDIR/doxy, using $DOXYDIR/Doxyfile
run_doxygen() # run_doxygen NAME NUMBER BRIEF OUTDIR
(
  cd $DOXYDIR/doxy
  cp ../Doxyfile .
  INPUTS= HAVE_DOT=NO SRC=NO REF=NO
  while test $# -ne 0 ; do
    case "$1" in \
      +cc)	INPUTS=+cc ; SRC=YES ; shift ;;
      +ref)	REF=YES ; shift ;;
      +dot)	HAVE_DOT=YES ; shift ;;
      *)        break ;;
    esac
  done
  echo "PROJECT_NAME = \"$1\""		>> Doxyfile
  echo "PROJECT_NUMBER = \"$2\""	>> Doxyfile
  echo "PROJECT_BRIEF = \"$3\""		>> Doxyfile
  echo "HAVE_DOT = $HAVE_DOT"		>> Doxyfile
  echo "VERBATIM_HEADERS = YES"		>> Doxyfile
  echo "SOURCE_BROWSER = $SRC"		>> Doxyfile
  echo "INLINE_SOURCES = NO"		>> Doxyfile
  echo "REFERENCED_BY_RELATION = $REF"	>> Doxyfile
  echo "REFERENCES_RELATION = $REF"	>> Doxyfile
  echo "REFERENCES_LINK_SOURCE = NO"	>> Doxyfile
  mark_doxygen_inputs $INPUTS .
  OUTPUT=$(readlink -f "$4")
  rm -rf html/
  doxygen
  mkdir -p "$OUTPUT" && rm -rf "$OUTPUT" # leaves parent dir
  replace_backlink html/
  mv -v html "$OUTPUT"
)

# == Ase Docs ==
if $WITH_ASE ; then
  rm -rf $DOXYDIR/doxy && mkdir -p $DOXYDIR/doxy
  cp -a ase devices jsonipc $DOXYDIR/doxy/
  ASE_VERSION=anklang-$(misc/version.sh | (read v h d && echo $v))
  ASE_NAME="${ASE_VERSION%-v*}" && ASE_NAME="${ASE_NAME^}"
  ASE_BRIEF="ASE — Anklang Sound Engine (C++)"
  run_doxygen +dot +ref +cc "$ASE_NAME" "${ASE_VERSION#*-v}" "$ASE_BRIEF" $DOXYDIR/ase/
fi
test -r "$DOXYDIR/ase/tagfile.xml" &&
  echo "TAGFILES += \"$DOXYDIR/ase/tagfile.xml=../ase/\"" >> $DOXYDIR/Doxyfile

# == Cleanup ==
rm -rf $DOXYDIR/doxy
