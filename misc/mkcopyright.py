#!/usr/bin/env python3
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import sys, os, re, subprocess, getopt
from datetime import datetime

# TODO:
# - Read header files from INI
# - Parse .po file Copyright statements

# Licenses as found in source code comments
licenses = {
  'MPL-2.0':    [ 'This Source Code Form is licensed MPL-2\.0',
                  'This Source Code Form is licensed MPL-2\.0: http://mozilla.org/MPL/2\.0' ],
  'CC0-1.0':    [ 'Licensed CC0 Public Domain',
                  'CC0 Public Domain: http://creativecommons.org/publicdomain/zero/1\.0/',
                  'Licensed CC0 Public Domain: http://creativecommons.org/publicdomain/zero/1\.0' ],
  'MIT':        [ 'MIT licensed', ],
}

# Comment suffix to ignore
suffixes = '([.,;:]?\s*([Bb]ased on|[Dd]erived from)\s*[:]?\s*http[^ \t]*)?'

# Comment patterns
comments = (
  (r'[#%;]+\s*',        r'\s*[#%;]*'),
  (r'///*\s*',          r'\s*/*'),
  (r'/\*\**\s*',        r'\s*(\*/)?'),
  (r'<!--\s*',          r'\s*(-->)?'),
)

# Comment pattern continuations
ccontinuations = (
  (r'[#%;*+-]*\s*',      r'\s*[#%;*+-]*'),
)

# Patterns for Copyright notices, expects year range in one of two groups
copyrights = (
  r'([0-9, \t-]+)\s+Copyright\s+(.+)',
  r'([0-9, \t-]+)\s+Copyright\s*ⓒ\s+(.+)',
  r'([0-9, \t-]+)\s+Copyright\s*\([Cc]\)\s+(.+)',
  r'Copyright\s*([0-9, \t-]+)\s+(.+)',
  r'Copyright\s*ⓒ\s*([0-9, \t-]+)\s+(.+)',
  r'Copyright\s*\([cC]\)\s*([0-9, \t-]+)\s+(.+)',
)

def load_config_sections (filename, config):
  import configparser
  cparser = configparser.RawConfigParser (empty_lines_in_values = False)
  cparser.optionxform = lambda option: option # avoid lowercasing option names
  cparser.read (filename)
  config.sections = { sectionname: dict (cparser.items (sectionname)) for sectionname in cparser.sections() }
  #for k, v in config.sections.items(): print (k + ':', v)

def copyright_patterns():
  global copyright_patterns_list
  if not copyright_patterns_list:
    l = []
    for crpattern in copyrights:
      for cpair in comments:
        l.append (re.compile (cpair[0] + crpattern + cpair[1]))
      for cpair in ccontinuations:
        l.append (re.compile (cpair[0] + crpattern + cpair[1]))
    copyright_patterns_list = l
  return copyright_patterns_list
copyright_patterns_list = []

def parse_years (yearstring):
  yearstring = yearstring.strip()
  years = []
  for yearish in yearstring.split (','):
    yearish = yearish.strip()
    m = year_range.match (yearish)
    if m:
      b, e = int (m.group (1)), int (m.group (2))
      if b < 100: b += 1900
      if e < 100: e += 1900 if e + 1900 >= b else 2000
      years += [ (min (b, e), max (b, e)) ]
      continue
    m = year_digits.match (yearish)
    if m:
      y = int (m.group())
      if y < 100: y += 1900
      years += [ (y, y) ]
      continue
  return years # [(year,delta),(year,delta),...]
year_digits = re.compile (r'([1-9][0-9][0-9][0-9]\b|[0-9][0-9]\b)')
year_range = re.compile (r'([1-9][0-9][0-9][0-9]|[0-9][0-9])\b\s*[—-]\s*([1-9][0-9][0-9][0-9]|[0-9][0-9])\b')

def open_as_utf8 (filename):
  for line in open (filename, 'rb'):
    try:        string = line.decode ('utf-8')
    except:     string = None
    if string != None: yield string

def find_copyrights (filename):
  copyrights = {}
  for line in open_as_utf8 (filename):
    for crpattern in copyright_patterns():
      m = crpattern.match (line.strip())
      if m:
        a, b = m.group (1).strip(), m.group (2).strip()
        if a[0] not in '0123456789':
          if b[0] not in '0123456789':
            continue
          b, a = a, b
        copyrights[b] = copyrights.get (b, []) + parse_years (a)
  return copyrights

def license_patterns():
  global license_patterns_dict
  if not license_patterns_dict:
    d = {}
    for license, identifiers in licenses.items():
      plist = []
      for ident in identifiers:
        for cpair in comments:
          plist.append (re.compile (cpair[0] + ident + suffixes + cpair[1]))
      d[license] = plist
    license_patterns_dict = d
  return license_patterns_dict
license_patterns_dict = None

def find_license (filename, config):
  n = config.max_header_lines
  for line in open_as_utf8 (filename):
    for license, patterns in license_patterns().items():
      for pattern in patterns:
        if pattern.match (line.strip()):
          return license
    n -= 1
    if not n: break
  return match_license (filename, config)

