#!/usr/bin/env python3
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
import sys, os, re, socket, select, time, unicodedata, json, ssl

# https://datatracker.ietf.org/doc/html/rfc1459

server = "irc.libera.chat"
port = 6697
channel = "#anklang2"
nickname = "YYBOT"
ircsock = None
timeout = 150
wait_timeout = 15000
github_event_data = None
# Libera.Chat throttles message sending to 1 per 2 seconds, this applies
# to bots too, see https://libera.chat/guides/faq#flood-exemptions-for-bots
message_rate = 2.0
last_message = 0.0
captured_lines = [] # lines collected while capturing=True
capturing = False
have_echo_message = False # server confirms deliveries via echo-message cap

def colors (how):
  E = '\u001b['
  C = '\u0003'
  if how == 0:          # NONE
    d = { 'YELLOW': '', 'ORANGE': '', 'RED': '', 'GREEN': '', 'CYAN': '', 'BLUE': '', 'MAGENTA': '', 'RESET': '' }
  elif how == 1:        # ANSI
    d = { 'YELLOW': E+'93m', 'ORANGE': E+'33m', 'RED': E+'31m', 'GREEN': E+'32m', 'CYAN': E+'36m', 'BLUE': E+'34m', 'MAGENTA': E+'35m', 'RESET': E+'m' }
  elif how == 2:        # mIRC
    d = { 'YELLOW': C+'08,99', 'ORANGE': C+'07,99', 'RED': C+'04,99', 'GREEN': C+'03,99', 'CYAN': C+'10,99', 'BLUE': C+'12,99', 'MAGENTA': C+'06,99', 'RESET': C+'' }
  from collections import namedtuple
  colors = namedtuple ("Colors", d.keys()) (*d.values())
  return colors

def status_color (txt, c):
  ER = r'false|\bno\b|\bnot|\bfail|fatal|error|\bwarn|\bbug|\bbad|\bred|broken'
  OK = r'true|\byes|\bok\b|success|\bpass|good|\bgreen'
  if re.search (ER, txt, flags = re.IGNORECASE):
    return c.RED
  if re.search (OK, txt, flags = re.IGNORECASE):
    return c.GREEN
  return c.YELLOW

def format_msg (args, how = 2):
  msg = ' '.join (args.message)
  c = colors (how)
  if args.S:
    msg = '[' + status_color (args.S, c) + args.S.upper() + c.RESET + '] ' + msg
  if args.D:
    msg = c.CYAN + args.D + c.RESET + ' ' + msg
  if args.U:
    msg = c.ORANGE + args.U + c.RESET + ' ' + msg
  if args.R:
    msg = '[' + c.BLUE + args.R + c.RESET + '] ' + msg
  return msg

def sendline (text):
  global args
  if not args.quiet:
    print (text, flush = True)
  msg = text + "\r\n"
  ircsock.send (msg.encode ('utf8'))

def close_socket ():
  global ircsock
  if ircsock:
    try:
      ircsock.close()
    except OSError:
      pass
    ircsock = None

def reset_session_state ():
  # fresh state per attempt, so retries aren't confused by leftover data
  global readall_buffer, expecting_commands, check_cmds, capturing
  close_socket()
  readall_buffer = b''
  expecting_commands = []
  check_cmds = []
  seen_cmds.clear()
  captured_lines.clear()
  capturing = False

def connect (server, port):
  global ircsock
  ircsock = socket.socket (socket.AF_INET, socket.SOCK_STREAM)
  ircsock.settimeout (30) # connect and TLS handshake must not hang CI forever
  if args.tls:
    ctx = ssl.create_default_context()
    ircsock = ctx.wrap_socket (ircsock, server_hostname = server)
  ircsock.connect ((server, port))
  ircsock.setblocking (True) # removes the timeout, reads are select() driven

def canread (milliseconds):
  if hasattr (ircsock, 'pending') and ircsock.pending() > 0:
    return True
  rs, ws, es = select.select ([ ircsock ], [], [], milliseconds * 0.001)
  return ircsock in rs

readall_buffer = b'' # unterminated start of next line
def readall (milliseconds = timeout):
  global readall_buffer
  gotlines = False
  while canread (milliseconds):
    milliseconds = 0
    buf = ircsock.recv (128 * 1024)
    if len (buf) == 0:
      raise Exception ('SOCKET CLOSED: connection lost') # triggers session retry
    gotlines = True
    readall_buffer += buf
    if readall_buffer.find (b'\n') >= 0:
      lines, readall_buffer = readall_buffer.rsplit (b'\n', 1)
      lines = lines.decode ('utf8', 'replace')
      for l in lines.split ('\n'):
        if l:
          gotline (l.rstrip())
  return gotlines

