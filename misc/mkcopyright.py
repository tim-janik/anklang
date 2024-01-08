#!/usr/bin/env python3
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import sys, os, re, subprocess, getopt
from datetime import datetime

# TODO:
# - Parse .po file Copyright statements

# Licenses as found in source code comments
licenses = {
  'MPL-2.0':    [ 'This Source Code Form is licensed MPL-2\.0',
                  'This Source Code Form is licensed MPL-2\.0: http://mozilla.org/MPL/2\.0' ],
  'CC0-1.0':    [ 'Licensed CC0 Public Domain',
                  'CC0 Public Domain: http://creativecommons.org/publicdomain/zero/1\.0/',
                  'Licensed CC0 Public Domain: http://creativecommons.org/publicdomain/zero/1\.0' ],
  'MIT':        [ 'MIT [Ll]icensed?', ],
  'ISC':        [ 'SPDX-License-Identifier: ISC' ]
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

re_MSI = re.M | re.S | re.I

# Match <cc:license resource=""/> from ccREL specification
rdf_xmlnscc_license = re.compile (r'<rdf:RDF\b.*\bxmlns:cc="https?://.*<cc:license\b[^>]*\bresource="([^"]+)"', re_MSI)
# SVG metadata for cc:license
xmlnscc_worklicense = re.compile (r'\bxmlns:cc="https?://.*<(?:rdf:RDF|cc:Work)\b.*<cc:license\b[^>]*\bresource="([^"]+)"', re_MSI)

def load_config_sections (filename, config):
  import configparser
  cparser = configparser.ConfigParser (empty_lines_in_values = False)
  cparser.optionxform = lambda option: option # avoid lowercasing option names
  cparser.read (filename)
  config.sections.update ({ sectionname: dict (cparser.items (sectionname)) for sectionname in cparser.sections() })
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
  try:
    ofile = open (filename, 'rb')
  except IsADirectoryError:
    return # ignore dirs
  for line in ofile:
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

def xml_license (xmlstring, config):
  m = xmlnscc_worklicense.search (xmlstring)
  if m: return m.group (1)
  m = rdf_xmlnscc_license.search (xmlstring)
  if m: return m.group (1)

def find_license (filename, config):
  utf8file = open_as_utf8 (filename)
  lines = []
  # find known license comment patterns
  for line in utf8file:
    for license, patterns in license_patterns().items():
      for pattern in patterns:
        if pattern.match (line.strip()):
          return license
    lines.append (line)
    if len (lines) >= config.max_header_lines: break
  # search xml-like files
  if re.search (r'<\?xml|<x?html|<svg|<rdf:RDF', ''.join (lines), re.I):
    for line in utf8file:
      lines.append (line)
      if len (lines) >= config.max_xml_lines: break
    license = xml_license (''.join (lines), config)
    if license: return license
  # match license to pathname
  return match_license (filename, config)

def match_license (filename, config):
  for identifier in spdx_licenses:
    if match_section (filename, config, identifier):
      return identifier

def match_section (filename, config, identifier):
  license_section = config.sections.get (identifier, None)
  if license_section:
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
  'max_header_lines': 10,
  'max_xml_lines': 999,
  'sections': { 'debian/copyright': {}, },
  'with_license': True,
  'without_license': True,
}
def parse_options (sysargv, dfltconfig = default_config):
  class Config (object): pass
  config = Config()
  config.__dict__.update (dfltconfig)
  opts, argv = getopt.getopt (sysargv[1:], 'blueh' + 'c:C:N:S:', [ 'help', 'spdx-licenses' ])
  upstream_headers = {}
  for k, v in opts:
    if k == '-b':
      config.brief = True
    elif k == '-c':
      load_config_sections (v, config)
    elif k == '-e':
      config.error_unlicensed = True
    elif k == '-h' or k == '--help':
      print_help (sysargv[0], 0)
    elif k == '--spdx-licenses':
      print_spdx_licenses()
      sys.exit (0)
    elif k == '-l':
      config.with_license = True
      config.without_license = False
    elif k == '-u':
      config.with_license = False
      config.without_license = True
    elif k == '-C':
      upstream_headers['Upstream-Contact'] = v
    elif k == '-N':
      upstream_headers['Upstream-Name'] = v
    elif k == '-S':
      upstream_headers['Source'] = v
  config.argv = argv
  config.sections['debian/copyright'].update (upstream_headers)
  return config

def mkcopyright (sysargv):
  # parse options and check inputs
  config = parse_options (sysargv)
  if len (config.argv) == 0:
    print_help (sysargv[0], 7)
  # print debian/copyright header
  if not config.brief and config.sections['debian/copyright']:
    print ('Format:', config.sections['debian/copyright'].get ('Format', 'https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/'))
    if 'Upstream-Name' in config.sections['debian/copyright']:
      print ('Upstream-Name:', config.sections['debian/copyright']['Upstream-Name'])
    for k, v in config.sections['debian/copyright'].items():
      if k in ('Format', 'Upstream-Name'): continue # initial fields
      print (k + ':', v)
  # gather copyrights and licenses
  count_unlicensed = 0
  used_licenses = set()
  for filename in config.argv:
    # ignore files
    if match_section (filename, config, 'ignore'):
      continue
    # detect license
    license = find_license (filename, config)
    count_unlicensed += not license
    if ((license and not config.with_license) or
        (not license and not config.without_license)):
      continue
    if config.brief:
      print ('%-16s %s' % (license or '?', filename))
      continue
    aname, atime = '', ''
    # extract copyright notices
    copyrights = find_copyrights (filename)
    # gather copyright history
    for l in shcmd ('git', 'log', '--follow',
                    '--dense', '-b', '-w', '--ignore-blank-lines',
                    '--pretty=%as %an', '--', filename).split ('\n'):
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
    print ('\nFiles:', filename)
    clines = []
    for n, y in clist:
      years = '%u' % y[0] if y[0] == y[1] else '%u-%u' % (y[0], y[1])
      clines.append ('Copyright (C) ' + years + ' ' + n)
    if len (clines) == 1:
      print ('Copyright:', clines[0])
    elif clines:
      print ('Copyright:')
      for l in clines:
        print ('  ' + l)
    print ('License:', license or '?')
    if license:
      used_licenses.add (license)
  # Print license identifiers
  for l in sorted (used_licenses):
    name, links = spdx_licenses.get (l, ('',''))
    if name or links:
      print ('\nLicense:', l)
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