def match_license (filename, config):
  for identifier in spdx_licenses:
    license_section = config.sections.get (identifier, None)
    if not license_section: continue
    if filename in license_section.get ('files', '').strip().splitlines():
      return identifier
    for p in license_section.get ('patterns', '').strip().splitlines():
      m = re.match (p, filename)
      if m and m.end() == len (filename):
        return identifier

def shcmd (*args):
  process = subprocess.Popen (args, stdout = subprocess.PIPE)
  out, err = process.communicate()
  if process.returncode:
    raise Exception ('%s: failed with status (%d), full command:\n  %s' % (args[0], process.returncode, ' '.join (args)))
  return out.decode ('utf-8')

def print_help (arg0, exit = None):
  u  = "Usage: %s [OPTIONS] <FILES...>" % arg0
  h  = "Scan FILES for licenses and list copyrights from Git(1) authors.\n"
  h += "OPTIONS:\n"
  h += "  -b            Display brief license list\n"
  h += "  -e            Exit with a non-zero status if any files are unlicensed\n"
  h += "  -h, --help    Show command help\n"
  h += "  -l            List licensed files only\n"
  h += "  -u            List unlicensed files only\n"
  h += "  -C<contact>   Add Upstream-Contact field\n"
  h += "  -N<name>      Add Upstream-Name field\n"
  h += "  -S<source>    Add Source field\n"
  if exit: # != 0
    print (u, file = sys.stderr)
    sys.exit (exit)
  print (u)
  print (h.rstrip())
  if exit == 0:
    sys.exit (0)

default_config = {
  'brief': False,
  'error_unlicensed': False,
  'header': '',
  'max_header_lines': 10,
  'sections': {},
  'with_license': True,
  'without_license': True,
}
def parse_options (sysargv, dfltconfig = default_config):
  class Config (object): pass
  config = Config()
  config.__dict__.update (dfltconfig)
  opts, argv = getopt.getopt (sysargv[1:], 'blueh' + 'c:C:N:S:', [ 'help' ])
  for k, v in opts:
    if k == '-b':
      config.brief = True
    elif k == '-c':
      load_config_sections (v, config)
    elif k == '-e':
      config.error_unlicensed = True
    elif k == '-h' or k == '--help':
      print_help (sysargv[0], 0)
    elif k == '-l':
      config.with_license = True
      config.without_license = False
    elif k == '-u':
      config.with_license = False
      config.without_license = True
    elif k == '-C':
      config.header += 'Upstream-Contact: ' + v + '\n'
    elif k == '-N':
      config.header += 'Upstream-Name: ' + v + '\n'
    elif k == '-S':
      config.header += 'Source: ' + v + '\n'
  config.argv = argv
  return config

def mkcopyright (sysargv):
  # parse options and check inputs
  config = parse_options (sysargv)
  if len (config.argv) == 0:
    print_help (sysargv[0], 7)
  # print copyright header
  if config.header:
    if not config.brief:
      print ('Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/')
    print (config.header.rstrip())
  # gather copyrights and licenses
  count_unlicensed = 0
  used_licenses = set()
  for f in config.argv:
    # detect license
    license = find_license (f, config)
    count_unlicensed += not license
    if ((license and not config.with_license) or
        (not license and not config.without_license)):
      continue
    if config.brief:
      print ('%-16s %s' % (license or '?', f))
      continue
    aname, atime = '', ''
    # extract copyright notices
    copyrights = find_copyrights (f)
    # gather copyright history
    for l in shcmd ('git', 'log', '--follow',
                    '--dense', '-b', '-w', '--ignore-blank-lines',
                    '--pretty=%as %an', '--', f).split ('\n'):
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
    # print copyright entries
    print ('\nFiles:', f)
    clines = []
    for n, y in clist:
      years = '%u' % y[0] if y[0] == y[1] else '%u-%u' % (y[0], y[1])
      clines.append ('Copyright (C) ' + years + ' ' + n)
    if len (clines) == 1:
      print ('Copyright:', clines[0])
    else:
      print ('Copyright:')
      for l in clines:
        print ('  ' + l)
    print ('License:', license or '?')
    if license:
      used_licenses.add (license)
  # Print license identifiers
  for l in sorted (used_licenses):
    print ('\nLicense:', l)
    name, links = spdx_licenses.get (l, ('',''))
    if name:
      print (' %s' % name)
    for l in links.split():
      print (' %s' % l)
  # Error on missing licenses
  if config.error_unlicensed and count_unlicensed:
    print ('\n%s: error: missing license in %d files' % (sysargv[0], count_unlicensed), file = sys.stderr)
    sys.exit (1)
  else:
    sys.exit (0)

# License identifiers
spdx_licenses = {
  'MPL-2.0': ('Mozilla Public License 2.0', 'https://spdx.org/licenses/MPL-2.0.html https://www.mozilla.org/MPL/2.0/ https://opensource.org/licenses/MPL-2.0'),
}

if __name__ == '__main__':
  mkcopyright (sys.argv)