class Fatal (Exception):
  pass # non-retryable session failure (e.g. server ban)

def waitfor (pred, milliseconds = wait_timeout):
  # Read incoming lines until pred (line) matches, returns the matched line
  global capturing
  endtime = time.time() + milliseconds * 0.001
  capturing = True
  captured_lines.clear()
  try:
    while True:
      try:
        readall (100)
      except Exception:
        # socket closed: final check of captured lines before propagating
        for l in captured_lines:
          if pred (l):
            return l
        raise
      for l in captured_lines:
        if pred (l):
          return l
      captured_lines.clear()
      if time.time() >= endtime:
        raise Exception ('TIMEOUT: no matching reply within ' + str (milliseconds) + 'ms')
  finally:
    capturing = False

def throttle ():
  # Sleep long enough to respect Libera.Chat's message rate limit
  global last_message
  elapsed = time.time() - last_message
  if elapsed < message_rate:
    time.sleep (message_rate - elapsed)
  last_message = time.time()

def is_printable(c):
  # Catch control sequences like:
  # c29f → U+009F (C1 control character: "Next Line").
  # c290 → U+0090 (C1 control character: "Cancel Line").
  # c287 → U+0087 (C0 control character: "Cancel Character").
  return unicodedata.category(c)[0] != 'C'

def gotline (msg):
  global args
  if capturing:
    captured_lines.append (msg)
  if not args.quiet:
    filtered_msg = ''.join (c for c in msg if is_printable (c))
    print (filtered_msg, flush = True)
  cmdargs = re.split (' +', msg)
  if cmdargs:
    prefix = ''
    if cmdargs[0] and cmdargs[0][0] == ':':
      prefix = cmdargs[0]
      cmdargs = cmdargs[1:]
      if not cmdargs:
        return
    gotcmd (prefix, cmdargs[0], cmdargs[1:])

expecting_commands = []
check_cmds = []
seen_cmds = [] # all commands seen so far, includes cmds seen during waitfor()
def gotcmd (prefix, cmd, args):
  global expecting_commands, check_cmds
  seen_cmds.append (cmd)
  if check_cmds:
    try: check_cmds.remove (cmd)
    except: pass
  if cmd in expecting_commands:
    expecting_commands = []
  if cmd == 'PING':
    return sendline ('PONG ' + ' '.join (args))

def register_nick ():
  # Wait for registration (001), retry with a suffixed nick on 433 (in use)
  global args
  for i in range (3):
    reply = waitfor (lambda l: re.search (r'\b(001|433|465)\b', l))
    if re.search (r'\b001\b', reply):
      return
    if re.search (r'\b465\b', reply):
      raise Fatal ('server ban (465), not retrying: ' + reply)
    args.n += '_' # 433: nickname is already in use
    sendline ("NICK " + args.n)
  raise Exception ('NICK: nickname already in use, all retries failed')

def expect (what = []):
  global expecting_commands
  expecting_commands = what if isinstance (what, (list, tuple)) else [ what ]
  for c in seen_cmds: # handle commands seen during earlier waitfor() calls
    if c in expecting_commands:
      expecting_commands = []
  while expecting_commands and readall (wait_timeout): pass
  if expecting_commands:
    raise (Exception ('MISSING REPLY: ' + ' | '.join (expecting_commands)))

usage_help = '''
Simple IRC bot for short messages.
A password for authentication can be set via $IRCBOT_PASS.
Connection failures and unverified messages are retried 3 times; a
message only counts as delivered once the server echoed it back.
Messages are throttled to Libera.Chat's rate limit of 1 per 2 seconds,
see https://libera.chat/guides/faq#flood-exemptions-for-bots
With -G, repository, user, branch, commit subject and URL are auto-filled
from $GITHUB_EVENT_PATH, the overall job status from $IRCBOT_JOBS.
'''

