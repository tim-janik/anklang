#!/usr/bin/env python3
# Dedicated to the Public Domain under the Unlicense: https://unlicense.org/UNLICENSE

import sys, os, re, subprocess, getopt, itertools
import fnmatch
from datetime import datetime
sys.stdin.reconfigure (encoding='utf-8')

def die (*args): print (sys.argv[0] + ': error:', *args, file = sys.stderr); sys.exit (1)

def glob_translate (inputstr):
  """Translate pathname with wildcards to regexp."""
  # TODO: use glob.translate from Python 3.13
  pat = ''
  for part in re.split (r'(\*\*|\*|\?)', inputstr):
    if   part == r'**': pat += r'.*'
    elif part == r'*':  pat += r'[^/]*'
    elif part == r'?':  pat += r'.'
    elif part:          pat += re.escape (part)
  return fr'(?s:{pat})\Z'

def print_help (arg0, exit = None):
  # TODO: --file-errors --pattern-errors
  u  = "Usage: %s [OPTIONS] <FILELIST> [COPYRIGHTFILES...]" % arg0
  h  = "Check `Files:` patterns from COPYRIGHTFILES against lines in FILELIST.\n"
  h += "OPTIONS:\n"
  h += "  -e            Exit with an error if patterns mismatch\n"
  h += "  --git         Print Copyright info from Git\n"
  h += "  -h, --help    Show command help\n"
  if exit: # != 0
    print (u, file = sys.stderr)
    sys.exit (exit)
  print (u)
  print (h.rstrip())
  if exit == 0:
    sys.exit (0)

FILE_PATTERNS = []      # [ ( regex, [ COUNT ], COPYRIGHTFILE, LINE, pstr )... ]
COPYRIGHTFILES = []
FILELIST = None
ERROR_ON_MISMATCH = False
GIT_COPYRIGHT = False

def parse_options (sysargv):
  opts, argv = getopt.getopt (sysargv[1:], 'eh', [ 'help', 'git' ])
  for k, v in opts:
    if k == '-e':
      global ERROR_ON_MISMATCH
      ERROR_ON_MISMATCH = True
    elif k == '-h' or k == '--help':
      print_help (sysargv[0], 0)
    elif k == '--git':
      global GIT_COPYRIGHT
      GIT_COPYRIGHT = True
  if len (argv) < 2:
    die ("at least two input files are required: <FILELIST> <COPYRIGHTFILE>")
  global COPYRIGHTFILES, FILELIST
  FILELIST = argv[0]
  COPYRIGHTFILES[:] = argv[1:]

def add_pattern (filename, lineno, pattern):
  global FILE_PATTERNS
  tup = ( re.compile (glob_translate (pattern), 0), [0], filename, lineno, pattern )
  FILE_PATTERNS.append (tup)

def parse_copyrightfile (filename):
  lineno, in_files = 0, False
  for line in open (filename, 'rt').read().splitlines():
    lineno += 1
    if line.startswith ('Files:'):
      in_files = True
      if line[7:]:
        add_pattern (filename, lineno, line[7:])
    elif in_files and line.startswith (' '):
      add_pattern (filename, lineno, line[1:])
    else:
      in_files = False

def git_copyright (filename):
  copyrights = {}
  # gather copyright history
  for l in shcmd ('git', 'log', '--follow',
                  '--dense', '-b', '-w', '--ignore-blank-lines',
                  '--pretty=%as %an', # 2021-02-03 Author Name
                  '--', filename).split ('\n'):
    if len (l) > 10 and l[10] == ' ':
      year = int (l[0:4])
      name = l[11:].strip()
      copyrights[name] = copyrights.get (name, []) + [ (year, year) ]
  # sort, and merge copyright years
  clist = [] # [(name,(firstyear,lastyear)),...]
  for n, yeardeltas in copyrights.items():
    yeardeltas.sort (reverse = True, key = lambda yd: yd[0])
    ylist = []
    for b, e in yeardeltas:
      if len (ylist) and b <= ylist[-1][1] + 1 and ylist[-1][0] <= e + 1:
        ylist[-1][0] = min (ylist[-1][0], b)
        ylist[-1][1] = max (ylist[-1][1], e)
        continue # merged
      ylist.append ([b, e])
    for yrange in ylist:
      clist.append ((n, yrange))
  clist.sort (reverse = True, key = lambda yd: yd[1][1] - yd[1][0]) # secondary, sort by largest range
  clist.sort (reverse = True, key = lambda yd: yd[1][1])            # primary, sort by latest year
  # list copyright entries
  clines = []
  for n, y in clist:
    years = '%u' % y[0] if y[0] == y[1] else '%u-%u' % (y[0], y[1])
    clines.append ('Copyright (C) ' + years + ' ' + n)
  return clines

def crpathcheck (sysargv):
  # parse options and check inputs
  parse_options (sysargv)
  # compile copyright patterns
  for crf in COPYRIGHTFILES:
    parse_copyrightfile (crf)
  # sort patterns by specificity, i.e. length and absence of wildcards
  FILE_PATTERNS[:] = sorted (FILE_PATTERNS, key = lambda tup: (tup[4].count ('**'), tup[4].count ('*'), tup[4].count ('?'), -len (tup[4]), tup[4]))
  #print ('\n'.join (str (e) for e in FILE_PATTERNS))
  # read input file or stdin
  inputstream = sys.stdin if FILELIST == '-' else open (FILELIST, 'rt')
  # check all files for matching pattern
  fileerrors = 0
  for fileline in inputstream.read().splitlines():
    fmatch = False
    for tup in FILE_PATTERNS:
      if tup[0].match (fileline):
        tup[1][0] += 1
        fmatch = True
        break
    if not fmatch:
      print (sys.argv[0] + ':', 'UNKNOWN-COYPRIGHT:', fileline, file = sys.stderr)
      fileerrors += 1
    if not fmatch and GIT_COPYRIGHT:
      clines = git_copyright (fileline)
      if len (clines) > 0:
        print ('Files:', fileline)
        if len (clines) == 1: print ('Copyright:', clines[0])
        else:
          print ('Copyright:')
          for l in clines:
            print (' ' + l)
        print ('License: ?')
        print ()
  # check for unused patterns
  patternerrors = 0
  for tup in FILE_PATTERNS:
    if tup[1][0] == 0:
      print ('%s:%u:' % (tup[2], tup[3]), 'UNUSED-ENTRY:', tup[4])
      patternerrors += 1
  # error out on -e
  if ERROR_ON_MISMATCH and patternerrors:
    die ("failed to match copyright entry in %u cases" % patternerrors)
  if ERROR_ON_MISMATCH and fileerrors:
    die ("failed to match file to copyright entry in %u cases" % fileerrors)
  sys.exit (0)

def shcmd (*args):
  process = subprocess.Popen (args, stdout = subprocess.PIPE)
  out, err = process.communicate()
  if process.returncode:
    raise Exception ('%s: failed with status (%d), full command:\n  %s' % (args[0], process.returncode, ' '.join (args)))
  return out.decode ('utf-8')

if __name__ == '__main__':
  crpathcheck (sys.argv)