# License identifiers retrieved from https://spdx.org/licenses/ under CC-BY-3.0
spdx_licenses = {
  '0BSD': ('BSD Zero Clause License', 'https://spdx.org/licenses/0BSD.html http://landley.net/toybox/license.html'),
  'AAL': ('Attribution Assurance License', 'https://spdx.org/licenses/AAL.html https://opensource.org/licenses/attribution'),
  'ADSL': ('Amazon Digital Services License', 'https://spdx.org/licenses/ADSL.html https://fedoraproject.org/wiki/Licensing/AmazonDigitalServicesLicense'),
  'AFL-1.1': ('Academic Free License v1.1', 'https://spdx.org/licenses/AFL-1.1.html http://opensource.linux-mirror.org/licenses/afl-1.1.txt http://wayback.archive.org/web/20021004124254/http://www.opensource.org/licenses/academic.php'),
  'AFL-1.2': ('Academic Free License v1.2', 'https://spdx.org/licenses/AFL-1.2.html http://opensource.linux-mirror.org/licenses/afl-1.2.txt http://wayback.archive.org/web/20021204204652/http://www.opensource.org/licenses/academic.php'),
  'AFL-2.0': ('Academic Free License v2.0', 'https://spdx.org/licenses/AFL-2.0.html http://wayback.archive.org/web/20060924134533/http://www.opensource.org/licenses/afl-2.0.txt'),
  'AFL-2.1': ('Academic Free License v2.1', 'https://spdx.org/licenses/AFL-2.1.html http://opensource.linux-mirror.org/licenses/afl-2.1.txt'),
  'AFL-3.0': ('Academic Free License v3.0', 'https://spdx.org/licenses/AFL-3.0.html http://www.rosenlaw.com/AFL3.0.htm https://opensource.org/licenses/afl-3.0'),
  'AGPL-1.0': ('Affero General Public License v1.0', 'https://spdx.org/licenses/AGPL-1.0.html http://www.affero.org/oagpl.html'),
  'AGPL-1.0-only': ('Affero General Public License v1.0 only', 'https://spdx.org/licenses/AGPL-1.0-only.html http://www.affero.org/oagpl.html'),
  'AGPL-1.0-or-later': ('Affero General Public License v1.0 or later', 'https://spdx.org/licenses/AGPL-1.0-or-later.html http://www.affero.org/oagpl.html'),
  'AGPL-3.0': ('GNU Affero General Public License v3.0', 'https://spdx.org/licenses/AGPL-3.0.html https://www.gnu.org/licenses/agpl.txt https://opensource.org/licenses/AGPL-3.0'),
  'AGPL-3.0-only': ('GNU Affero General Public License v3.0 only', 'https://spdx.org/licenses/AGPL-3.0-only.html https://www.gnu.org/licenses/agpl.txt https://opensource.org/licenses/AGPL-3.0'),
  'AGPL-3.0-or-later': ('GNU Affero General Public License v3.0 or later', 'https://spdx.org/licenses/AGPL-3.0-or-later.html https://www.gnu.org/licenses/agpl.txt https://opensource.org/licenses/AGPL-3.0'),
  'AMDPLPA': ("AMD's plpa_map.c License", 'https://spdx.org/licenses/AMDPLPA.html https://fedoraproject.org/wiki/Licensing/AMD_plpa_map_License'),
  'AML': ('Apple MIT License', 'https://spdx.org/licenses/AML.html https://fedoraproject.org/wiki/Licensing/Apple_MIT_License'),
  'AMPAS': ('Academy of Motion Picture Arts and Sciences BSD', 'https://spdx.org/licenses/AMPAS.html https://fedoraproject.org/wiki/Licensing/BSD#AMPASBSD'),
  'ANTLR-PD': ('ANTLR Software Rights Notice', 'https://spdx.org/licenses/ANTLR-PD.html http://www.antlr2.org/license.html'),
  'ANTLR-PD-fallback': ('ANTLR Software Rights Notice with license fallback', 'https://spdx.org/licenses/ANTLR-PD-fallback.html http://www.antlr2.org/license.html'),
  'APAFML': ('Adobe Postscript AFM License', 'https://spdx.org/licenses/APAFML.html https://fedoraproject.org/wiki/Licensing/AdobePostscriptAFM'),
  'APL-1.0': ('Adaptive Public License 1.0', 'https://spdx.org/licenses/APL-1.0.html https://opensource.org/licenses/APL-1.0'),
  'APSL-1.0': ('Apple Public Source License 1.0', 'https://spdx.org/licenses/APSL-1.0.html https://fedoraproject.org/wiki/Licensing/Apple_Public_Source_License_1.0'),
  'APSL-1.1': ('Apple Public Source License 1.1', 'https://spdx.org/licenses/APSL-1.1.html http://www.opensource.apple.com/source/IOSerialFamily/IOSerialFamily-7/APPLE_LICENSE'),
  'APSL-1.2': ('Apple Public Source License 1.2', 'https://spdx.org/licenses/APSL-1.2.html http://www.samurajdata.se/opensource/mirror/licenses/apsl.php'),
  'APSL-2.0': ('Apple Public Source License 2.0', 'https://spdx.org/licenses/APSL-2.0.html http://www.opensource.apple.com/license/apsl/'),
  'Abstyles': ('Abstyles License', 'https://spdx.org/licenses/Abstyles.html https://fedoraproject.org/wiki/Licensing/Abstyles'),
  'Adobe-2006': ('Adobe Systems Incorporated Source Code License Agreement', 'https://spdx.org/licenses/Adobe-2006.html https://fedoraproject.org/wiki/Licensing/AdobeLicense'),
  'Adobe-Glyph': ('Adobe Glyph List License', 'https://spdx.org/licenses/Adobe-Glyph.html https://fedoraproject.org/wiki/Licensing/MIT#AdobeGlyph'),
  'Afmparse': ('Afmparse License', 'https://spdx.org/licenses/Afmparse.html https://fedoraproject.org/wiki/Licensing/Afmparse'),
  'Aladdin': ('Aladdin Free Public License', 'https://spdx.org/licenses/Aladdin.html http://pages.cs.wisc.edu/~ghost/doc/AFPL/6.01/Public.htm'),
  'Apache-1.0': ('Apache License 1.0', 'https://spdx.org/licenses/Apache-1.0.html http://www.apache.org/licenses/LICENSE-1.0'),
  'Apache-1.1': ('Apache License 1.1', 'https://spdx.org/licenses/Apache-1.1.html http://apache.org/licenses/LICENSE-1.1 https://opensource.org/licenses/Apache-1.1'),
  'Apache-2.0': ('Apache License 2.0', 'https://spdx.org/licenses/Apache-2.0.html https://www.apache.org/licenses/LICENSE-2.0 https://opensource.org/licenses/Apache-2.0'),
  'Artistic-1.0': ('Artistic License 1.0', 'https://spdx.org/licenses/Artistic-1.0.html https://opensource.org/licenses/Artistic-1.0'),
  'Artistic-1.0-Perl': ('Artistic License 1.0 (Perl)', 'https://spdx.org/licenses/Artistic-1.0-Perl.html http://dev.perl.org/licenses/artistic.html'),
  'Artistic-1.0-cl8': ('Artistic License 1.0 w/clause 8', 'https://spdx.org/licenses/Artistic-1.0-cl8.html https://opensource.org/licenses/Artistic-1.0'),
  'Artistic-2.0': ('Artistic License 2.0', 'https://spdx.org/licenses/Artistic-2.0.html http://www.perlfoundation.org/artistic_license_2_0 https://www.perlfoundation.org/artistic-license-20.html https://opensource.org/licenses/artistic-license-2.0'),
  'BSD-1-Clause': ('BSD 1-Clause License', 'https://spdx.org/licenses/BSD-1-Clause.html https://svnweb.freebsd.org/base/head/include/ifaddrs.h?revision=326823'),
  'BSD-2-Clause': ('BSD 2-Clause "Simplified" License', 'https://spdx.org/licenses/BSD-2-Clause.html https://opensource.org/licenses/BSD-2-Clause'),
  'BSD-2-Clause-FreeBSD': ('BSD 2-Clause FreeBSD License', 'https://spdx.org/licenses/BSD-2-Clause-FreeBSD.html http://www.freebsd.org/copyright/freebsd-license.html'),
  'BSD-2-Clause-NetBSD': ('BSD 2-Clause NetBSD License', 'https://spdx.org/licenses/BSD-2-Clause-NetBSD.html http://www.netbsd.org/about/redistribution.html#default'),
  'BSD-2-Clause-Patent': ('BSD-2-Clause Plus Patent License', 'https://spdx.org/licenses/BSD-2-Clause-Patent.html https://opensource.org/licenses/BSDplusPatent'),
  'BSD-2-Clause-Views': ('BSD 2-Clause with views sentence', 'https://spdx.org/licenses/BSD-2-Clause-Views.html http://www.freebsd.org/copyright/freebsd-license.html https://people.freebsd.org/~ivoras/wine/patch-wine-nvidia.sh https://github.com/protegeproject/protege/blob/master/license.txt'),
  'BSD-3-Clause': ('BSD 3-Clause "New" or "Revised" License', 'https://spdx.org/licenses/BSD-3-Clause.html https://opensource.org/licenses/BSD-3-Clause'),
  'BSD-3-Clause-Attribution': ('BSD with attribution', 'https://spdx.org/licenses/BSD-3-Clause-Attribution.html https://fedoraproject.org/wiki/Licensing/BSD_with_Attribution'),
  'BSD-3-Clause-Clear': ('BSD 3-Clause Clear License', 'https://spdx.org/licenses/BSD-3-Clause-Clear.html http://labs.metacarta.com/license-explanation.html#license'),
  'BSD-3-Clause-LBNL': ('Lawrence Berkeley National Labs BSD variant license', 'https://spdx.org/licenses/BSD-3-Clause-LBNL.html https://fedoraproject.org/wiki/Licensing/LBNLBSD'),
  'BSD-3-Clause-Modification': ('BSD 3-Clause Modification', 'https://spdx.org/licenses/BSD-3-Clause-Modification.html https://fedoraproject.org/wiki/Licensing:BSD#Modification_Variant'),
  'BSD-3-Clause-No-Military-License': ('BSD 3-Clause No Military License', 'https://spdx.org/licenses/BSD-3-Clause-No-Military-License.html https://gitlab.syncad.com/hive/dhive/-/blob/master/LICENSE https://github.com/greymass/swift-eosio/blob/master/LICENSE'),
  'BSD-3-Clause-No-Nuclear-License': ('BSD 3-Clause No Nuclear License', 'https://spdx.org/licenses/BSD-3-Clause-No-Nuclear-License.html http://download.oracle.com/otn-pub/java/licenses/bsd.txt?AuthParam=1467140197_43d516ce1776bd08a58235a7785be1cc'),
  'BSD-3-Clause-No-Nuclear-License-2014': ('BSD 3-Clause No Nuclear License 2014', 'https://spdx.org/licenses/BSD-3-Clause-No-Nuclear-License-2014.html https://java.net/projects/javaeetutorial/pages/BerkeleyLicense'),
  'BSD-3-Clause-No-Nuclear-Warranty': ('BSD 3-Clause No Nuclear Warranty', 'https://spdx.org/licenses/BSD-3-Clause-No-Nuclear-Warranty.html https://jogamp.org/git/?p=gluegen.git;a=blob_plain;f=LICENSE.txt'),
  'BSD-3-Clause-Open-MPI': ('BSD 3-Clause Open MPI variant', 'https://spdx.org/licenses/BSD-3-Clause-Open-MPI.html https://www.open-mpi.org/community/license.php http://www.netlib.org/lapack/LICENSE.txt'),
  'BSD-4-Clause': ('BSD 4-Clause "Original" or "Old" License', 'https://spdx.org/licenses/BSD-4-Clause.html http://directory.fsf.org/wiki/License:BSD_4Clause'),
  'BSD-4-Clause-Shortened': ('BSD 4 Clause Shortened', 'https://spdx.org/licenses/BSD-4-Clause-Shortened.html https://metadata.ftp-master.debian.org/changelogs//main/a/arpwatch/arpwatch_2.1a15-7_copyright'),
  'BSD-4-Clause-UC': ('BSD-4-Clause (University of California-Specific)', 'https://spdx.org/licenses/BSD-4-Clause-UC.html http://www.freebsd.org/copyright/license.html'),
  'BSD-Protection': ('BSD Protection License', 'https://spdx.org/licenses/BSD-Protection.html https://fedoraproject.org/wiki/Licensing/BSD_Protection_License'),
  'BSD-Source-Code': ('BSD Source Code Attribution', 'https://spdx.org/licenses/BSD-Source-Code.html https://github.com/robbiehanson/CocoaHTTPServer/blob/master/LICENSE.txt'),
  'BSL-1.0': ('Boost Software License 1.0', 'https://spdx.org/licenses/BSL-1.0.html http://www.boost.org/LICENSE_1_0.txt https://opensource.org/licenses/BSL-1.0'),
  'BUSL-1.1': ('Business Source License 1.1', 'https://spdx.org/licenses/BUSL-1.1.html https://mariadb.com/bsl11/'),
  'Bahyph': ('Bahyph License', 'https://spdx.org/licenses/Bahyph.html https://fedoraproject.org/wiki/Licensing/Bahyph'),
  'Barr': ('Barr License', 'https://spdx.org/licenses/Barr.html https://fedoraproject.org/wiki/Licensing/Barr'),
  'Beerware': ('Beerware License', 'https://spdx.org/licenses/Beerware.html https://fedoraproject.org/wiki/Licensing/Beerware https://people.freebsd.org/~phk/'),
  'BitTorrent-1.0': ('BitTorrent Open Source License v1.0', 'https://spdx.org/licenses/BitTorrent-1.0.html http://sources.gentoo.org/cgi-bin/viewvc.cgi/gentoo-x86/licenses/BitTorrent?r1=1.1&r2=1.1.1.1&diff_format=s'),
  'BitTorrent-1.1': ('BitTorrent Open Source License v1.1', 'https://spdx.org/licenses/BitTorrent-1.1.html http://directory.fsf.org/wiki/License:BitTorrentOSL1.1'),
  'BlueOak-1.0.0': ('Blue Oak Model License 1.0.0', 'https://spdx.org/licenses/BlueOak-1.0.0.html https://blueoakcouncil.org/license/1.0.0'),
  'Borceux': ('Borceux license', 'https://spdx.org/licenses/Borceux.html https://fedoraproject.org/wiki/Licensing/Borceux'),
  'C-UDA-1.0': ('Computational Use of Data Agreement v1.0', 'https://spdx.org/licenses/C-UDA-1.0.html https://github.com/microsoft/Computational-Use-of-Data-Agreement/blob/master/C-UDA-1.0.md https://cdla.dev/computational-use-of-data-agreement-v1-0/'),
  'CAL-1.0': ('Cryptographic Autonomy License 1.0', 'https://spdx.org/licenses/CAL-1.0.html http://cryptographicautonomylicense.com/license-text.html https://opensource.org/licenses/CAL-1.0'),
  'CAL-1.0-Combined-Work-Exception': ('Cryptographic Autonomy License 1.0 (Combined Work Exception)', 'https://spdx.org/licenses/CAL-1.0-Combined-Work-Exception.html http://cryptographicautonomylicense.com/license-text.html https://opensource.org/licenses/CAL-1.0'),
  'CATOSL-1.1': ('Computer Associates Trusted Open Source License 1.1', 'https://spdx.org/licenses/CATOSL-1.1.html https://opensource.org/licenses/CATOSL-1.1'),
  'CC-BY-1.0': ('Creative Commons Attribution 1.0 Generic', 'https://spdx.org/licenses/CC-BY-1.0.html https://creativecommons.org/licenses/by/1.0/legalcode'),
  'CC-BY-2.0': ('Creative Commons Attribution 2.0 Generic', 'https://spdx.org/licenses/CC-BY-2.0.html https://creativecommons.org/licenses/by/2.0/legalcode'),
  'CC-BY-2.5': ('Creative Commons Attribution 2.5 Generic', 'https://spdx.org/licenses/CC-BY-2.5.html https://creativecommons.org/licenses/by/2.5/legalcode'),
  'CC-BY-2.5-AU': ('Creative Commons Attribution 2.5 Australia', 'https://spdx.org/licenses/CC-BY-2.5-AU.html https://creativecommons.org/licenses/by/2.5/au/legalcode'),
  'CC-BY-3.0': ('Creative Commons Attribution 3.0 Unported', 'https://spdx.org/licenses/CC-BY-3.0.html https://creativecommons.org/licenses/by/3.0/legalcode'),
  'CC-BY-3.0-AT': ('Creative Commons Attribution 3.0 Austria', 'https://spdx.org/licenses/CC-BY-3.0-AT.html https://creativecommons.org/licenses/by/3.0/at/legalcode'),
  'CC-BY-3.0-DE': ('Creative Commons Attribution 3.0 Germany', 'https://spdx.org/licenses/CC-BY-3.0-DE.html https://creativecommons.org/licenses/by/3.0/de/legalcode'),
  'CC-BY-3.0-NL': ('Creative Commons Attribution 3.0 Netherlands', 'https://spdx.org/licenses/CC-BY-3.0-NL.html https://creativecommons.org/licenses/by/3.0/nl/legalcode'),
  'CC-BY-3.0-US': ('Creative Commons Attribution 3.0 United States', 'https://spdx.org/licenses/CC-BY-3.0-US.html https://creativecommons.org/licenses/by/3.0/us/legalcode'),
  'CC-BY-4.0': ('Creative Commons Attribution 4.0 International', 'https://spdx.org/licenses/CC-BY-4.0.html https://creativecommons.org/licenses/by/4.0/legalcode'),
  'CC-BY-NC-1.0': ('Creative Commons Attribution Non Commercial 1.0 Generic', 'https://spdx.org/licenses/CC-BY-NC-1.0.html https://creativecommons.org/licenses/by-nc/1.0/legalcode'),
  'CC-BY-NC-2.0': ('Creative Commons Attribution Non Commercial 2.0 Generic', 'https://spdx.org/licenses/CC-BY-NC-2.0.html https://creativecommons.org/licenses/by-nc/2.0/legalcode'),
  'CC-BY-NC-2.5': ('Creative Commons Attribution Non Commercial 2.5 Generic', 'https://spdx.org/licenses/CC-BY-NC-2.5.html https://creativecommons.org/licenses/by-nc/2.5/legalcode'),
  'CC-BY-NC-3.0': ('Creative Commons Attribution Non Commercial 3.0 Unported', 'https://spdx.org/licenses/CC-BY-NC-3.0.html https://creativecommons.org/licenses/by-nc/3.0/legalcode'),
  'CC-BY-NC-3.0-DE': ('Creative Commons Attribution Non Commercial 3.0 Germany', 'https://spdx.org/licenses/CC-BY-NC-3.0-DE.html https://creativecommons.org/licenses/by-nc/3.0/de/legalcode'),
  'CC-BY-NC-4.0': ('Creative Commons Attribution Non Commercial 4.0 International', 'https://spdx.org/licenses/CC-BY-NC-4.0.html https://creativecommons.org/licenses/by-nc/4.0/legalcode'),
  'CC-BY-NC-ND-1.0': ('Creative Commons Attribution Non Commercial No Derivatives 1.0 Generic', 'https://spdx.org/licenses/CC-BY-NC-ND-1.0.html https://creativecommons.org/licenses/by-nd-nc/1.0/legalcode'),
  'CC-BY-NC-ND-2.0': ('Creative Commons Attribution Non Commercial No Derivatives 2.0 Generic', 'https://spdx.org/licenses/CC-BY-NC-ND-2.0.html https://creativecommons.org/licenses/by-nc-nd/2.0/legalcode'),
  'CC-BY-NC-ND-2.5': ('Creative Commons Attribution Non Commercial No Derivatives 2.5 Generic', 'https://spdx.org/licenses/CC-BY-NC-ND-2.5.html https://creativecommons.org/licenses/by-nc-nd/2.5/legalcode'),
  'CC-BY-NC-ND-3.0': ('Creative Commons Attribution Non Commercial No Derivatives 3.0 Unported', 'https://spdx.org/licenses/CC-BY-NC-ND-3.0.html https://creativecommons.org/licenses/by-nc-nd/3.0/legalcode'),
  'CC-BY-NC-ND-3.0-DE': ('Creative Commons Attribution Non Commercial No Derivatives 3.0 Germany', 'https://spdx.org/licenses/CC-BY-NC-ND-3.0-DE.html https://creativecommons.org/licenses/by-nc-nd/3.0/de/legalcode'),
  'CC-BY-NC-ND-3.0-IGO': ('Creative Commons Attribution Non Commercial No Derivatives 3.0 IGO', 'https://spdx.org/licenses/CC-BY-NC-ND-3.0-IGO.html https://creativecommons.org/licenses/by-nc-nd/3.0/igo/legalcode'),
  'CC-BY-NC-ND-4.0': ('Creative Commons Attribution Non Commercial No Derivatives 4.0 International', 'https://spdx.org/licenses/CC-BY-NC-ND-4.0.html https://creativecommons.org/licenses/by-nc-nd/4.0/legalcode'),
  'CC-BY-NC-SA-1.0': ('Creative Commons Attribution Non Commercial Share Alike 1.0 Generic', 'https://spdx.org/licenses/CC-BY-NC-SA-1.0.html https://creativecommons.org/licenses/by-nc-sa/1.0/legalcode'),
  'CC-BY-NC-SA-2.0': ('Creative Commons Attribution Non Commercial Share Alike 2.0 Generic', 'https://spdx.org/licenses/CC-BY-NC-SA-2.0.html https://creativecommons.org/licenses/by-nc-sa/2.0/legalcode'),
  'CC-BY-NC-SA-2.0-FR': ('Creative Commons Attribution-NonCommercial-ShareAlike 2.0 France', 'https://spdx.org/licenses/CC-BY-NC-SA-2.0-FR.html https://creativecommons.org/licenses/by-nc-sa/2.0/fr/legalcode'),
  'CC-BY-NC-SA-2.0-UK': ('Creative Commons Attribution Non Commercial Share Alike 2.0 England and Wales', 'https://spdx.org/licenses/CC-BY-NC-SA-2.0-UK.html https://creativecommons.org/licenses/by-nc-sa/2.0/uk/legalcode'),
  'CC-BY-NC-SA-2.5': ('Creative Commons Attribution Non Commercial Share Alike 2.5 Generic', 'https://spdx.org/licenses/CC-BY-NC-SA-2.5.html https://creativecommons.org/licenses/by-nc-sa/2.5/legalcode'),
  'CC-BY-NC-SA-3.0': ('Creative Commons Attribution Non Commercial Share Alike 3.0 Unported', 'https://spdx.org/licenses/CC-BY-NC-SA-3.0.html https://creativecommons.org/licenses/by-nc-sa/3.0/legalcode'),
  'CC-BY-NC-SA-3.0-DE': ('Creative Commons Attribution Non Commercial Share Alike 3.0 Germany', 'https://spdx.org/licenses/CC-BY-NC-SA-3.0-DE.html https://creativecommons.org/licenses/by-nc-sa/3.0/de/legalcode'),
  'CC-BY-NC-SA-3.0-IGO': ('Creative Commons Attribution Non Commercial Share Alike 3.0 IGO', 'https://spdx.org/licenses/CC-BY-NC-SA-3.0-IGO.html https://creativecommons.org/licenses/by-nc-sa/3.0/igo/legalcode'),
  'CC-BY-NC-SA-4.0': ('Creative Commons Attribution Non Commercial Share Alike 4.0 International', 'https://spdx.org/licenses/CC-BY-NC-SA-4.0.html https://creativecommons.org/licenses/by-nc-sa/4.0/legalcode'),
  'CC-BY-ND-1.0': ('Creative Commons Attribution No Derivatives 1.0 Generic', 'https://spdx.org/licenses/CC-BY-ND-1.0.html https://creativecommons.org/licenses/by-nd/1.0/legalcode'),
  'CC-BY-ND-2.0': ('Creative Commons Attribution No Derivatives 2.0 Generic', 'https://spdx.org/licenses/CC-BY-ND-2.0.html https://creativecommons.org/licenses/by-nd/2.0/legalcode'),
  'CC-BY-ND-2.5': ('Creative Commons Attribution No Derivatives 2.5 Generic', 'https://spdx.org/licenses/CC-BY-ND-2.5.html https://creativecommons.org/licenses/by-nd/2.5/legalcode'),
  'CC-BY-ND-3.0': ('Creative Commons Attribution No Derivatives 3.0 Unported', 'https://spdx.org/licenses/CC-BY-ND-3.0.html https://creativecommons.org/licenses/by-nd/3.0/legalcode'),
  'CC-BY-ND-3.0-DE': ('Creative Commons Attribution No Derivatives 3.0 Germany', 'https://spdx.org/licenses/CC-BY-ND-3.0-DE.html https://creativecommons.org/licenses/by-nd/3.0/de/legalcode'),
  'CC-BY-ND-4.0': ('Creative Commons Attribution No Derivatives 4.0 International', 'https://spdx.org/licenses/CC-BY-ND-4.0.html https://creativecommons.org/licenses/by-nd/4.0/legalcode'),
  'CC-BY-SA-1.0': ('Creative Commons Attribution Share Alike 1.0 Generic', 'https://spdx.org/licenses/CC-BY-SA-1.0.html https://creativecommons.org/licenses/by-sa/1.0/legalcode'),
  'CC-BY-SA-2.0': ('Creative Commons Attribution Share Alike 2.0 Generic', 'https://spdx.org/licenses/CC-BY-SA-2.0.html https://creativecommons.org/licenses/by-sa/2.0/legalcode'),
  'CC-BY-SA-2.0-UK': ('Creative Commons Attribution Share Alike 2.0 England and Wales', 'https://spdx.org/licenses/CC-BY-SA-2.0-UK.html https://creativecommons.org/licenses/by-sa/2.0/uk/legalcode'),
  'CC-BY-SA-2.1-JP': ('Creative Commons Attribution Share Alike 2.1 Japan', 'https://spdx.org/licenses/CC-BY-SA-2.1-JP.html https://creativecommons.org/licenses/by-sa/2.1/jp/legalcode'),
  'CC-BY-SA-2.5': ('Creative Commons Attribution Share Alike 2.5 Generic', 'https://spdx.org/licenses/CC-BY-SA-2.5.html https://creativecommons.org/licenses/by-sa/2.5/legalcode'),
  'CC-BY-SA-3.0': ('Creative Commons Attribution Share Alike 3.0 Unported', 'https://spdx.org/licenses/CC-BY-SA-3.0.html https://creativecommons.org/licenses/by-sa/3.0/legalcode'),
  'CC-BY-SA-3.0-AT': ('Creative Commons Attribution Share Alike 3.0 Austria', 'https://spdx.org/licenses/CC-BY-SA-3.0-AT.html https://creativecommons.org/licenses/by-sa/3.0/at/legalcode'),
  'CC-BY-SA-3.0-DE': ('Creative Commons Attribution Share Alike 3.0 Germany', 'https://spdx.org/licenses/CC-BY-SA-3.0-DE.html https://creativecommons.org/licenses/by-sa/3.0/de/legalcode'),
  'CC-BY-SA-4.0': ('Creative Commons Attribution Share Alike 4.0 International', 'https://spdx.org/licenses/CC-BY-SA-4.0.html https://creativecommons.org/licenses/by-sa/4.0/legalcode'),
  'CC-PDDC': ('Creative Commons Public Domain Dedication and Certification', 'https://spdx.org/licenses/CC-PDDC.html https://creativecommons.org/licenses/publicdomain/'),
  'CC0-1.0': ('Creative Commons Zero v1.0 Universal', 'https://spdx.org/licenses/CC0-1.0.html https://creativecommons.org/publicdomain/zero/1.0/legalcode'),
  'CDDL-1.0': ('Common Development and Distribution License 1.0', 'https://spdx.org/licenses/CDDL-1.0.html https://opensource.org/licenses/cddl1'),
  'CDDL-1.1': ('Common Development and Distribution License 1.1', 'https://spdx.org/licenses/CDDL-1.1.html http://glassfish.java.net/public/CDDL+GPL_1_1.html https://javaee.github.io/glassfish/LICENSE'),
  'CDL-1.0': ('Common Documentation License 1.0', 'https://spdx.org/licenses/CDL-1.0.html http://www.opensource.apple.com/cdl/ https://fedoraproject.org/wiki/Licensing/Common_Documentation_License https://www.gnu.org/licenses/license-list.html#ACDL'),
  'CDLA-Permissive-1.0': ('Community Data License Agreement Permissive 1.0', 'https://spdx.org/licenses/CDLA-Permissive-1.0.html https://cdla.io/permissive-1-0'),
  'CDLA-Permissive-2.0': ('Community Data License Agreement Permissive 2.0', 'https://spdx.org/licenses/CDLA-Permissive-2.0.html https://cdla.dev/permissive-2-0'),
  'CDLA-Sharing-1.0': ('Community Data License Agreement Sharing 1.0', 'https://spdx.org/licenses/CDLA-Sharing-1.0.html https://cdla.io/sharing-1-0'),
  'CECILL-1.0': ('CeCILL Free Software License Agreement v1.0', 'https://spdx.org/licenses/CECILL-1.0.html http://www.cecill.info/licences/Licence_CeCILL_V1-fr.html'),
  'CECILL-1.1': ('CeCILL Free Software License Agreement v1.1', 'https://spdx.org/licenses/CECILL-1.1.html http://www.cecill.info/licences/Licence_CeCILL_V1.1-US.html'),
  'CECILL-2.0': ('CeCILL Free Software License Agreement v2.0', 'https://spdx.org/licenses/CECILL-2.0.html http://www.cecill.info/licences/Licence_CeCILL_V2-en.html'),
  'CECILL-2.1': ('CeCILL Free Software License Agreement v2.1', 'https://spdx.org/licenses/CECILL-2.1.html http://www.cecill.info/licences/Licence_CeCILL_V2.1-en.html'),
  'CECILL-B': ('CeCILL-B Free Software License Agreement', 'https://spdx.org/licenses/CECILL-B.html http://www.cecill.info/licences/Licence_CeCILL-B_V1-en.html'),
  'CECILL-C': ('CeCILL-C Free Software License Agreement', 'https://spdx.org/licenses/CECILL-C.html http://www.cecill.info/licences/Licence_CeCILL-C_V1-en.html'),
  'CERN-OHL-1.1': ('CERN Open Hardware Licence v1.1', 'https://spdx.org/licenses/CERN-OHL-1.1.html https://www.ohwr.org/project/licenses/wikis/cern-ohl-v1.1'),
  'CERN-OHL-1.2': ('CERN Open Hardware Licence v1.2', 'https://spdx.org/licenses/CERN-OHL-1.2.html https://www.ohwr.org/project/licenses/wikis/cern-ohl-v1.2'),
  'CERN-OHL-P-2.0': ('CERN Open Hardware Licence Version 2 - Permissive', 'https://spdx.org/licenses/CERN-OHL-P-2.0.html https://www.ohwr.org/project/cernohl/wikis/Documents/CERN-OHL-version-2'),
  'CERN-OHL-S-2.0': ('CERN Open Hardware Licence Version 2 - Strongly Reciprocal', 'https://spdx.org/licenses/CERN-OHL-S-2.0.html https://www.ohwr.org/project/cernohl/wikis/Documents/CERN-OHL-version-2'),
  'CERN-OHL-W-2.0': ('CERN Open Hardware Licence Version 2 - Weakly Reciprocal', 'https://spdx.org/licenses/CERN-OHL-W-2.0.html https://www.ohwr.org/project/cernohl/wikis/Documents/CERN-OHL-version-2'),
  'CNRI-Jython': ('CNRI Jython License', 'https://spdx.org/licenses/CNRI-Jython.html http://www.jython.org/license.html'),
  'CNRI-Python': ('CNRI Python License', 'https://spdx.org/licenses/CNRI-Python.html https://opensource.org/licenses/CNRI-Python'),
  'CNRI-Python-GPL-Compatible': ('CNRI Python Open Source GPL Compatible License Agreement', 'https://spdx.org/licenses/CNRI-Python-GPL-Compatible.html http://www.python.org/download/releases/1.6.1/download_win/'),
  'CPAL-1.0': ('Common Public Attribution License 1.0', 'https://spdx.org/licenses/CPAL-1.0.html https://opensource.org/licenses/CPAL-1.0'),
  'CPL-1.0': ('Common Public License 1.0', 'https://spdx.org/licenses/CPL-1.0.html https://opensource.org/licenses/CPL-1.0'),
  'CPOL-1.02': ('Code Project Open License 1.02', 'https://spdx.org/licenses/CPOL-1.02.html http://www.codeproject.com/info/cpol10.aspx'),
  'CUA-OPL-1.0': ('CUA Office Public License v1.0', 'https://spdx.org/licenses/CUA-OPL-1.0.html https://opensource.org/licenses/CUA-OPL-1.0'),
  'Caldera': ('Caldera License', 'https://spdx.org/licenses/Caldera.html http://www.lemis.com/grog/UNIX/ancient-source-all.pdf'),
  'ClArtistic': ('Clarified Artistic License', 'https://spdx.org/licenses/ClArtistic.html http://gianluca.dellavedova.org/2011/01/03/clarified-artistic-license/ http://www.ncftp.com/ncftp/doc/LICENSE.txt'),
  'Condor-1.1': ('Condor Public License v1.1', 'https://spdx.org/licenses/Condor-1.1.html http://research.cs.wisc.edu/condor/license.html#condor http://web.archive.org/web/20111123062036/http://research.cs.wisc.edu/condor/license.html#condor'),
  'Crossword': ('Crossword License', 'https://spdx.org/licenses/Crossword.html https://fedoraproject.org/wiki/Licensing/Crossword'),
  'CrystalStacker': ('CrystalStacker License', 'https://spdx.org/licenses/CrystalStacker.html https://fedoraproject.org/wiki/Licensing:CrystalStacker?rd=Licensing/CrystalStacker'),
  'Cube': ('Cube License', 'https://spdx.org/licenses/Cube.html https://fedoraproject.org/wiki/Licensing/Cube'),
  'D-FSL-1.0': ('Deutsche Freie Software Lizenz', 'https://spdx.org/licenses/D-FSL-1.0.html http://www.dipp.nrw.de/d-fsl/lizenzen/ http://www.dipp.nrw.de/d-fsl/index_html/lizenzen/de/D-FSL-1_0_de.txt http://www.dipp.nrw.de/d-fsl/index_html/lizenzen/en/D-FSL-1_0_en.txt https://www.hbz-nrw.de/produkte/open-access/lizenzen/dfsl https://www.hbz-nrw.de/produkte/open-access/lizenzen/dfsl/deutsche-freie-software-lizenz https://www.hbz-nrw.de/produkte/open-access/lizenzen/dfsl/german-free-software-license https://www.hbz-nrw.de/produkte/open-access/lizenzen/dfsl/D-FSL-1_0_de.txt/at_download/file https://www.hbz-nrw.de/produkte/open-access/lizenzen/dfsl/D-FSL-1_0_en.txt/at_download/file'),
  'DOC': ('DOC License', 'https://spdx.org/licenses/DOC.html http://www.cs.wustl.edu/~schmidt/ACE-copying.html https://www.dre.vanderbilt.edu/~schmidt/ACE-copying.html'),
  'DRL-1.0': ('Detection Rule License 1.0', 'https://spdx.org/licenses/DRL-1.0.html https://github.com/Neo23x0/sigma/blob/master/LICENSE.Detection.Rules.md'),
  'DSDP': ('DSDP License', 'https://spdx.org/licenses/DSDP.html https://fedoraproject.org/wiki/Licensing/DSDP'),
  'Dotseqn': ('Dotseqn License', 'https://spdx.org/licenses/Dotseqn.html https://fedoraproject.org/wiki/Licensing/Dotseqn'),
  'ECL-1.0': ('Educational Community License v1.0', 'https://spdx.org/licenses/ECL-1.0.html https://opensource.org/licenses/ECL-1.0'),
  'ECL-2.0': ('Educational Community License v2.0', 'https://spdx.org/licenses/ECL-2.0.html https://opensource.org/licenses/ECL-2.0'),
  'EFL-1.0': ('Eiffel Forum License v1.0', 'https://spdx.org/licenses/EFL-1.0.html http://www.eiffel-nice.org/license/forum.txt https://opensource.org/licenses/EFL-1.0'),
  'EFL-2.0': ('Eiffel Forum License v2.0', 'https://spdx.org/licenses/EFL-2.0.html http://www.eiffel-nice.org/license/eiffel-forum-license-2.html https://opensource.org/licenses/EFL-2.0'),
  'EPICS': ('EPICS Open License', 'https://spdx.org/licenses/EPICS.html https://epics.anl.gov/license/open.php'),
  'EPL-1.0': ('Eclipse Public License 1.0', 'https://spdx.org/licenses/EPL-1.0.html http://www.eclipse.org/legal/epl-v10.html https://opensource.org/licenses/EPL-1.0'),
  'EPL-2.0': ('Eclipse Public License 2.0', 'https://spdx.org/licenses/EPL-2.0.html https://www.eclipse.org/legal/epl-2.0 https://www.opensource.org/licenses/EPL-2.0'),
  'EUDatagrid': ('EU DataGrid Software License', 'https://spdx.org/licenses/EUDatagrid.html http://eu-datagrid.web.cern.ch/eu-datagrid/license.html https://opensource.org/licenses/EUDatagrid'),
  'EUPL-1.0': ('European Union Public License 1.0', 'https://spdx.org/licenses/EUPL-1.0.html http://ec.europa.eu/idabc/en/document/7330.html http://ec.europa.eu/idabc/servlets/Doc027f.pdf?id=31096'),
  'EUPL-1.1': ('European Union Public License 1.1', 'https://spdx.org/licenses/EUPL-1.1.html https://joinup.ec.europa.eu/software/page/eupl/licence-eupl https://joinup.ec.europa.eu/sites/default/files/custom-page/attachment/eupl1.1.-licence-en_0.pdf https://opensource.org/licenses/EUPL-1.1'),
  'EUPL-1.2': ('European Union Public License 1.2', 'https://spdx.org/licenses/EUPL-1.2.html https://joinup.ec.europa.eu/page/eupl-text-11-12 https://joinup.ec.europa.eu/sites/default/files/custom-page/attachment/eupl_v1.2_en.pdf https://joinup.ec.europa.eu/sites/default/files/custom-page/attachment/2020-03/EUPL-1.2%20EN.txt https://joinup.ec.europa.eu/sites/default/files/inline-files/EUPL%20v1_2%20EN(1).txt http://eur-lex.europa.eu/legal-content/EN/TXT/HTML/?uri=CELEX:32017D0863 https://opensource.org/licenses/EUPL-1.2'),
  'Entessa': ('Entessa Public License v1.0', 'https://spdx.org/licenses/Entessa.html https://opensource.org/licenses/Entessa'),
  'ErlPL-1.1': ('Erlang Public License v1.1', 'https://spdx.org/licenses/ErlPL-1.1.html http://www.erlang.org/EPLICENSE'),
  'Eurosym': ('Eurosym License', 'https://spdx.org/licenses/Eurosym.html https://fedoraproject.org/wiki/Licensing/Eurosym'),
  'FSFAP': ('FSF All Permissive License', 'https://spdx.org/licenses/FSFAP.html https://www.gnu.org/prep/maintain/html_node/License-Notices-for-Other-Files.html'),
  'FSFUL': ('FSF Unlimited License', 'https://spdx.org/licenses/FSFUL.html https://fedoraproject.org/wiki/Licensing/FSF_Unlimited_License'),
  'FSFULLR': ('FSF Unlimited License (with License Retention)', 'https://spdx.org/licenses/FSFULLR.html https://fedoraproject.org/wiki/Licensing/FSF_Unlimited_License#License_Retention_Variant'),
  'FTL': ('Freetype Project License', 'https://spdx.org/licenses/FTL.html http://freetype.fis.uniroma2.it/FTL.TXT http://git.savannah.gnu.org/cgit/freetype/freetype2.git/tree/docs/FTL.TXT http://gitlab.freedesktop.org/freetype/freetype/-/raw/master/docs/FTL.TXT'),
  'Fair': ('Fair License', 'https://spdx.org/licenses/Fair.html http://fairlicense.org/ https://opensource.org/licenses/Fair'),
  'Frameworx-1.0': ('Frameworx Open License 1.0', 'https://spdx.org/licenses/Frameworx-1.0.html https://opensource.org/licenses/Frameworx-1.0'),
  'FreeBSD-DOC': ('FreeBSD Documentation License', 'https://spdx.org/licenses/FreeBSD-DOC.html https://www.freebsd.org/copyright/freebsd-doc-license/'),
  'FreeImage': ('FreeImage Public License v1.0', 'https://spdx.org/licenses/FreeImage.html http://freeimage.sourceforge.net/freeimage-license.txt'),
  'GD': ('GD License', 'https://spdx.org/licenses/GD.html https://libgd.github.io/manuals/2.3.0/files/license-txt.html'),
  'GFDL-1.1': ('GNU Free Documentation License v1.1', 'https://spdx.org/licenses/GFDL-1.1.html https://www.gnu.org/licenses/old-licenses/fdl-1.1.txt'),
  'GFDL-1.1-invariants-only': ('GNU Free Documentation License v1.1 only - invariants', 'https://spdx.org/licenses/GFDL-1.1-invariants-only.html https://www.gnu.org/licenses/old-licenses/fdl-1.1.txt'),
  'GFDL-1.1-invariants-or-later': ('GNU Free Documentation License v1.1 or later - invariants', 'https://spdx.org/licenses/GFDL-1.1-invariants-or-later.html https://www.gnu.org/licenses/old-licenses/fdl-1.1.txt'),
  'GFDL-1.1-no-invariants-only': ('GNU Free Documentation License v1.1 only - no invariants', 'https://spdx.org/licenses/GFDL-1.1-no-invariants-only.html https://www.gnu.org/licenses/old-licenses/fdl-1.1.txt'),
  'GFDL-1.1-no-invariants-or-later': ('GNU Free Documentation License v1.1 or later - no invariants', 'https://spdx.org/licenses/GFDL-1.1-no-invariants-or-later.html https://www.gnu.org/licenses/old-licenses/fdl-1.1.txt'),
  'GFDL-1.1-only': ('GNU Free Documentation License v1.1 only', 'https://spdx.org/licenses/GFDL-1.1-only.html https://www.gnu.org/licenses/old-licenses/fdl-1.1.txt'),
  'GFDL-1.1-or-later': ('GNU Free Documentation License v1.1 or later', 'https://spdx.org/licenses/GFDL-1.1-or-later.html https://www.gnu.org/licenses/old-licenses/fdl-1.1.txt'),
  'GFDL-1.2': ('GNU Free Documentation License v1.2', 'https://spdx.org/licenses/GFDL-1.2.html https://www.gnu.org/licenses/old-licenses/fdl-1.2.txt'),
  'GFDL-1.2-invariants-only': ('GNU Free Documentation License v1.2 only - invariants', 'https://spdx.org/licenses/GFDL-1.2-invariants-only.html https://www.gnu.org/licenses/old-licenses/fdl-1.2.txt'),
  'GFDL-1.2-invariants-or-later': ('GNU Free Documentation License v1.2 or later - invariants', 'https://spdx.org/licenses/GFDL-1.2-invariants-or-later.html https://www.gnu.org/licenses/old-licenses/fdl-1.2.txt'),
  'GFDL-1.2-no-invariants-only': ('GNU Free Documentation License v1.2 only - no invariants', 'https://spdx.org/licenses/GFDL-1.2-no-invariants-only.html https://www.gnu.org/licenses/old-licenses/fdl-1.2.txt'),
  'GFDL-1.2-no-invariants-or-later': ('GNU Free Documentation License v1.2 or later - no invariants', 'https://spdx.org/licenses/GFDL-1.2-no-invariants-or-later.html https://www.gnu.org/licenses/old-licenses/fdl-1.2.txt'),
  'GFDL-1.2-only': ('GNU Free Documentation License v1.2 only', 'https://spdx.org/licenses/GFDL-1.2-only.html https://www.gnu.org/licenses/old-licenses/fdl-1.2.txt'),
  'GFDL-1.2-or-later': ('GNU Free Documentation License v1.2 or later', 'https://spdx.org/licenses/GFDL-1.2-or-later.html https://www.gnu.org/licenses/old-licenses/fdl-1.2.txt'),
  'GFDL-1.3': ('GNU Free Documentation License v1.3', 'https://spdx.org/licenses/GFDL-1.3.html https://www.gnu.org/licenses/fdl-1.3.txt'),
  'GFDL-1.3-invariants-only': ('GNU Free Documentation License v1.3 only - invariants', 'https://spdx.org/licenses/GFDL-1.3-invariants-only.html https://www.gnu.org/licenses/fdl-1.3.txt'),
  'GFDL-1.3-invariants-or-later': ('GNU Free Documentation License v1.3 or later - invariants', 'https://spdx.org/licenses/GFDL-1.3-invariants-or-later.html https://www.gnu.org/licenses/fdl-1.3.txt'),
  'GFDL-1.3-no-invariants-only': ('GNU Free Documentation License v1.3 only - no invariants', 'https://spdx.org/licenses/GFDL-1.3-no-invariants-only.html https://www.gnu.org/licenses/fdl-1.3.txt'),
  'GFDL-1.3-no-invariants-or-later': ('GNU Free Documentation License v1.3 or later - no invariants', 'https://spdx.org/licenses/GFDL-1.3-no-invariants-or-later.html https://www.gnu.org/licenses/fdl-1.3.txt'),
  'GFDL-1.3-only': ('GNU Free Documentation License v1.3 only', 'https://spdx.org/licenses/GFDL-1.3-only.html https://www.gnu.org/licenses/fdl-1.3.txt'),
  'GFDL-1.3-or-later': ('GNU Free Documentation License v1.3 or later', 'https://spdx.org/licenses/GFDL-1.3-or-later.html https://www.gnu.org/licenses/fdl-1.3.txt'),
  'GL2PS': ('GL2PS License', 'https://spdx.org/licenses/GL2PS.html http://www.geuz.org/gl2ps/COPYING.GL2PS'),
  'GLWTPL': ('Good Luck With That Public License', 'https://spdx.org/licenses/GLWTPL.html https://github.com/me-shaon/GLWTPL/commit/da5f6bc734095efbacb442c0b31e33a65b9d6e85'),
  'GPL-1.0': ('GNU General Public License v1.0 only', 'https://spdx.org/licenses/GPL-1.0.html https://www.gnu.org/licenses/old-licenses/gpl-1.0-standalone.html'),
  'GPL-1.0+': ('GNU General Public License v1.0 or later', 'https://spdx.org/licenses/GPL-1.0+.html https://www.gnu.org/licenses/old-licenses/gpl-1.0-standalone.html'),
  'GPL-1.0-only': ('GNU General Public License v1.0 only', 'https://spdx.org/licenses/GPL-1.0-only.html https://www.gnu.org/licenses/old-licenses/gpl-1.0-standalone.html'),
  'GPL-1.0-or-later': ('GNU General Public License v1.0 or later', 'https://spdx.org/licenses/GPL-1.0-or-later.html https://www.gnu.org/licenses/old-licenses/gpl-1.0-standalone.html'),
  'GPL-2.0': ('GNU General Public License v2.0 only', 'https://spdx.org/licenses/GPL-2.0.html https://www.gnu.org/licenses/old-licenses/gpl-2.0-standalone.html https://opensource.org/licenses/GPL-2.0'),
  'GPL-2.0+': ('GNU General Public License v2.0 or later', 'https://spdx.org/licenses/GPL-2.0+.html https://www.gnu.org/licenses/old-licenses/gpl-2.0-standalone.html https://opensource.org/licenses/GPL-2.0'),
  'GPL-2.0-only': ('GNU General Public License v2.0 only', 'https://spdx.org/licenses/GPL-2.0-only.html https://www.gnu.org/licenses/old-licenses/gpl-2.0-standalone.html https://opensource.org/licenses/GPL-2.0'),
  'GPL-2.0-or-later': ('GNU General Public License v2.0 or later', 'https://spdx.org/licenses/GPL-2.0-or-later.html https://www.gnu.org/licenses/old-licenses/gpl-2.0-standalone.html https://opensource.org/licenses/GPL-2.0'),
  'GPL-2.0-with-GCC-exception': ('GNU General Public License v2.0 w/GCC Runtime Library exception', 'https://spdx.org/licenses/GPL-2.0-with-GCC-exception.html https://gcc.gnu.org/git/?p=gcc.git;a=blob;f=gcc/libgcc1.c;h=762f5143fc6eed57b6797c82710f3538aa52b40b;hb=cb143a3ce4fb417c68f5fa2691a1b1b1053dfba9#l10'),
  'GPL-2.0-with-autoconf-exception': ('GNU General Public License v2.0 w/Autoconf exception', 'https://spdx.org/licenses/GPL-2.0-with-autoconf-exception.html http://ac-archive.sourceforge.net/doc/copyright.html'),
  'GPL-2.0-with-bison-exception': ('GNU General Public License v2.0 w/Bison exception', 'https://spdx.org/licenses/GPL-2.0-with-bison-exception.html http://git.savannah.gnu.org/cgit/bison.git/tree/data/yacc.c?id=193d7c7054ba7197b0789e14965b739162319b5e#n141'),
  'GPL-2.0-with-classpath-exception': ('GNU General Public License v2.0 w/Classpath exception', 'https://spdx.org/licenses/GPL-2.0-with-classpath-exception.html https://www.gnu.org/software/classpath/license.html'),
  'GPL-2.0-with-font-exception': ('GNU General Public License v2.0 w/Font exception', 'https://spdx.org/licenses/GPL-2.0-with-font-exception.html https://www.gnu.org/licenses/gpl-faq.html#FontException'),
  'GPL-3.0': ('GNU General Public License v3.0 only', 'https://spdx.org/licenses/GPL-3.0.html https://www.gnu.org/licenses/gpl-3.0-standalone.html https://opensource.org/licenses/GPL-3.0'),
  'GPL-3.0+': ('GNU General Public License v3.0 or later', 'https://spdx.org/licenses/GPL-3.0+.html https://www.gnu.org/licenses/gpl-3.0-standalone.html https://opensource.org/licenses/GPL-3.0'),
  'GPL-3.0-only': ('GNU General Public License v3.0 only', 'https://spdx.org/licenses/GPL-3.0-only.html https://www.gnu.org/licenses/gpl-3.0-standalone.html https://opensource.org/licenses/GPL-3.0'),
  'GPL-3.0-or-later': ('GNU General Public License v3.0 or later', 'https://spdx.org/licenses/GPL-3.0-or-later.html https://www.gnu.org/licenses/gpl-3.0-standalone.html https://opensource.org/licenses/GPL-3.0'),
  'GPL-3.0-with-GCC-exception': ('GNU General Public License v3.0 w/GCC Runtime Library exception', 'https://spdx.org/licenses/GPL-3.0-with-GCC-exception.html https://www.gnu.org/licenses/gcc-exception-3.1.html'),
  'GPL-3.0-with-autoconf-exception': ('GNU General Public License v3.0 w/Autoconf exception', 'https://spdx.org/licenses/GPL-3.0-with-autoconf-exception.html https://www.gnu.org/licenses/autoconf-exception-3.0.html'),
  'Giftware': ('Giftware License', 'https://spdx.org/licenses/Giftware.html http://liballeg.org/license.html#allegro-4-the-giftware-license'),
  'Glide': ('3dfx Glide License', 'https://spdx.org/licenses/Glide.html http://www.users.on.net/~triforce/glidexp/COPYING.txt'),
  'Glulxe': ('Glulxe License', 'https://spdx.org/licenses/Glulxe.html https://fedoraproject.org/wiki/Licensing/Glulxe'),
  'HPND': ('Historical Permission Notice and Disclaimer', 'https://spdx.org/licenses/HPND.html https://opensource.org/licenses/HPND'),
  'HPND-sell-variant': ('Historical Permission Notice and Disclaimer - sell variant', 'https://spdx.org/licenses/HPND-sell-variant.html https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/net/sunrpc/auth_gss/gss_generic_token.c?h=v4.19'),
  'HTMLTIDY': ('HTML Tidy License', 'https://spdx.org/licenses/HTMLTIDY.html https://github.com/htacg/tidy-html5/blob/next/README/LICENSE.md'),
  'HaskellReport': ('Haskell Language Report License', 'https://spdx.org/licenses/HaskellReport.html https://fedoraproject.org/wiki/Licensing/Haskell_Language_Report_License'),
  'Hippocratic-2.1': ('Hippocratic License 2.1', 'https://spdx.org/licenses/Hippocratic-2.1.html https://firstdonoharm.dev/version/2/1/license.html https://github.com/EthicalSource/hippocratic-license/blob/58c0e646d64ff6fbee275bfe2b9492f914e3ab2a/LICENSE.txt'),
  'IBM-pibs': ('IBM PowerPC Initialization and Boot Software', 'https://spdx.org/licenses/IBM-pibs.html http://git.denx.de/?p=u-boot.git;a=blob;f=arch/powerpc/cpu/ppc4xx/miiphy.c;h=297155fdafa064b955e53e9832de93bfb0cfb85b;hb=9fab4bf4cc077c21e43941866f3f2c196f28670d'),
  'ICU': ('ICU License', 'https://spdx.org/licenses/ICU.html http://source.icu-project.org/repos/icu/icu/trunk/license.html'),
  'IJG': ('Independent JPEG Group License', 'https://spdx.org/licenses/IJG.html http://dev.w3.org/cvsweb/Amaya/libjpeg/Attic/README?rev=1.2'),
  'IPA': ('IPA Font License', 'https://spdx.org/licenses/IPA.html https://opensource.org/licenses/IPA'),
  'IPL-1.0': ('IBM Public License v1.0', 'https://spdx.org/licenses/IPL-1.0.html https://opensource.org/licenses/IPL-1.0'),
  'ISC': ('ISC License', 'https://spdx.org/licenses/ISC.html https://www.isc.org/licenses/ https://www.isc.org/downloads/software-support-policy/isc-license/ https://opensource.org/licenses/ISC'),
  'ImageMagick': ('ImageMagick License', 'https://spdx.org/licenses/ImageMagick.html http://www.imagemagick.org/script/license.php'),
  'Imlib2': ('Imlib2 License', 'https://spdx.org/licenses/Imlib2.html http://trac.enlightenment.org/e/browser/trunk/imlib2/COPYING https://git.enlightenment.org/legacy/imlib2.git/tree/COPYING'),
  'Info-ZIP': ('Info-ZIP License', 'https://spdx.org/licenses/Info-ZIP.html http://www.info-zip.org/license.html'),
  'Intel': ('Intel Open Source License', 'https://spdx.org/licenses/Intel.html https://opensource.org/licenses/Intel'),
  'Intel-ACPI': ('Intel ACPI Software License Agreement', 'https://spdx.org/licenses/Intel-ACPI.html https://fedoraproject.org/wiki/Licensing/Intel_ACPI_Software_License_Agreement'),
  'Interbase-1.0': ('Interbase Public License v1.0', 'https://spdx.org/licenses/Interbase-1.0.html https://web.archive.org/web/20060319014854/http://info.borland.com/devsupport/interbase/opensource/IPL.html'),
  'JPNIC': ('Japan Network Information Center License', 'https://spdx.org/licenses/JPNIC.html https://gitlab.isc.org/isc-projects/bind9/blob/master/COPYRIGHT#L366'),
  'JSON': ('JSON License', 'https://spdx.org/licenses/JSON.html http://www.json.org/license.html'),
  'JasPer-2.0': ('JasPer License', 'https://spdx.org/licenses/JasPer-2.0.html http://www.ece.uvic.ca/~mdadams/jasper/LICENSE'),
  'LAL-1.2': ('Licence Art Libre 1.2', 'https://spdx.org/licenses/LAL-1.2.html http://artlibre.org/licence/lal/licence-art-libre-12/'),
  'LAL-1.3': ('Licence Art Libre 1.3', 'https://spdx.org/licenses/LAL-1.3.html https://artlibre.org/'),
  'LGPL-2.0': ('GNU Library General Public License v2 only', 'https://spdx.org/licenses/LGPL-2.0.html https://www.gnu.org/licenses/old-licenses/lgpl-2.0-standalone.html'),
  'LGPL-2.0+': ('GNU Library General Public License v2 or later', 'https://spdx.org/licenses/LGPL-2.0+.html https://www.gnu.org/licenses/old-licenses/lgpl-2.0-standalone.html'),
  'LGPL-2.0-only': ('GNU Library General Public License v2 only', 'https://spdx.org/licenses/LGPL-2.0-only.html https://www.gnu.org/licenses/old-licenses/lgpl-2.0-standalone.html'),
  'LGPL-2.0-or-later': ('GNU Library General Public License v2 or later', 'https://spdx.org/licenses/LGPL-2.0-or-later.html https://www.gnu.org/licenses/old-licenses/lgpl-2.0-standalone.html'),
  'LGPL-2.1': ('GNU Lesser General Public License v2.1 only', 'https://spdx.org/licenses/LGPL-2.1.html https://www.gnu.org/licenses/old-licenses/lgpl-2.1-standalone.html https://opensource.org/licenses/LGPL-2.1'),
  'LGPL-2.1+': ('GNU Library General Public License v2.1 or later', 'https://spdx.org/licenses/LGPL-2.1+.html https://www.gnu.org/licenses/old-licenses/lgpl-2.1-standalone.html https://opensource.org/licenses/LGPL-2.1'),
  'LGPL-2.1-only': ('GNU Lesser General Public License v2.1 only', 'https://spdx.org/licenses/LGPL-2.1-only.html https://www.gnu.org/licenses/old-licenses/lgpl-2.1-standalone.html https://opensource.org/licenses/LGPL-2.1'),
  'LGPL-2.1-or-later': ('GNU Lesser General Public License v2.1 or later', 'https://spdx.org/licenses/LGPL-2.1-or-later.html https://www.gnu.org/licenses/old-licenses/lgpl-2.1-standalone.html https://opensource.org/licenses/LGPL-2.1'),
  'LGPL-3.0': ('GNU Lesser General Public License v3.0 only', 'https://spdx.org/licenses/LGPL-3.0.html https://www.gnu.org/licenses/lgpl-3.0-standalone.html https://opensource.org/licenses/LGPL-3.0'),
  'LGPL-3.0+': ('GNU Lesser General Public License v3.0 or later', 'https://spdx.org/licenses/LGPL-3.0+.html https://www.gnu.org/licenses/lgpl-3.0-standalone.html https://opensource.org/licenses/LGPL-3.0'),
  'LGPL-3.0-only': ('GNU Lesser General Public License v3.0 only', 'https://spdx.org/licenses/LGPL-3.0-only.html https://www.gnu.org/licenses/lgpl-3.0-standalone.html https://opensource.org/licenses/LGPL-3.0'),
  'LGPL-3.0-or-later': ('GNU Lesser General Public License v3.0 or later', 'https://spdx.org/licenses/LGPL-3.0-or-later.html https://www.gnu.org/licenses/lgpl-3.0-standalone.html https://opensource.org/licenses/LGPL-3.0'),
  'LGPLLR': ('Lesser General Public License For Linguistic Resources', 'https://spdx.org/licenses/LGPLLR.html http://www-igm.univ-mlv.fr/~unitex/lgpllr.html'),
  'LPL-1.0': ('Lucent Public License Version 1.0', 'https://spdx.org/licenses/LPL-1.0.html https://opensource.org/licenses/LPL-1.0'),
  'LPL-1.02': ('Lucent Public License v1.02', 'https://spdx.org/licenses/LPL-1.02.html http://plan9.bell-labs.com/plan9/license.html https://opensource.org/licenses/LPL-1.02'),
  'LPPL-1.0': ('LaTeX Project Public License v1.0', 'https://spdx.org/licenses/LPPL-1.0.html http://www.latex-project.org/lppl/lppl-1-0.txt'),
  'LPPL-1.1': ('LaTeX Project Public License v1.1', 'https://spdx.org/licenses/LPPL-1.1.html http://www.latex-project.org/lppl/lppl-1-1.txt'),
  'LPPL-1.2': ('LaTeX Project Public License v1.2', 'https://spdx.org/licenses/LPPL-1.2.html http://www.latex-project.org/lppl/lppl-1-2.txt'),
  'LPPL-1.3a': ('LaTeX Project Public License v1.3a', 'https://spdx.org/licenses/LPPL-1.3a.html http://www.latex-project.org/lppl/lppl-1-3a.txt'),
  'LPPL-1.3c': ('LaTeX Project Public License v1.3c', 'https://spdx.org/licenses/LPPL-1.3c.html http://www.latex-project.org/lppl/lppl-1-3c.txt https://opensource.org/licenses/LPPL-1.3c'),
  'Latex2e': ('Latex2e License', 'https://spdx.org/licenses/Latex2e.html https://fedoraproject.org/wiki/Licensing/Latex2e'),
  'Leptonica': ('Leptonica License', 'https://spdx.org/licenses/Leptonica.html https://fedoraproject.org/wiki/Licensing/Leptonica'),
  'LiLiQ-P-1.1': ('Licence Libre du Québec – Permissive version 1.1', 'https://spdx.org/licenses/LiLiQ-P-1.1.html https://forge.gouv.qc.ca/licence/fr/liliq-v1-1/ http://opensource.org/licenses/LiLiQ-P-1.1'),
  'LiLiQ-R-1.1': ('Licence Libre du Québec – Réciprocité version 1.1', 'https://spdx.org/licenses/LiLiQ-R-1.1.html https://www.forge.gouv.qc.ca/participez/licence-logicielle/licence-libre-du-quebec-liliq-en-francais/licence-libre-du-quebec-reciprocite-liliq-r-v1-1/ http://opensource.org/licenses/LiLiQ-R-1.1'),
  'LiLiQ-Rplus-1.1': ('Licence Libre du Québec – Réciprocité forte version 1.1', 'https://spdx.org/licenses/LiLiQ-Rplus-1.1.html https://www.forge.gouv.qc.ca/participez/licence-logicielle/licence-libre-du-quebec-liliq-en-francais/licence-libre-du-quebec-reciprocite-forte-liliq-r-v1-1/ http://opensource.org/licenses/LiLiQ-Rplus-1.1'),
  'Libpng': ('libpng License', 'https://spdx.org/licenses/Libpng.html http://www.libpng.org/pub/png/src/libpng-LICENSE.txt'),
  'Linux-OpenIB': ('Linux Kernel Variant of OpenIB.org license', 'https://spdx.org/licenses/Linux-OpenIB.html https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/infiniband/core/sa.h'),
  'MIT': ('MIT License', 'https://spdx.org/licenses/MIT.html https://opensource.org/licenses/MIT'),
  'MIT-0': ('MIT No Attribution', 'https://spdx.org/licenses/MIT-0.html https://github.com/aws/mit-0 https://romanrm.net/mit-zero https://github.com/awsdocs/aws-cloud9-user-guide/blob/master/LICENSE-SAMPLECODE'),
  'MIT-CMU': ('CMU License', 'https://spdx.org/licenses/MIT-CMU.html https://fedoraproject.org/wiki/Licensing:MIT?rd=Licensing/MIT#CMU_Style https://github.com/python-pillow/Pillow/blob/fffb426092c8db24a5f4b6df243a8a3c01fb63cd/LICENSE'),
  'MIT-Modern-Variant': ('MIT License Modern Variant', 'https://spdx.org/licenses/MIT-Modern-Variant.html https://fedoraproject.org/wiki/Licensing:MIT#Modern_Variants https://ptolemy.berkeley.edu/copyright.htm https://pirlwww.lpl.arizona.edu/resources/guide/software/PerlTk/Tixlic.html'),
  'MIT-advertising': ('Enlightenment License (e16)', 'https://spdx.org/licenses/MIT-advertising.html https://fedoraproject.org/wiki/Licensing/MIT_With_Advertising'),
  'MIT-enna': ('enna License', 'https://spdx.org/licenses/MIT-enna.html https://fedoraproject.org/wiki/Licensing/MIT#enna'),
  'MIT-feh': ('feh License', 'https://spdx.org/licenses/MIT-feh.html https://fedoraproject.org/wiki/Licensing/MIT#feh'),
  'MIT-open-group': ('MIT Open Group variant', 'https://spdx.org/licenses/MIT-open-group.html https://gitlab.freedesktop.org/xorg/app/iceauth/-/blob/master/COPYING https://gitlab.freedesktop.org/xorg/app/xvinfo/-/blob/master/COPYING https://gitlab.freedesktop.org/xorg/app/xsetroot/-/blob/master/COPYING https://gitlab.freedesktop.org/xorg/app/xauth/-/blob/master/COPYING'),
  'MITNFA': ('MIT +no-false-attribs license', 'https://spdx.org/licenses/MITNFA.html https://fedoraproject.org/wiki/Licensing/MITNFA'),
  'MPL-1.0': ('Mozilla Public License 1.0', 'https://spdx.org/licenses/MPL-1.0.html http://www.mozilla.org/MPL/MPL-1.0.html https://opensource.org/licenses/MPL-1.0'),
  'MPL-1.1': ('Mozilla Public License 1.1', 'https://spdx.org/licenses/MPL-1.1.html http://www.mozilla.org/MPL/MPL-1.1.html https://opensource.org/licenses/MPL-1.1'),
  'MPL-2.0': ('Mozilla Public License 2.0', 'https://spdx.org/licenses/MPL-2.0.html https://www.mozilla.org/MPL/2.0/ https://opensource.org/licenses/MPL-2.0'),
  'MPL-2.0-no-copyleft-exception': ('Mozilla Public License 2.0 (no copyleft exception)', 'https://spdx.org/licenses/MPL-2.0-no-copyleft-exception.html http://www.mozilla.org/MPL/2.0/ https://opensource.org/licenses/MPL-2.0'),
  'MS-PL': ('Microsoft Public License', 'https://spdx.org/licenses/MS-PL.html http://www.microsoft.com/opensource/licenses.mspx https://opensource.org/licenses/MS-PL'),
  'MS-RL': ('Microsoft Reciprocal License', 'https://spdx.org/licenses/MS-RL.html http://www.microsoft.com/opensource/licenses.mspx https://opensource.org/licenses/MS-RL'),
  'MTLL': ('Matrix Template Library License', 'https://spdx.org/licenses/MTLL.html https://fedoraproject.org/wiki/Licensing/Matrix_Template_Library_License'),
  'MakeIndex': ('MakeIndex License', 'https://spdx.org/licenses/MakeIndex.html https://fedoraproject.org/wiki/Licensing/MakeIndex'),
  'MirOS': ('The MirOS Licence', 'https://spdx.org/licenses/MirOS.html https://opensource.org/licenses/MirOS'),
  'Motosoto': ('Motosoto License', 'https://spdx.org/licenses/Motosoto.html https://opensource.org/licenses/Motosoto'),
  'MulanPSL-1.0': ('Mulan Permissive Software License, Version 1', 'https://spdx.org/licenses/MulanPSL-1.0.html https://license.coscl.org.cn/MulanPSL/ https://github.com/yuwenlong/longphp/blob/25dfb70cc2a466dc4bb55ba30901cbce08d164b5/LICENSE'),
  'MulanPSL-2.0': ('Mulan Permissive Software License, Version 2', 'https://spdx.org/licenses/MulanPSL-2.0.html https://license.coscl.org.cn/MulanPSL2/'),
  'Multics': ('Multics License', 'https://spdx.org/licenses/Multics.html https://opensource.org/licenses/Multics'),
  'Mup': ('Mup License', 'https://spdx.org/licenses/Mup.html https://fedoraproject.org/wiki/Licensing/Mup'),
  'NAIST-2003': ('Nara Institute of Science and Technology License (2003)', 'https://spdx.org/licenses/NAIST-2003.html https://enterprise.dejacode.com/licenses/public/naist-2003/#license-text https://github.com/nodejs/node/blob/4a19cc8947b1bba2b2d27816ec3d0edf9b28e503/LICENSE#L343'),
  'NASA-1.3': ('NASA Open Source Agreement 1.3', 'https://spdx.org/licenses/NASA-1.3.html http://ti.arc.nasa.gov/opensource/nosa/ https://opensource.org/licenses/NASA-1.3'),
  'NBPL-1.0': ('Net Boolean Public License v1', 'https://spdx.org/licenses/NBPL-1.0.html http://www.openldap.org/devel/gitweb.cgi?p=openldap.git;a=blob;f=LICENSE;hb=37b4b3f6cc4bf34e1d3dec61e69914b9819d8894'),
  'NCGL-UK-2.0': ('Non-Commercial Government Licence', 'https://spdx.org/licenses/NCGL-UK-2.0.html http://www.nationalarchives.gov.uk/doc/non-commercial-government-licence/version/2/'),
  'NCSA': ('University of Illinois/NCSA Open Source License', 'https://spdx.org/licenses/NCSA.html http://otm.illinois.edu/uiuc_openSource https://opensource.org/licenses/NCSA'),
  'NGPL': ('Nethack General Public License', 'https://spdx.org/licenses/NGPL.html https://opensource.org/licenses/NGPL'),
  'NIST-PD': ('NIST Public Domain Notice', 'https://spdx.org/licenses/NIST-PD.html https://github.com/tcheneau/simpleRPL/blob/e645e69e38dd4e3ccfeceb2db8cba05b7c2e0cd3/LICENSE.txt https://github.com/tcheneau/Routing/blob/f09f46fcfe636107f22f2c98348188a65a135d98/README.md'),
  'NIST-PD-fallback': ('NIST Public Domain Notice with license fallback', 'https://spdx.org/licenses/NIST-PD-fallback.html https://github.com/usnistgov/jsip/blob/59700e6926cbe96c5cdae897d9a7d2656b42abe3/LICENSE https://github.com/usnistgov/fipy/blob/86aaa5c2ba2c6f1be19593c5986071cf6568cc34/LICENSE.rst'),
  'NLOD-1.0': ('Norwegian Licence for Open Government Data (NLOD) 1.0', 'https://spdx.org/licenses/NLOD-1.0.html http://data.norge.no/nlod/en/1.0'),
  'NLOD-2.0': ('Norwegian Licence for Open Government Data (NLOD) 2.0', 'https://spdx.org/licenses/NLOD-2.0.html http://data.norge.no/nlod/en/2.0'),
  'NLPL': ('No Limit Public License', 'https://spdx.org/licenses/NLPL.html https://fedoraproject.org/wiki/Licensing/NLPL'),
  'NOSL': ('Netizen Open Source License', 'https://spdx.org/licenses/NOSL.html http://bits.netizen.com.au/licenses/NOSL/nosl.txt'),
  'NPL-1.0': ('Netscape Public License v1.0', 'https://spdx.org/licenses/NPL-1.0.html http://www.mozilla.org/MPL/NPL/1.0/'),
  'NPL-1.1': ('Netscape Public License v1.1', 'https://spdx.org/licenses/NPL-1.1.html http://www.mozilla.org/MPL/NPL/1.1/'),
  'NPOSL-3.0': ('Non-Profit Open Software License 3.0', 'https://spdx.org/licenses/NPOSL-3.0.html https://opensource.org/licenses/NOSL3.0'),
  'NRL': ('NRL License', 'https://spdx.org/licenses/NRL.html http://web.mit.edu/network/isakmp/nrllicense.html'),
  'NTP': ('NTP License', 'https://spdx.org/licenses/NTP.html https://opensource.org/licenses/NTP'),
  'NTP-0': ('NTP No Attribution', 'https://spdx.org/licenses/NTP-0.html https://github.com/tytso/e2fsprogs/blob/master/lib/et/et_name.c'),
  'Naumen': ('Naumen Public License', 'https://spdx.org/licenses/Naumen.html https://opensource.org/licenses/Naumen'),
  'Net-SNMP': ('Net-SNMP License', 'https://spdx.org/licenses/Net-SNMP.html http://net-snmp.sourceforge.net/about/license.html'),
  'NetCDF': ('NetCDF license', 'https://spdx.org/licenses/NetCDF.html http://www.unidata.ucar.edu/software/netcdf/copyright.html'),
  'Newsletr': ('Newsletr License', 'https://spdx.org/licenses/Newsletr.html https://fedoraproject.org/wiki/Licensing/Newsletr'),
  'Nokia': ('Nokia Open Source License', 'https://spdx.org/licenses/Nokia.html https://opensource.org/licenses/nokia'),
  'Noweb': ('Noweb License', 'https://spdx.org/licenses/Noweb.html https://fedoraproject.org/wiki/Licensing/Noweb'),
  'Nunit': ('Nunit License', 'https://spdx.org/licenses/Nunit.html https://fedoraproject.org/wiki/Licensing/Nunit'),
  'O-UDA-1.0': ('Open Use of Data Agreement v1.0', 'https://spdx.org/licenses/O-UDA-1.0.html https://github.com/microsoft/Open-Use-of-Data-Agreement/blob/v1.0/O-UDA-1.0.md https://cdla.dev/open-use-of-data-agreement-v1-0/'),
  'OCCT-PL': ('Open CASCADE Technology Public License', 'https://spdx.org/licenses/OCCT-PL.html http://www.opencascade.com/content/occt-public-license'),
  'OCLC-2.0': ('OCLC Research Public License 2.0', 'https://spdx.org/licenses/OCLC-2.0.html http://www.oclc.org/research/activities/software/license/v2final.htm https://opensource.org/licenses/OCLC-2.0'),
  'ODC-By-1.0': ('Open Data Commons Attribution License v1.0', 'https://spdx.org/licenses/ODC-By-1.0.html https://opendatacommons.org/licenses/by/1.0/'),
  'ODbL-1.0': ('Open Data Commons Open Database License v1.0', 'https://spdx.org/licenses/ODbL-1.0.html http://www.opendatacommons.org/licenses/odbl/1.0/ https://opendatacommons.org/licenses/odbl/1-0/'),
  'OFL-1.0': ('SIL Open Font License 1.0', 'https://spdx.org/licenses/OFL-1.0.html http://scripts.sil.org/cms/scripts/page.php?item_id=OFL10_web'),
  'OFL-1.0-RFN': ('SIL Open Font License 1.0 with Reserved Font Name', 'https://spdx.org/licenses/OFL-1.0-RFN.html http://scripts.sil.org/cms/scripts/page.php?item_id=OFL10_web'),
  'OFL-1.0-no-RFN': ('SIL Open Font License 1.0 with no Reserved Font Name', 'https://spdx.org/licenses/OFL-1.0-no-RFN.html http://scripts.sil.org/cms/scripts/page.php?item_id=OFL10_web'),
  'OFL-1.1': ('SIL Open Font License 1.1', 'https://spdx.org/licenses/OFL-1.1.html http://scripts.sil.org/cms/scripts/page.php?item_id=OFL_web https://opensource.org/licenses/OFL-1.1'),
  'OFL-1.1-RFN': ('SIL Open Font License 1.1 with Reserved Font Name', 'https://spdx.org/licenses/OFL-1.1-RFN.html http://scripts.sil.org/cms/scripts/page.php?item_id=OFL_web https://opensource.org/licenses/OFL-1.1'),
  'OFL-1.1-no-RFN': ('SIL Open Font License 1.1 with no Reserved Font Name', 'https://spdx.org/licenses/OFL-1.1-no-RFN.html http://scripts.sil.org/cms/scripts/page.php?item_id=OFL_web https://opensource.org/licenses/OFL-1.1'),
  'OGC-1.0': ('OGC Software License, Version 1.0', 'https://spdx.org/licenses/OGC-1.0.html https://www.ogc.org/ogc/software/1.0'),
  'OGDL-Taiwan-1.0': ('Taiwan Open Government Data License, version 1.0', 'https://spdx.org/licenses/OGDL-Taiwan-1.0.html https://data.gov.tw/license'),
  'OGL-Canada-2.0': ('Open Government Licence - Canada', 'https://spdx.org/licenses/OGL-Canada-2.0.html https://open.canada.ca/en/open-government-licence-canada'),
  'OGL-UK-1.0': ('Open Government Licence v1.0', 'https://spdx.org/licenses/OGL-UK-1.0.html http://www.nationalarchives.gov.uk/doc/open-government-licence/version/1/'),
  'OGL-UK-2.0': ('Open Government Licence v2.0', 'https://spdx.org/licenses/OGL-UK-2.0.html http://www.nationalarchives.gov.uk/doc/open-government-licence/version/2/'),
  'OGL-UK-3.0': ('Open Government Licence v3.0', 'https://spdx.org/licenses/OGL-UK-3.0.html http://www.nationalarchives.gov.uk/doc/open-government-licence/version/3/'),
  'OGTSL': ('Open Group Test Suite License', 'https://spdx.org/licenses/OGTSL.html http://www.opengroup.org/testing/downloads/The_Open_Group_TSL.txt https://opensource.org/licenses/OGTSL'),
  'OLDAP-1.1': ('Open LDAP Public License v1.1', 'https://spdx.org/licenses/OLDAP-1.1.html http://www.openldap.org/devel/gitweb.cgi?p=openldap.git;a=blob;f=LICENSE;hb=806557a5ad59804ef3a44d5abfbe91d706b0791f'),
  'OLDAP-1.2': ('Open LDAP Public License v1.2', 'https://spdx.org/licenses/OLDAP-1.2.html http://www.openldap.org/devel/gitweb.cgi?p=openldap.git;a=blob;f=LICENSE;hb=42b0383c50c299977b5893ee695cf4e486fb0dc7'),
  'OLDAP-1.3': ('Open LDAP Public License v1.3', 'https://spdx.org/licenses/OLDAP-1.3.html http://www.openldap.org/devel/gitweb.cgi?p=openldap.git;a=blob;f=LICENSE;hb=e5f8117f0ce088d0bd7a8e18ddf37eaa40eb09b1'),
  'OLDAP-1.4': ('Open LDAP Public License v1.4', 'https://spdx.org/licenses/OLDAP-1.4.html http://www.openldap.org/devel/gitweb.cgi?p=openldap.git;a=blob;f=LICENSE;hb=c9f95c2f3f2ffb5e0ae55fe7388af75547660941'),
  'OLDAP-2.0': ('Open LDAP Public License v2.0 (or possibly 2.0A and 2.0B)', 'https://spdx.org/licenses/OLDAP-2.0.html http://www.openldap.org/devel/gitweb.cgi?p=openldap.git;a=blob;f=LICENSE;hb=cbf50f4e1185a21abd4c0a54d3f4341fe28f36ea'),
  'OLDAP-2.0.1': ('Open LDAP Public License v2.0.1', 'https://spdx.org/licenses/OLDAP-2.0.1.html http://www.openldap.org/devel/gitweb.cgi?p=openldap.git;a=blob;f=LICENSE;hb=b6d68acd14e51ca3aab4428bf26522aa74873f0e'),
  'OLDAP-2.1': ('Open LDAP Public License v2.1', 'https://spdx.org/licenses/OLDAP-2.1.html http://www.openldap.org/devel/gitweb.cgi?p=openldap.git;a=blob;f=LICENSE;hb=b0d176738e96a0d3b9f85cb51e140a86f21be715'),
  'OLDAP-2.2': ('Open LDAP Public License v2.2', 'https://spdx.org/licenses/OLDAP-2.2.html http://www.openldap.org/devel/gitweb.cgi?p=openldap.git;a=blob;f=LICENSE;hb=470b0c18ec67621c85881b2733057fecf4a1acc3'),
  'OLDAP-2.2.1': ('Open LDAP Public License v2.2.1', 'https://spdx.org/licenses/OLDAP-2.2.1.html http://www.openldap.org/devel/gitweb.cgi?p=openldap.git;a=blob;f=LICENSE;hb=4bc786f34b50aa301be6f5600f58a980070f481e'),
  'OLDAP-2.2.2': ('Open LDAP Public License 2.2.2', 'https://spdx.org/licenses/OLDAP-2.2.2.html http://www.openldap.org/devel/gitweb.cgi?p=openldap.git;a=blob;f=LICENSE;hb=df2cc1e21eb7c160695f5b7cffd6296c151ba188'),
  'OLDAP-2.3': ('Open LDAP Public License v2.3', 'https://spdx.org/licenses/OLDAP-2.3.html http://www.openldap.org/devel/gitweb.cgi?p=openldap.git;a=blob;f=LICENSE;hb=d32cf54a32d581ab475d23c810b0a7fbaf8d63c3'),
  'OLDAP-2.4': ('Open LDAP Public License v2.4', 'https://spdx.org/licenses/OLDAP-2.4.html http://www.openldap.org/devel/gitweb.cgi?p=openldap.git;a=blob;f=LICENSE;hb=cd1284c4a91a8a380d904eee68d1583f989ed386'),
  'OLDAP-2.5': ('Open LDAP Public License v2.5', 'https://spdx.org/licenses/OLDAP-2.5.html http://www.openldap.org/devel/gitweb.cgi?p=openldap.git;a=blob;f=LICENSE;hb=6852b9d90022e8593c98205413380536b1b5a7cf'),
  'OLDAP-2.6': ('Open LDAP Public License v2.6', 'https://spdx.org/licenses/OLDAP-2.6.html http://www.openldap.org/devel/gitweb.cgi?p=openldap.git;a=blob;f=LICENSE;hb=1cae062821881f41b73012ba816434897abf4205'),
  'OLDAP-2.7': ('Open LDAP Public License v2.7', 'https://spdx.org/licenses/OLDAP-2.7.html http://www.openldap.org/devel/gitweb.cgi?p=openldap.git;a=blob;f=LICENSE;hb=47c2415c1df81556eeb39be6cad458ef87c534a2'),
  'OLDAP-2.8': ('Open LDAP Public License v2.8', 'https://spdx.org/licenses/OLDAP-2.8.html http://www.openldap.org/software/release/license.html'),
  'OML': ('Open Market License', 'https://spdx.org/licenses/OML.html https://fedoraproject.org/wiki/Licensing/Open_Market_License'),
  'OPL-1.0': ('Open Public License v1.0', 'https://spdx.org/licenses/OPL-1.0.html http://old.koalateam.com/jackaroo/OPL_1_0.TXT https://fedoraproject.org/wiki/Licensing/Open_Public_License'),
  'OPUBL-1.0': ('Open Publication License v1.0', 'https://spdx.org/licenses/OPUBL-1.0.html http://opencontent.org/openpub/ https://www.debian.org/opl https://www.ctan.org/license/opl'),
  'OSET-PL-2.1': ('OSET Public License version 2.1', 'https://spdx.org/licenses/OSET-PL-2.1.html http://www.osetfoundation.org/public-license https://opensource.org/licenses/OPL-2.1'),
  'OSL-1.0': ('Open Software License 1.0', 'https://spdx.org/licenses/OSL-1.0.html https://opensource.org/licenses/OSL-1.0'),
  'OSL-1.1': ('Open Software License 1.1', 'https://spdx.org/licenses/OSL-1.1.html https://fedoraproject.org/wiki/Licensing/OSL1.1'),
  'OSL-2.0': ('Open Software License 2.0', 'https://spdx.org/licenses/OSL-2.0.html http://web.archive.org/web/20041020171434/http://www.rosenlaw.com/osl2.0.html'),
  'OSL-2.1': ('Open Software License 2.1', 'https://spdx.org/licenses/OSL-2.1.html http://web.archive.org/web/20050212003940/http://www.rosenlaw.com/osl21.htm https://opensource.org/licenses/OSL-2.1'),
  'OSL-3.0': ('Open Software License 3.0', 'https://spdx.org/licenses/OSL-3.0.html https://web.archive.org/web/20120101081418/http://rosenlaw.com:80/OSL3.0.htm https://opensource.org/licenses/OSL-3.0'),
  'OpenSSL': ('OpenSSL License', 'https://spdx.org/licenses/OpenSSL.html http://www.openssl.org/source/license.html'),
  'PDDL-1.0': ('Open Data Commons Public Domain Dedication & License 1.0', 'https://spdx.org/licenses/PDDL-1.0.html http://opendatacommons.org/licenses/pddl/1.0/ https://opendatacommons.org/licenses/pddl/'),
  'PHP-3.0': ('PHP License v3.0', 'https://spdx.org/licenses/PHP-3.0.html http://www.php.net/license/3_0.txt https://opensource.org/licenses/PHP-3.0'),
  'PHP-3.01': ('PHP License v3.01', 'https://spdx.org/licenses/PHP-3.01.html http://www.php.net/license/3_01.txt'),
  'PSF-2.0': ('Python Software Foundation License 2.0', 'https://spdx.org/licenses/PSF-2.0.html https://opensource.org/licenses/Python-2.0'),
  'Parity-6.0.0': ('The Parity Public License 6.0.0', 'https://spdx.org/licenses/Parity-6.0.0.html https://paritylicense.com/versions/6.0.0.html'),
  'Parity-7.0.0': ('The Parity Public License 7.0.0', 'https://spdx.org/licenses/Parity-7.0.0.html https://paritylicense.com/versions/7.0.0.html'),
  'Plexus': ('Plexus Classworlds License', 'https://spdx.org/licenses/Plexus.html https://fedoraproject.org/wiki/Licensing/Plexus_Classworlds_License'),
  'PolyForm-Noncommercial-1.0.0': ('PolyForm Noncommercial License 1.0.0', 'https://spdx.org/licenses/PolyForm-Noncommercial-1.0.0.html https://polyformproject.org/licenses/noncommercial/1.0.0'),
  'PolyForm-Small-Business-1.0.0': ('PolyForm Small Business License 1.0.0', 'https://spdx.org/licenses/PolyForm-Small-Business-1.0.0.html https://polyformproject.org/licenses/small-business/1.0.0'),
  'PostgreSQL': ('PostgreSQL License', 'https://spdx.org/licenses/PostgreSQL.html http://www.postgresql.org/about/licence https://opensource.org/licenses/PostgreSQL'),
  'Python-2.0': ('Python License 2.0', 'https://spdx.org/licenses/Python-2.0.html https://opensource.org/licenses/Python-2.0'),
  'QPL-1.0': ('Q Public License 1.0', 'https://spdx.org/licenses/QPL-1.0.html http://doc.qt.nokia.com/3.3/license.html https://opensource.org/licenses/QPL-1.0'),
  'Qhull': ('Qhull License', 'https://spdx.org/licenses/Qhull.html https://fedoraproject.org/wiki/Licensing/Qhull'),
  'RHeCos-1.1': ('Red Hat eCos Public License v1.1', 'https://spdx.org/licenses/RHeCos-1.1.html http://ecos.sourceware.org/old-license.html'),
  'RPL-1.1': ('Reciprocal Public License 1.1', 'https://spdx.org/licenses/RPL-1.1.html https://opensource.org/licenses/RPL-1.1'),
  'RPL-1.5': ('Reciprocal Public License 1.5', 'https://spdx.org/licenses/RPL-1.5.html https://opensource.org/licenses/RPL-1.5'),
  'RPSL-1.0': ('RealNetworks Public Source License v1.0', 'https://spdx.org/licenses/RPSL-1.0.html https://helixcommunity.org/content/rpsl https://opensource.org/licenses/RPSL-1.0'),
  'RSA-MD': ('RSA Message-Digest License', 'https://spdx.org/licenses/RSA-MD.html http://www.faqs.org/rfcs/rfc1321.html'),
  'RSCPL': ('Ricoh Source Code Public License', 'https://spdx.org/licenses/RSCPL.html http://wayback.archive.org/web/20060715140826/http://www.risource.org/RPL/RPL-1.0A.shtml https://opensource.org/licenses/RSCPL'),
  'Rdisc': ('Rdisc License', 'https://spdx.org/licenses/Rdisc.html https://fedoraproject.org/wiki/Licensing/Rdisc_License'),
  'Ruby': ('Ruby License', 'https://spdx.org/licenses/Ruby.html http://www.ruby-lang.org/en/LICENSE.txt'),
  'SAX-PD': ('Sax Public Domain Notice', 'https://spdx.org/licenses/SAX-PD.html http://www.saxproject.org/copying.html'),
  'SCEA': ('SCEA Shared Source License', 'https://spdx.org/licenses/SCEA.html http://research.scea.com/scea_shared_source_license.html'),
  'SGI-B-1.0': ('SGI Free Software License B v1.0', 'https://spdx.org/licenses/SGI-B-1.0.html http://oss.sgi.com/projects/FreeB/SGIFreeSWLicB.1.0.html'),
  'SGI-B-1.1': ('SGI Free Software License B v1.1', 'https://spdx.org/licenses/SGI-B-1.1.html http://oss.sgi.com/projects/FreeB/'),
  'SGI-B-2.0': ('SGI Free Software License B v2.0', 'https://spdx.org/licenses/SGI-B-2.0.html http://oss.sgi.com/projects/FreeB/SGIFreeSWLicB.2.0.pdf'),
  'SHL-0.5': ('Solderpad Hardware License v0.5', 'https://spdx.org/licenses/SHL-0.5.html https://solderpad.org/licenses/SHL-0.5/'),
  'SHL-0.51': ('Solderpad Hardware License, Version 0.51', 'https://spdx.org/licenses/SHL-0.51.html https://solderpad.org/licenses/SHL-0.51/'),
  'SISSL': ('Sun Industry Standards Source License v1.1', 'https://spdx.org/licenses/SISSL.html http://www.openoffice.org/licenses/sissl_license.html https://opensource.org/licenses/SISSL'),
  'SISSL-1.2': ('Sun Industry Standards Source License v1.2', 'https://spdx.org/licenses/SISSL-1.2.html http://gridscheduler.sourceforge.net/Gridengine_SISSL_license.html'),
  'SMLNJ': ('Standard ML of New Jersey License', 'https://spdx.org/licenses/SMLNJ.html https://www.smlnj.org/license.html'),
  'SMPPL': ('Secure Messaging Protocol Public License', 'https://spdx.org/licenses/SMPPL.html https://github.com/dcblake/SMP/blob/master/Documentation/License.txt'),
  'SNIA': ('SNIA Public License 1.1', 'https://spdx.org/licenses/SNIA.html https://fedoraproject.org/wiki/Licensing/SNIA_Public_License'),
  'SPL-1.0': ('Sun Public License v1.0', 'https://spdx.org/licenses/SPL-1.0.html https://opensource.org/licenses/SPL-1.0'),
  'SSH-OpenSSH': ('SSH OpenSSH license', 'https://spdx.org/licenses/SSH-OpenSSH.html https://github.com/openssh/openssh-portable/blob/1b11ea7c58cd5c59838b5fa574cd456d6047b2d4/LICENCE#L10'),
  'SSH-short': ('SSH short notice', 'https://spdx.org/licenses/SSH-short.html https://github.com/openssh/openssh-portable/blob/1b11ea7c58cd5c59838b5fa574cd456d6047b2d4/pathnames.h http://web.mit.edu/kolya/.f/root/athena.mit.edu/sipb.mit.edu/project/openssh/OldFiles/src/openssh-2.9.9p2/ssh-add.1 https://joinup.ec.europa.eu/svn/lesoll/trunk/italc/lib/src/dsa_key.cpp'),
  'SSPL-1.0': ('Server Side Public License, v 1', 'https://spdx.org/licenses/SSPL-1.0.html https://www.mongodb.com/licensing/server-side-public-license'),
  'SWL': ('Scheme Widget Library (SWL) Software License Agreement', 'https://spdx.org/licenses/SWL.html https://fedoraproject.org/wiki/Licensing/SWL'),
  'Saxpath': ('Saxpath License', 'https://spdx.org/licenses/Saxpath.html https://fedoraproject.org/wiki/Licensing/Saxpath_License'),
  'Sendmail': ('Sendmail License', 'https://spdx.org/licenses/Sendmail.html http://www.sendmail.com/pdfs/open_source/sendmail_license.pdf https://web.archive.org/web/20160322142305/https://www.sendmail.com/pdfs/open_source/sendmail_license.pdf'),
  'Sendmail-8.23': ('Sendmail License 8.23', 'https://spdx.org/licenses/Sendmail-8.23.html https://www.proofpoint.com/sites/default/files/sendmail-license.pdf https://web.archive.org/web/20181003101040/https://www.proofpoint.com/sites/default/files/sendmail-license.pdf'),
  'SimPL-2.0': ('Simple Public License 2.0', 'https://spdx.org/licenses/SimPL-2.0.html https://opensource.org/licenses/SimPL-2.0'),
  'Sleepycat': ('Sleepycat License', 'https://spdx.org/licenses/Sleepycat.html https://opensource.org/licenses/Sleepycat'),
  'Spencer-86': ('Spencer License 86', 'https://spdx.org/licenses/Spencer-86.html https://fedoraproject.org/wiki/Licensing/Henry_Spencer_Reg-Ex_Library_License'),
  'Spencer-94': ('Spencer License 94', 'https://spdx.org/licenses/Spencer-94.html https://fedoraproject.org/wiki/Licensing/Henry_Spencer_Reg-Ex_Library_License'),
  'Spencer-99': ('Spencer License 99', 'https://spdx.org/licenses/Spencer-99.html http://www.opensource.apple.com/source/tcl/tcl-5/tcl/generic/regfronts.c'),
  'StandardML-NJ': ('Standard ML of New Jersey License', 'https://spdx.org/licenses/StandardML-NJ.html http://www.smlnj.org//license.html'),
  'SugarCRM-1.1.3': ('SugarCRM Public License v1.1.3', 'https://spdx.org/licenses/SugarCRM-1.1.3.html http://www.sugarcrm.com/crm/SPL'),
  'TAPR-OHL-1.0': ('TAPR Open Hardware License v1.0', 'https://spdx.org/licenses/TAPR-OHL-1.0.html https://www.tapr.org/OHL'),
  'TCL': ('TCL/TK License', 'https://spdx.org/licenses/TCL.html http://www.tcl.tk/software/tcltk/license.html https://fedoraproject.org/wiki/Licensing/TCL'),
  'TCP-wrappers': ('TCP Wrappers License', 'https://spdx.org/licenses/TCP-wrappers.html http://rc.quest.com/topics/openssh/license.php#tcpwrappers'),
  'TMate': ('TMate Open Source License', 'https://spdx.org/licenses/TMate.html http://svnkit.com/license.html'),
  'TORQUE-1.1': ('TORQUE v2.5+ Software License v1.1', 'https://spdx.org/licenses/TORQUE-1.1.html https://fedoraproject.org/wiki/Licensing/TORQUEv1.1'),
  'TOSL': ('Trusster Open Source License', 'https://spdx.org/licenses/TOSL.html https://fedoraproject.org/wiki/Licensing/TOSL'),
  'TU-Berlin-1.0': ('Technische Universitaet Berlin License 1.0', 'https://spdx.org/licenses/TU-Berlin-1.0.html https://github.com/swh/ladspa/blob/7bf6f3799fdba70fda297c2d8fd9f526803d9680/gsm/COPYRIGHT'),
  'TU-Berlin-2.0': ('Technische Universitaet Berlin License 2.0', 'https://spdx.org/licenses/TU-Berlin-2.0.html https://github.com/CorsixTH/deps/blob/fd339a9f526d1d9c9f01ccf39e438a015da50035/licences/libgsm.txt'),
  'UCL-1.0': ('Upstream Compatibility License v1.0', 'https://spdx.org/licenses/UCL-1.0.html https://opensource.org/licenses/UCL-1.0'),
  'UPL-1.0': ('Universal Permissive License v1.0', 'https://spdx.org/licenses/UPL-1.0.html https://opensource.org/licenses/UPL'),
  'Unicode-DFS-2015': ('Unicode License Agreement - Data Files and Software (2015)', 'https://spdx.org/licenses/Unicode-DFS-2015.html https://web.archive.org/web/20151224134844/http://unicode.org/copyright.html'),
  'Unicode-DFS-2016': ('Unicode License Agreement - Data Files and Software (2016)', 'https://spdx.org/licenses/Unicode-DFS-2016.html http://www.unicode.org/copyright.html'),
  'Unicode-TOU': ('Unicode Terms of Use', 'https://spdx.org/licenses/Unicode-TOU.html http://www.unicode.org/copyright.html'),
  'Unlicense': ('The Unlicense', 'https://spdx.org/licenses/Unlicense.html https://unlicense.org/'),
  'VOSTROM': ('VOSTROM Public License for Open Source', 'https://spdx.org/licenses/VOSTROM.html https://fedoraproject.org/wiki/Licensing/VOSTROM'),
  'VSL-1.0': ('Vovida Software License v1.0', 'https://spdx.org/licenses/VSL-1.0.html https://opensource.org/licenses/VSL-1.0'),
  'Vim': ('Vim License', 'https://spdx.org/licenses/Vim.html http://vimdoc.sourceforge.net/htmldoc/uganda.html'),
  'W3C': ('W3C Software Notice and License (2002-12-31)', 'https://spdx.org/licenses/W3C.html http://www.w3.org/Consortium/Legal/2002/copyright-software-20021231.html https://opensource.org/licenses/W3C'),
  'W3C-19980720': ('W3C Software Notice and License (1998-07-20)', 'https://spdx.org/licenses/W3C-19980720.html http://www.w3.org/Consortium/Legal/copyright-software-19980720.html'),
  'W3C-20150513': ('W3C Software Notice and Document License (2015-05-13)', 'https://spdx.org/licenses/W3C-20150513.html https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document'),
  'WTFPL': ('Do What The F*ck You Want To Public License', 'https://spdx.org/licenses/WTFPL.html http://www.wtfpl.net/about/ http://sam.zoy.org/wtfpl/COPYING'),
  'Watcom-1.0': ('Sybase Open Watcom Public License 1.0', 'https://spdx.org/licenses/Watcom-1.0.html https://opensource.org/licenses/Watcom-1.0'),
  'Wsuipa': ('Wsuipa License', 'https://spdx.org/licenses/Wsuipa.html https://fedoraproject.org/wiki/Licensing/Wsuipa'),
  'X11': ('X11 License', 'https://spdx.org/licenses/X11.html http://www.xfree86.org/3.3.6/COPYRIGHT2.html#3'),
  'XFree86-1.1': ('XFree86 License 1.1', 'https://spdx.org/licenses/XFree86-1.1.html http://www.xfree86.org/current/LICENSE4.html'),
  'XSkat': ('XSkat License', 'https://spdx.org/licenses/XSkat.html https://fedoraproject.org/wiki/Licensing/XSkat_License'),
  'Xerox': ('Xerox License', 'https://spdx.org/licenses/Xerox.html https://fedoraproject.org/wiki/Licensing/Xerox'),
  'Xnet': ('X.Net License', 'https://spdx.org/licenses/Xnet.html https://opensource.org/licenses/Xnet'),
  'YPL-1.0': ('Yahoo! Public License v1.0', 'https://spdx.org/licenses/YPL-1.0.html http://www.zimbra.com/license/yahoo_public_license_1.0.html'),
  'YPL-1.1': ('Yahoo! Public License v1.1', 'https://spdx.org/licenses/YPL-1.1.html http://www.zimbra.com/license/yahoo_public_license_1.1.html'),
  'ZPL-1.1': ('Zope Public License 1.1', 'https://spdx.org/licenses/ZPL-1.1.html http://old.zope.org/Resources/License/ZPL-1.1'),
  'ZPL-2.0': ('Zope Public License 2.0', 'https://spdx.org/licenses/ZPL-2.0.html http://old.zope.org/Resources/License/ZPL-2.0 https://opensource.org/licenses/ZPL-2.0'),
  'ZPL-2.1': ('Zope Public License 2.1', 'https://spdx.org/licenses/ZPL-2.1.html http://old.zope.org/Resources/ZPL/'),
  'Zed': ('Zed License', 'https://spdx.org/licenses/Zed.html https://fedoraproject.org/wiki/Licensing/Zed'),
  'Zend-2.0': ('Zend License v2.0', 'https://spdx.org/licenses/Zend-2.0.html https://web.archive.org/web/20130517195954/http://www.zend.com/license/2_00.txt'),
  'Zimbra-1.3': ('Zimbra Public License v1.3', 'https://spdx.org/licenses/Zimbra-1.3.html http://web.archive.org/web/20100302225219/http://www.zimbra.com/license/zimbra-public-license-1-3.html'),
  'Zimbra-1.4': ('Zimbra Public License v1.4', 'https://spdx.org/licenses/Zimbra-1.4.html http://www.zimbra.com/legal/zimbra-public-license-1-4'),
  'Zlib': ('zlib License', 'https://spdx.org/licenses/Zlib.html http://www.zlib.net/zlib_license.html https://opensource.org/licenses/Zlib'),
  'blessing': ('SQLite Blessing', 'https://spdx.org/licenses/blessing.html https://www.sqlite.org/src/artifact/e33a4df7e32d742a?ln=4-9 https://sqlite.org/src/artifact/df5091916dbb40e6'),
  'bzip2-1.0.5': ('bzip2 and libbzip2 License v1.0.5', 'https://spdx.org/licenses/bzip2-1.0.5.html https://sourceware.org/bzip2/1.0.5/bzip2-manual-1.0.5.html http://bzip.org/1.0.5/bzip2-manual-1.0.5.html'),
  'bzip2-1.0.6': ('bzip2 and libbzip2 License v1.0.6', 'https://spdx.org/licenses/bzip2-1.0.6.html https://sourceware.org/git/?p=bzip2.git;a=blob;f=LICENSE;hb=bzip2-1.0.6 http://bzip.org/1.0.5/bzip2-manual-1.0.5.html'),
  'copyleft-next-0.3.0': ('copyleft-next 0.3.0', 'https://spdx.org/licenses/copyleft-next-0.3.0.html https://github.com/copyleft-next/copyleft-next/blob/master/Releases/copyleft-next-0.3.0'),
  'copyleft-next-0.3.1': ('copyleft-next 0.3.1', 'https://spdx.org/licenses/copyleft-next-0.3.1.html https://github.com/copyleft-next/copyleft-next/blob/master/Releases/copyleft-next-0.3.1'),
  'curl': ('curl License', 'https://spdx.org/licenses/curl.html https://github.com/bagder/curl/blob/master/COPYING'),
  'diffmark': ('diffmark license', 'https://spdx.org/licenses/diffmark.html https://fedoraproject.org/wiki/Licensing/diffmark'),
  'dvipdfm': ('dvipdfm License', 'https://spdx.org/licenses/dvipdfm.html https://fedoraproject.org/wiki/Licensing/dvipdfm'),
  'eCos-2.0': ('eCos license version 2.0', 'https://spdx.org/licenses/eCos-2.0.html https://www.gnu.org/licenses/ecos-license.html'),
  'eGenix': ('eGenix.com Public License 1.1.0', 'https://spdx.org/licenses/eGenix.html http://www.egenix.com/products/eGenix.com-Public-License-1.1.0.pdf https://fedoraproject.org/wiki/Licensing/eGenix.com_Public_License_1.1.0'),
  'etalab-2.0': ('Etalab Open License 2.0', 'https://spdx.org/licenses/etalab-2.0.html https://github.com/DISIC/politique-de-contribution-open-source/blob/master/LICENSE.pdf https://raw.githubusercontent.com/DISIC/politique-de-contribution-open-source/master/LICENSE'),
  'gSOAP-1.3b': ('gSOAP Public License v1.3b', 'https://spdx.org/licenses/gSOAP-1.3b.html http://www.cs.fsu.edu/~engelen/license.html'),
  'gnuplot': ('gnuplot License', 'https://spdx.org/licenses/gnuplot.html https://fedoraproject.org/wiki/Licensing/Gnuplot'),
  'iMatix': ('iMatix Standard Function Library Agreement', 'https://spdx.org/licenses/iMatix.html http://legacy.imatix.com/html/sfl/sfl4.htm#license'),
  'libpng-2.0': ('PNG Reference Library version 2', 'https://spdx.org/licenses/libpng-2.0.html http://www.libpng.org/pub/png/src/libpng-LICENSE.txt'),
  'libselinux-1.0': ('libselinux public domain notice', 'https://spdx.org/licenses/libselinux-1.0.html https://github.com/SELinuxProject/selinux/blob/master/libselinux/LICENSE'),
  'libtiff': ('libtiff License', 'https://spdx.org/licenses/libtiff.html https://fedoraproject.org/wiki/Licensing/libtiff'),
  'mpich2': ('mpich2 License', 'https://spdx.org/licenses/mpich2.html https://fedoraproject.org/wiki/Licensing/MIT'),
  'psfrag': ('psfrag License', 'https://spdx.org/licenses/psfrag.html https://fedoraproject.org/wiki/Licensing/psfrag'),
  'psutils': ('psutils License', 'https://spdx.org/licenses/psutils.html https://fedoraproject.org/wiki/Licensing/psutils'),
  'wxWindows': ('wxWindows Library License', 'https://spdx.org/licenses/wxWindows.html https://opensource.org/licenses/WXwindows'),
  'xinetd': ('xinetd License', 'https://spdx.org/licenses/xinetd.html https://fedoraproject.org/wiki/Licensing/Xinetd_License'),
  'xpp': ('XPP License', 'https://spdx.org/licenses/xpp.html https://fedoraproject.org/wiki/Licensing/xpp'),
  'zlib-acknowledgement': ('zlib/libpng License with Acknowledgement', 'https://spdx.org/licenses/zlib-acknowledgement.html https://fedoraproject.org/wiki/Licensing/ZlibWithAcknowledgement'),
}
def print_spdx_licenses():
  import json, urllib.request
  req = urllib.request.urlopen ('https://raw.githubusercontent.com/spdx/license-list-data/master/json/licenses.json')
  for l in sorted (json.loads (req.read().decode ('utf-8'))['licenses'], key = lambda d: d['licenseId']):
    links = ' '.join ([ l['reference'] ] + l['seeAlso'])
    print ("  %s: (%s, %s)," % (repr (l['licenseId']), repr (l['name']), repr (links)))
  # generate: $0 --spdx-licenses

if __name__ == '__main__':
  mkcopyright (sys.argv)
