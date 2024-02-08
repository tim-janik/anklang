#!/usr/bin/env python3
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
import sys, re

# Configure checks
checks = {
  'ban-printf':                         True,
  'separate-body':                      True,
  'whitespace-before-parenthesis':      True,
  'whitespace-after-parenthesis':       True,
  'ban-fixme':                          True,
  'ban-todo':                           False,
}

# Detect various code smells via regexp
def line_matcher (code, text, has_comment, orig, filename):
  error, warning = '', ''
  # ban-printf
  if m := checks['ban-printf'] and re.search (r'\bd?printf\s*\(', code):
    error = "invalid call to printf"
  # whitespace-before-parenthesis
  elif m := checks['whitespace-before-parenthesis'] and code.find ('#') < 0 and re.search (r'\s\w+[a-z0-9]\([^)]', code, re.IGNORECASE):
    if not re.search (r'\s\.|:\s', code):               # ignore funcs in CSS rules
      m = m.end() - 2
      warning = "missing whitespace before parenthesis"
  # whitespace-after-parenthesis
  elif m := checks['whitespace-after-parenthesis'] and re.search (r'\)[a-z0-9]', code, re.IGNORECASE):
    if not any_in (('(/', ',/', '/,', '/)'), code):     # ignore JS regexp
      m = m.end() - 2
      warning = "missing whitespace after parenthesis"
  # ban-fixme
  elif m := checks['ban-fixme'] and has_comment and re.search (r'\bFI[X]ME\b', text, re.IGNORECASE):
    error = "comment indicates unfinished code"
  # ban-todo
  elif m := checks['ban-todo'] and has_comment and re.search (r'\bTODO\b', text, re.IGNORECASE):
    error = "comment indicates open issues"
  # separate-body
  elif m := checks['separate-body'] and re.match (r'\s*[\w <:,>]+\s*\((.+\s+.+)?\)[\s\w]*{\s*(/[/*].*)?$', code):
    m = m.start() + m.group().find ('{')
    ignore = bool (re.search (r'\]\s*\(', code))                # ignore lambda
    ignore |= bool (re.search (r'\balignas\s*\(', code))        # ignore alignas()
    if not ignore:
      for w in ('do', 'switch', 'while', 'for', 'if', 'namespace'):
        if re.search (r'\b' + w + r'\b', code):
          ignore = True
          break
    if not ignore:
      warning = "missing newline before function body"
  # print diagnostics
  if error or warning:
    lc = orig.count ('\n')
    msg = filename + ':' + str (lc) + ':'
    B, f, R, G, M, Z = '', '', '', '', '', ''
    if sys.stderr.isatty():
      B, f, R, G, M, Z = "\033[01m", "\033[22m", "\033[31m", "\033[32m", "\033[35m", "\033[0m"
    msg = B + msg + ' '
    if error: msg += B + R + 'error:' + Z
    else:     msg += B + M + 'warning:' + Z
    msg += ' ' + (error or warning) + f
    print (msg, file = sys.stderr)
    print ('%5u |' % lc, text.rstrip(), file = sys.stderr)
    if not isinstance (m, (int, float)):
      m = m.start()
    print ('%5u |' % lc, ' ' * m + B + G + '^' + Z, file = sys.stderr)
    # print ('%5u |' % lc, re.sub ('[^^]', ' ', re.sub (r'\w', '^', text)).rstrip())

def any_in (needles, haystack):
  for needle in needles:
    if needle in haystack:
      return True
  return False

# First disguise C strings and comments, then invoke matcher predicate per line
class CLexer:
  def __init__ (self, fname, linematcher):
    self.filename = fname
    self.sq = False
    self.dq = False
    self.bs = False
    self.slc = False
    self.mlc = False
    self.orig = ''
    self.out = ''
    self.last = ''
    self.commentline = False
    self.linematcher = linematcher
  def feed_char (self, c):
    self.orig += c
    if self.last == '\\' and (self.sq or self.dq):      # backslash inside string
      self.out += c; self.last = ' '; return
    if self.sq:                                         # single quote string
      if c == "'":
        self.sq = False
        return self.append (c, c)
      return self.append ('_', c)
    if self.dq:                                         # double quote string
      if c == '"':
        self.dq = False
        return self.append (c, c)
      return self.append ('_', c)
    if self.mlc:                                        # multi line comment
      if c == "/" and self.last == '*':
        self.mlc = False
        self.out = self.out[0:-1] + '*'
        return self.append (c, c)
      if c == "\n":
        return self.append (c, c)
      return self.append ('_', c)
    if self.slc:                                        # single line comment
      if c == "\n":
        self.slc = False
        return self.append (c, c)
      return self.append ('_', c)
    if self.last == "/" and c == "/":                   # start single line comment
      self.slc = True
      return self.append (c, c)
    if self.last == "/" and c == "*":                   # start multi line comment
      self.mlc = True
      return self.append (c, ' ')
    if c == '"':                                        # start double quote string
      self.dq = True
      return self.append (c, c)
    if c == "'":                                        # start single quote string
      self.sq = True
      return self.append (c, c)
    if c in " \t\v\f\n\r":                              # whitespace
      return self.append (c, c)
    if 0 and re.match (r'\w', c):
      return self.append ('x', c)                       # identifier
    return self.append (c, c)                           # non word char
  def feed (self, txt):
    for c in txt:
      self.feed_char (c)
  def append (self, char, last, is_commentline = False):
    self.commentline = self.commentline or self.mlc or self.slc
    if char == '\n':
      was_commentline = self.commentline
      self.commentline = self.mlc or self.slc
      p1 = self.out.rfind ('\n') or 0
      p1 += p1 != 0
      self.linematcher (self.out[p1:], self.orig[p1:], was_commentline, self.orig, self.filename)
    self.out += char
    self.last = last

# CLI help
def print_help():
  import os
  print ("Usage: %s [options] {ccfile} " % os.path.basename (sys.argv[0]))
  print ("Check syntax for code smells, based on simple regular expression.")
  print ("Options:")
  print ("  --help, -h                Print this help message")
  print ("  --all                     Enable all checks")
  print ("  --none                    Disable all checks")
  print ("  --list                    Show check configuration")
  print ("  --<check-name>=1          Enable check")
  print ("  --<check-name>=0          Disnable check")

# main program
if __name__ == '__main__':
  def checks_arg (arg):
    if not arg.startswith ('--'): return None
    eq = arg.find ('=')
    if eq < 3: return None
    if not arg[2:eq] in checks: return None
    return arg[2:eq], int (arg[eq+1:])
  for arg in sys.argv[1:]:
    if arg == "--help" or arg == "-h":
      print_help()
      sys.exit (0)
    elif kv := checks_arg (arg):
      checks[kv[0]] = bool (kv[1])
    elif arg == '--all':
      for k in checks: checks[k] = True
    elif arg == '--none':
      for k in checks: checks[k] = False
    elif arg == '--list':
      for k in checks:
        print (' ', '--' + k + '=' + str (int (checks[k])))
    else:
      clex = CLexer (arg, line_matcher)
      clex.feed (open (arg, 'r').read())
  if len (sys.argv) <= 1:
    print_help()
    sys.exit (0)
