#!/usr/bin/env python3
# Dedicated to the Public Domain under the Unlicense: https://unlicense.org/UNLICENSE

import sys, os, re, types, argparse
import subprocess, itertools
import fnmatch, shutil
import subprocess
from pathlib import Path
from datetime import datetime


PROJECT_DICT = {
  'PROJECT_NAME': "Anklang C++ API",
  'PROJECT_NUMBER': "0.0.0",
  'PROJECT_BACKNAV': "Anklang Documentation",
  'PROJECT_FOOTER': "Anklang Documentation",
  'DOCS_URL': "..",
  'INPUT': """ase/ devices/ jsonipc/""",
  'EXCLUDE': "",
}
# use realpath, to find source even if this script is linked to
__file_dir__ = os.path.dirname (os.path.realpath (__file__))
__anklang_dir__ = os.path.realpath (os.path.join (__file_dir__, '..'))

def main():
  parser = argparse.ArgumentParser (description = 'Generate doxygen docs.')
  parser.add_argument ('--quiet', action = 'store_true', help = 'Silence output.')
  parser.add_argument ('outputdir', type = str, help = 'The output directory.')
  args = parser.parse_args()
  config = types.SimpleNamespace()
  config.cwd = __anklang_dir__
  config.quiet = bool (args.quiet)
  config.doxygen_dir = os.path.realpath (args.outputdir)
  make_doxygen (config)

def make_doxygen (config):
  shutil.rmtree (config.doxygen_dir, ignore_errors = True) # clear
  os.makedirs (config.doxygen_dir)
  doxyfile_path = os.path.join (config.doxygen_dir, 'Doxyfile')
  make_doxyfiles (doxyfile_path, config.doxygen_dir)
  exec_doxygen (config, doxyfile_path)
  patch_javascript (config.doxygen_dir)
  patch_all_html (config.doxygen_dir)

def exec_doxygen (config, doxyfile_path):
  try:
    cmd = [ 'doxygen', doxyfile_path ]
    if not config.quiet:
      print ('>', ' '.join (cmd), file = sys.stderr)
    result = subprocess.run (cmd, cwd = config.cwd,
                             capture_output = config.quiet,
                             text = True,
                             check = True) # might raise CalledProcessError
  except subprocess.CalledProcessError as e:
    if e.stderr:
      print (f"{e.stderr.rstrip()}", file = sys.stderr)
    print (f"doxygen.py: failed to run `doxygen`: {e.returncode}", file = sys.stderr)
    sys.exit (e.returncode)
  if not config.quiet or (result.stderr and
                          re.search (r'(warning:|error:)', result.stderr, re.IGNORECASE)):
    if not config.quiet and result.stdout:
      print (result.stdout.rstrip())
    if result.stderr:
      print (f"{result.stderr.rstrip()}", file = sys.stderr)

def patch_all_html (doxygen_dir):
  p, r = r'&#160;</', r'</'     # remove line breaks forced via &nbsp;
  html_dir = os.path.join (doxygen_dir, 'html')
  for filepath in Path (html_dir).glob('*.html'):
    with open (filepath, 'r', encoding = 'utf-8') as htmlfile:
      content = htmlfile.read()
      patched = re.sub (p, r, content)
      if patched == content: continue
      with open (filepath, 'w', encoding = 'utf-8') as htmlfile:
        htmlfile.write (patched)
  p, r = r'(?<=\s)font(-family|-size)?:', r'--font\1:'
  for filepath in Path (html_dir).glob('*.css'):
    if filepath.match ('*doxyextra.css'): continue
    with open (filepath, 'r', encoding = 'utf-8') as cssfile:
      content = cssfile.read()
      patched = re.sub (p, r, content)
      if patched == content: continue
      with open (filepath, 'w', encoding = 'utf-8') as cssfile:
        cssfile.write (patched)

# patch dynsections.js navtreedata.js
def patch_javascript (doxygen_dir):
  dynsections_js = os.path.join (doxygen_dir, 'html', 'dynsections.js')
  with open (dynsections_js, 'a') as f:
    f.write ('\n') # appending
    f.write (JS_ARROW_TOGGLE_FOLDERS)
  navtreedata_js = os.path.join (doxygen_dir, 'html', 'navtreedata.js')
  if os.path.exists (navtreedata_js):
    with open (navtreedata_js, 'r', encoding = 'utf-8') as f:
      content = f.read()
      content = re.sub (r'"index\.html"', '"{DOCS_URL}"'.format (**PROJECT_DICT), content)
      content = content.replace ('"'+PROJECT_DICT['PROJECT_NAME']+'"', '"'+PROJECT_DICT['PROJECT_BACKNAV']+'"')
      with open (navtreedata_js, 'w', encoding = 'utf-8') as f:
        f.write (content)
JS_ARROW_TOGGLE_FOLDERS = r"""
window.addEventListener ('load', event => {
for (let el of document.querySelectorAll ('.arrow[onclick^="dynsection.toggleFolder"]'))
  if (el.textContent.includes ("►"))
    console.log ("toggle:", el, el.click());
});
""";