def parse_args (sysargs):
  import argparse
  global server, port, nickname, argparser
  parser = argparse.ArgumentParser (description = usage_help)
  parser.add_argument ('message', metavar = 'messages', type = str, nargs = '*',
                       help = 'Message to post on IRC')
  parser.add_argument ('-j', metavar = 'CHANNEL', default = '',
                       help = 'Channel to join on IRC')
  parser.add_argument ('-J', metavar = 'CHANNEL', default = '',
                       help = 'Message channel without joining')
  parser.add_argument ('-n', metavar = 'NICK', default = nickname,
                       help = 'Nickname to use on IRC [' + nickname + ']')
  parser.add_argument ('-s', metavar = 'SERVER', default = server,
                       help = 'Server for IRC connection [' + server + ']')
  parser.add_argument ('-p', metavar = 'PORT', default = port, type = int,
                       help = 'Port to connect to [' + str (port) + ']')
  parser.add_argument ('-l', action = "store_true",
                       help = 'List channels')
  parser.add_argument ('-G', action = "store_true",
                       help = 'Read notification bits from $GITHUB_EVENT_PATH')
  parser.add_argument ('-R', metavar = 'REPOSITORY', default = '',
                       help = 'Initiating repository name')
  parser.add_argument ('-U', metavar = 'NAME', default = '',
                       help = 'Initiating user name')
  parser.add_argument ('-D', metavar = 'DEPARTMENT', default = '',
                       help = 'Initiating department')
  parser.add_argument ('-S', metavar = 'STATUS', default = '',
                       help = 'Initiating status code')
  parser.add_argument ('--no-tls', action = "store_false", dest = 'tls', default = True,
                       help = 'Disable TLS encryption, plaintext connection')
  parser.add_argument ('--ping', action = "store_true",
                       help = 'Require PING/PONG after connecting')
  parser.add_argument ('--quiet', '-q', action = "store_true",
                       help = 'Avoid unnecessary output')
  argparser = parser
  args = parser.parse_args (sysargs)
  #print ('ARGS:', repr (args), flush = True)
  return args

args = parse_args (sys.argv[1:])

if args.G:
  # $GITHUB_EVENT_PATH holds the verbatim webhook payload of the triggering event;
  # the field layout of each event type is documented at
  #   https://docs.github.com/en/webhooks/webhook-events-and-payloads
  # with machine readable schemas at
  #   https://github.com/octokit/webhooks/tree/main/payload-schemas/api.github.com
  # note: payloads drift from these schemas in both directions (e.g. head_commit
  # can be null, sender.user_view_type is payload-only), read all fields defensively
  event_path = os.getenv ('GITHUB_EVENT_PATH')
  if event_path and os.path.exists (event_path):
    with open (event_path, 'r') as f:
      github_event_data = json.load (f)

# Derive announcement fields from the event payload; which events reach the bot
# is decided by the calling workflow, the bot handles all payload shapes.
if github_event_data:
  ev = github_event_data
  R = (ev.get ('repository') or {}).get ('full_name', '')
  args.R = R if R else args.R
  U = (ev.get ('pusher') or {}).get ('name', '')
  args.U = U if U else args.U
  ref = ev.get ('ref') or ''
  if ref:
    args.D = re.sub (r'^refs/(heads|tags)/', '', ref) # branch or tag name
  head = ev.get ('head_commit') or {} # schema allows null
  pr = ev.get ('pull_request') or {} # pull_request payloads: no ref/pusher/head_commit
  subject = (head.get ('message', '').splitlines() or [ '' ])[0] # commit subject line
  if pr and not subject: # pull_request payload: announce "action: title"
    if pr.get ('number'):
      args.D = '#' + str (pr['number'])
    args.U = args.U or (ev.get ('sender') or {}).get ('login', '')
    subject = pr.get ('title', '')
    if subject and ev.get ('action'):
      subject = ev['action'] + ': ' + subject
  url = head.get ('url') or pr.get ('html_url') or ''
  if not args.message and subject: # default message: commit subject or PR title
    args.message = [ subject ]
  if url and args.message:
    args.message += [ '-', url ]
  # overall job status: IRCBOT_JOBS passes the workflow's needs.*.result values
  # joined by spaces; anything but success|skipped is announced as FAILURE, the
  # run conclusion itself is handled by GitHub
  needs_results = os.getenv ('IRCBOT_JOBS', '').split()
  failed = [ r for r in needs_results if r not in ( 'success', 'skipped' ) ]
  if not args.S and needs_results:
    args.S = 'FAILURE' if failed else 'SUCCESS'
  print ('EVENT:', args.R or '-', args.U or '-', args.D or '-', url or '-', '| jobs:',
         ' '.join (needs_results) or '-', file = sys.stderr)

