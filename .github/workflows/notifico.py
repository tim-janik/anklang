#!/usr/bin/env python3
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
import sys, os, re, requests

usage_help = '''
Simple script to format and trigger a notifico plain text hook.
As fallback notification hook, a URL can be set via $NOTIFICO_PHOOK.
'''

def parse_args (sysargs):
  import argparse
  global server, port, nickname
  parser = argparse.ArgumentParser (description = usage_help)
  parser.add_argument ('-0', action = "store_true",
		       help = 'Use zero colors')
  parser.add_argument ('-H', metavar = 'URL', default = '',
		       help = 'Remote notification hook')
  parser.add_argument ('-n', metavar = 'NAME', default = '',
                       help = 'Initiating nickname')
  parser.add_argument ('-b', metavar = 'BRANCH', default = '',
                       help = 'Repository branch')
  parser.add_argument ('-u', metavar = 'URL', default = '',
                       help = 'Associated URL')
  parser.add_argument ('-s', metavar = 'STATUS', default = '',
                       help = 'Status information')
  parser.add_argument ('MSG', type = str, nargs = '+',
                       help = 'Message to send to notifico')
  args = parser.parse_args (sysargs)
  #print ('ARGS:', repr (args), flush = True)
  return args

def colors (how):
  E = '\u001b['
  C = '\u0003'
  if how == 0:          # NONE
    d = { 'YELLOW': '', 'ORANGE': '', 'RED': '', 'GREEN': '', 'CYAN': '', 'BLUE': '', 'MAGENTA': '', 'RESET': '' }
  elif how == 1:        # ANSI
    d = { 'YELLOW': E+'93m', 'ORANGE': E+'33m', 'RED': E+'31m', 'GREEN': E+'32m', 'CYAN': E+'36m', 'BLUE': E+'34m', 'MAGENTA': E+'35m', 'RESET': E+'m' }
  elif how == 2:        # mIRC
    d = { 'YELLOW': C+'08,99', 'ORANGE': C+'07,99', 'RED': C+'04,99', 'GREEN': C+'03,99', 'CYAN': C+'10,99', 'BLUE': C+'12,99', 'MAGENTA': C+'06,99', 'RESET': C+'99,99' }
  from collections import namedtuple
  colors = namedtuple ("Colors", d.keys()) (*d.values())
  return colors

ER = r'false|\bno\b|\bnot|\bfail|fatal|error|\bwarn|\bbug|\bbad|\bred|broken'
OK = r'true|\byes|\bok\b|success|\bpass|good|\bgreen'

def status_color (txt):
  if re.search (ER, txt, flags = re.IGNORECASE):
    return c.RED
  if re.search (OK, txt, flags = re.IGNORECASE):
    return c.GREEN
  return c.YELLOW

c = {}

def form_message (args):
  s = []
  if args.n:
    s += [ c.ORANGE + args.n + c.RESET ]
  if args.b:
    s += [ c.CYAN + args.b + c.RESET ]
  if args.s:
    s += [ '[' + status_color (args.s) + args.s.upper() + c.RESET + ']' ]
  s += args.MSG
  if args.u:
    s += [ ' ' + args.u ]
  return ' '.join (s)

args = parse_args (sys.argv[1:])
args_0 = vars (args)['0']
color1 = 0 if args_0 else 1
color2 = 0 if args_0 else 2

c = colors (color1)
apayload = form_message (args)
print (apayload)

hook = args.H if args.H else os.getenv ("NOTIFICO_PHOOK")
if hook:
  c = colors (color2)
  ipayload = form_message (args)
  import requests
  r = requests.get (hook, params = { 'payload': ipayload })
  host = re.sub (r'(\w+?://)?([^/]+).*', r'\1\2', hook)
  print ('REQUEST: STAUS=%s URL=%s/…' % (r.status_code, host), file = sys.stderr)