def make_doxyfiles (doxyfile_path, doxygen_dir):
  PROJECT_DICT['OUTPUT_DIRECTORY'] = doxygen_dir
  with open (doxyfile_path, 'w') as f:
    f.write (doxyfile_settings.format (**PROJECT_DICT))
    f.write (doxyfile_basics.format (**PROJECT_DICT))
    if PROJECT_DICT['PROJECT_NAME']:
      f.write ('PROJECT_NAME = "%s"\n' % PROJECT_DICT['PROJECT_NAME'])
    if PROJECT_DICT['PROJECT_NUMBER']:
      f.write ('PROJECT_NUMBER = "%s"\n' % PROJECT_DICT['PROJECT_NUMBER'])
    f.write ('OUTPUT_DIRECTORY = "%s"\n' % PROJECT_DICT['OUTPUT_DIRECTORY'])
  footer_path = os.path.join (doxygen_dir, 'doxyfooter.html')
  with open (footer_path, 'w') as f:
    f.write (doxyfile_footer.format (**PROJECT_DICT))

doxyfile_footer = r"""
<!-- HTML footer for doxygen 1.13.2-->
<!-- start footer part -->
<!--BEGIN GENERATE_TREEVIEW-->
<div id="nav-path" class="navpath"><!-- id is needed for treeview function! -->
  <ul>
    $navpath
    <center class="footer"><a href="{DOCS_URL}">{PROJECT_FOOTER} </a> {PROJECT_NUMBER} </center>
  </ul>
</div>
<!--END GENERATE_TREEVIEW-->
<!--BEGIN !GENERATE_TREEVIEW-->
<hr class="footer"/><address class="footer"><small>
<center class="footer"><a href="{DOCS_URL}">{PROJECT_FOOTER} </a> {PROJECT_NUMBER} </center>
</small></address>
</div><!-- doc-content -->
<!--END !GENERATE_TREEVIEW-->
</body>
</html>
"""
# doxygen -w html headerFile footerFile styleSheetFile

doxyfile_settings = r"""
EXCLUDE			= {EXCLUDE}
INPUT			= {INPUT}
RECURSIVE		= YES
# PREDEFINED		= ASE_CLASS_DECLS(x)= ASE_CLASS_DECLS(x)= ASE_ALWAYS_INLINE ASE_CONST
PREDEFINED		= ASE_ALWAYS_INLINE= ASE_CONST=
EXPAND_ONLY_PREDEF      = YES
MACRO_EXPANSION        = YES
EXCLUDE_SYMBOLS         = ASE_CLASS_DECLS ASE_DEFINE_ENUM_EQUALITY ASE_STRUCT_DECLS \
        ASE_CONST ASE_ALWAYS_INLINE __attribute__
# HTML_HEADER		= doxy-header.html
HTML_FOOTER		= {OUTPUT_DIRECTORY}/doxyfooter.html
GENERATE_TAGFILE	= "{OUTPUT_DIRECTORY}/html/tagfile.xml"
HTML_EXTRA_STYLESHEET	= doc/style/doxyextra.css
# INPUT_FILTER            = sed '1s|^|/*@file*/ |'
"""

doxyfile_basics = r"""
FORMULA_FONTSIZE	= 16

MATHJAX_FORMAT		= SVG
DOT_IMAGE_FORMAT	= svg
DOT_MULTI_TARGETS	= YES
DOT_TRANSPARENT         = YES
HAVE_DOT		= YES
GRAPHICAL_HIERARCHY	= YES
COLLABORATION_GRAPH	= NO
GROUP_GRAPHS		= NO
INCLUDE_GRAPH		= NO
INCLUDED_BY_GRAPH	= NO
MARKDOWN_SUPPORT	= NO
AUTOLINK_SUPPORT	= YES
EXTRACT_ALL		= YES
EXTRACT_LOCAL_CLASSES	= YES
EXTRACT_ANON_NSPACES    = NO
INTERNAL_DOCS		= YES
ALLOW_UNICODE_NAMES	= YES
CASE_SENSE_NAMES	= NO
HIDE_UNDOC_MEMBERS	= YES
HIDE_UNDOC_CLASSES	= YES
HIDE_SCOPE_NAMES	= YES
HIDE_COMPOUND_REFERENCE = YES
HIDE_FRIEND_COMPOUNDS   = YES
SHOW_INCLUDE_FILES	= NO
FORCE_LOCAL_INCLUDES	= YES
INLINE_INFO		= NO
SORT_MEMBER_DOCS	= NO
SORT_BRIEF_DOCS		= YES
ALWAYS_DETAILED_SEC	= YES
MAX_INITIALIZER_LINES	= 0
SHOW_USED_FILES		= NO
SHOW_NAMESPACES		= YES
SHOW_FILES		= YES
VERBATIM_HEADERS	= NO
SEARCHENGINE		= YES
GENERATE_LATEX		= NO
GENERATE_HTML		= YES
BRIEF_MEMBER_DESC	= NO
HTML_DYNAMIC_MENUS	= NO
HTML_CODE_FOLDING	= NO
HTML_FORMULA_FORMAT	= svg
HTML_COLORSTYLE_GAMMA   = 80
HTML_COLORSTYLE_HUE     = 230.4
HTML_COLORSTYLE_SAT	= 63.3
HTML_COLORSTYLE         = AUTO_LIGHT

# AUTOLINK_SUPPORT	= NO
# MARKDOWN_SUPPORT	= NO
# DIRECTORY_GRAPH	= YES
# SHOW_NAMESPACES	= NO

GENERATE_TREEVIEW	= YES
DISABLE_INDEX		= YES
FULL_SIDEBAR		= YES

# https://github.com/jothepro/doxygen-awesome-css
#HTML_COLORSTYLE	= LIGHT # required with Doxygen >= 1.9.5
#GENERATE_TREEVIEW	= YES # required!
#DISABLE_INDEX		= NO
#FULL_SIDEBAR		= NO
#HTML_EXTRA_STYLESHEET	= doxygen-awesome.css doxygen-awesome-sidebar-only.css
"""

if __name__ == "__main__":
  main()