# Never open a remote connection without a message to deliver
if not args.message and not args.l:
  argparser.error ('a message is required (or -l to list channels)')

if args.message and not args.quiet:
  print (format_msg (args, 1))
def register_connection ():
  # CAP negotiation for echo-message (delivery verification), then USER/NICK
  global have_echo_message
  # echo-message makes the server send back our own messages, this is
  # how deliveries are verified without channel operator privileges
  sendline ("CAP LS 302") # IRCv3: CAP negotiation starts with CAP LS
  sendline ("CAP REQ :echo-message")
  ackline = waitfor (lambda l: re.search (r' CAP .* (ACK|NAK)', l))
  have_echo_message = not re.search (r' CAP .* NAK', ackline)
  if not have_echo_message:
    print ('IRC: server lacks echo-message, delivery cannot be verified', file = sys.stderr, flush = True)
  ircbot_pass = os.getenv ("IRCBOT_PASS")
  if ircbot_pass:
    sendline ("PASS " + ircbot_pass)
  sendline ("USER " + args.n + " localhost " + args.s + " :" + args.n)
  sendline ("NICK " + args.n)
  sendline ("CAP END")
  register_nick()
  expect ('251') # LUSER reply

def run_session ():
  # One IRC session: connect, register, join, send (and verify) the message
  reset_session_state()
  connect (args.s, args.p)
  readall (500)
  register_connection()

  if args.ping:
    sendline ("PING :pleasegetbacktome")
    expect ('PONG')

  if args.j:
    sendline ("JOIN " + args.j)
    # wait for the join echo or a rejection numeric (403, 471, 473, 474, 475)
    reply = waitfor (lambda l: re.search (r'\b(JOIN|403|471|473|474|475)\b', l))
    if re.search (r'\bJOIN\b', reply):
      pass # joined, join echo received
    elif re.search (r'\b471\b', reply):
      raise Exception ('JOIN rejected, channel full (471), retrying: ' + reply)
    else:
      raise Fatal ('JOIN rejected: ' + reply)

  msg = format_msg (args)
  for line in re.split ('\n ?', msg):
    channel = (args.j or args.J or args.n).split (' ')[0] # drop JOIN key part
    if line:
      throttle() # Libera.Chat allows 1 message per 2 seconds
      sendline ("PRIVMSG " + channel + " :" + line)
      if have_echo_message:
        # delivery is only confirmed once the server echoes the message back
        waitfor (lambda l: re.search (r' PRIVMSG ' + re.escape (channel) + r' :' + re.escape (line[:64]), l), 15000)
      else:
        readall()

  if args.l:
    global check_cmds
    sendline ("LIST")
    check_cmds = [ '322' ]
    expect ('323')
    if check_cmds:
      # empty list, retry after 60seconds
      time.sleep (30)
      check_cmds = [ 'PING' ]
      readall()
      if check_cmds:
        sendline ("PING :pleasegetbacktome")
        expect ('PONG')
      time.sleep (30)
      readall()
      sendline ("LIST")
      expect ('323')

  readall (500)
  try:
    sendline ("QUIT :Bye Bye")
    expect (['QUIT', 'ERROR'])
  except Exception:
    pass
  close_socket()

# Connection drops, missing replies and unverified messages are retried,
# the bot only exits successfully once delivery was confirmed by the server.
orig_nick = args.n
delivered = False
attempts = 3
for attempt in range (1, attempts + 1):
  try:
    args.n = orig_nick
    run_session()
    delivered = True
    verified = 'echo verified' if have_echo_message else 'unverified, no echo-message cap'
    print (f'IRC: delivered ({verified}) on attempt {attempt}/{attempts}', file = sys.stderr, flush = True)
    break
  except Fatal as e:
    print (f'IRC: fatal: {e}', file = sys.stderr, flush = True)
    close_socket()
    break # don't retry a server ban
  except Exception as e:
    print (f'IRC: attempt {attempt}/{attempts} failed: {e}', file = sys.stderr, flush = True)
    close_socket()
    if attempt < attempts:
      time.sleep (5 * 2 ** (attempt - 1)) # exponential backoff before reconnecting
# Nonzero exit is reserved for notification failures; a failed build is
# communicated via the [FAILURE] tag and GitHub's own run conclusion
if not delivered:
  sys.exit (1)
