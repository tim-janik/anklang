#!/usr/bin/env python3
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
import sys, os, re, socket, select, time, unicodedata, json, ssl
from collections import deque, namedtuple

# https://datatracker.ietf.org/doc/html/rfc1459

server = "irc.libera.chat"
port = 6697
channel = "#anklang2"
nickname = "YYBOT"
ircsock = None
timeout = 150
wait_timeout = 15000
socket_timeout = 30
max_line_bytes = 512
github_event_data = None
# Libera.Chat throttles message sending to 1 per 2 seconds, this applies
# to bots too, see https://libera.chat/guides/faq#flood-exemptions-for-bots
message_rate = 2.0
last_message = 0.0
replies = deque()
Message = namedtuple ("Message", "prefix command params")
have_echo_message = False # server confirms deliveries via echo-message cap
delivery_started = False # a PRIVMSG send was attempted, retrying could duplicate it
notified = False # all message echoes were verified, LIST failure must not fail the run

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
  msg = '\n'.join (clean_text (line) for line in ' '.join (args.message).split ('\n'))
  c = colors (how)
  if args.S:
    msg = '[' + status_color (args.S, c) + clean_text (args.S).upper() + c.RESET + '] ' + msg
  if args.D:
    msg = c.CYAN + clean_text (args.D) + c.RESET + ' ' + msg
  if args.U:
    msg = c.ORANGE + clean_text (args.U) + c.RESET + ' ' + msg
  if args.R:
    msg = '[' + c.BLUE + clean_text (args.R) + c.RESET + '] ' + msg
  return msg

def clean_text (text):
  return ''.join (' ' if unicodedata.category (c) == 'Cc' else c for c in text)

def encode_line (text):
  if any (c in text for c in '\r\n\0'):
    raise Fatal ('IRC command contains a line break or NUL')
  data = (text + '\r\n').encode ('utf8')
  if len (data) > max_line_bytes:
    raise Fatal ('IRC command exceeds the byte limit')
  return data

def message_lines ():
  target = (args.j or args.J or args.n).split (' ')[0]
  if not args.message:
    return target, []
  budget = max_line_bytes - len (('PRIVMSG ' + target + ' :\r\n').encode ('utf8'))
  if budget < 4:
    raise Fatal ('IRC message target is too long')
  lines = []
  for line in re.split ('\n ?', format_msg (args)):
    chunk = ''
    size = 0
    for char in line:
      width = len (char.encode ('utf8'))
      if size + width > budget:
        lines.append (chunk)
        chunk, size = '', 0
      chunk += char
      size += width
    if chunk:
      lines.append (chunk)
  return target, lines

def validate_commands ():
  for value in (args.n, args.s, args.j, args.J):
    if clean_text (value) != value:
      raise Fatal ('IRC connection arguments contain control characters')
  if not args.n or any (c.isspace() for c in args.n) or args.n.startswith (':'):
    raise Fatal ('Invalid IRC nickname')
  target, lines = message_lines()
  if args.message and not lines:
    raise Fatal ('Message has no text to send')
  commands = ['USER ' + args.n + ' localhost ' + args.s + ' :' + args.n, 'NICK ' + args.n]
  if args.j:
    commands.append ('JOIN ' + args.j)
  if os.getenv ('IRCBOT_PASS'):
    commands.append ('PASS ' + os.environ['IRCBOT_PASS'])
  for command in commands + ['PRIVMSG ' + target + ' :' + line for line in lines]:
    encode_line (command)

def sendline (text):
  global args
  data = encode_line (text)
  if not args.quiet:
    print ("PASS <redacted>" if text.split (" ", 1)[0].upper() == "PASS" else text, flush = True)
  previous_timeout = ircsock.gettimeout()
  try:
    ircsock.settimeout (socket_timeout)
    ircsock.sendall (data)
  finally:
    ircsock.settimeout (previous_timeout)

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
  global readall_buffer, have_echo_message
  close_socket()
  readall_buffer = b''
  replies.clear()
  have_echo_message = False

def connect (server, port):
  global ircsock
  ircsock = socket.socket (socket.AF_INET, socket.SOCK_STREAM)
  ircsock.settimeout (socket_timeout) # connect and TLS handshake must not hang CI forever
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
  if not canread (milliseconds):
    return False
  previous_timeout = ircsock.gettimeout()
  try:
    ircsock.settimeout (max (0.001, min (socket_timeout, milliseconds * 0.001)))
    buf = ircsock.recv (128 * 1024)
  finally:
    ircsock.settimeout (previous_timeout)
  if not buf:
    raise ConnectionError ('SOCKET CLOSED: connection lost') # triggers session retry
  readall_buffer += buf
  if b'\n' in readall_buffer:
    lines, readall_buffer = readall_buffer.rsplit (b'\n', 1)
    for line in lines.decode ('utf8', 'replace').split ('\n'):
      if line:
        gotline (line.removesuffix ('\r'))
  return True

class Fatal (Exception):
  pass # non-retryable session failure (e.g. server ban)

def waitfor (pred, milliseconds = wait_timeout):
  # Read incoming replies until pred (reply) matches, returns the matched reply
  endtime = time.monotonic() + milliseconds * 0.001
  while True:
    while replies:
      reply = replies.popleft()
      if pred (reply):
        return reply
    remaining = endtime - time.monotonic()
    if remaining <= 0:
      raise TimeoutError ('TIMEOUT: no matching reply within ' + str (milliseconds) + 'ms')
    readall (min (remaining * 1000, 100))

def throttle ():
  # Sleep long enough to respect Libera.Chat's message rate limit
  global last_message
  elapsed = time.monotonic() - last_message
  if elapsed < message_rate:
    time.sleep (message_rate - elapsed)
  last_message = time.monotonic()

def is_printable(c):
  # Catch control sequences like:
  # c29f → U+009F (C1 control character: "Next Line").
  # c290 → U+0090 (C1 control character: "Cancel Line").
  # c287 → U+0087 (C0 control character: "Cancel Character").
  return unicodedata.category(c)[0] != 'C'

def parse_line (line):
  if line.startswith ('@'):
    line = line.partition (' ')[2]
  prefix = ''
  if line.startswith (':'):
    prefix, _, line = line[1:].partition (' ')
  middle, separator, trailing = line.partition (' :')
  words = middle.split()
  if not words:
    return Message (prefix, '', [])
  params = words[1:] + ([trailing] if separator else [])
  return Message (prefix, words[0].upper(), params)

def gotline (line):
  if not args.quiet:
    print (''.join (c for c in line if is_printable (c)), flush = True)
  reply = parse_line (line)
  if reply.command == '465':
    raise Fatal ('server ban (465), not retrying')
  if reply.command == 'PING':
    if reply.params:
      sendline ('PONG ' + ' '.join (reply.params[:-1] + [':' + reply.params[-1]]))
  elif reply.command:
    replies.append (reply)

def register_nick ():
  # Wait for registration (001), retry with a suffixed nick on 433 (in use)
  for i in range (3):
    reply = waitfor (lambda r:
      (r.command == '001' and bool (r.params)) or
      (r.command == '433' and len (r.params) >= 2 and r.params[1] == args.n))
    if reply.command == '001':
      args.n = reply.params[0]
      return
    if i < 2:
      args.n += '_' # 433: nickname is already in use
      sendline ("NICK " + args.n)
  raise Exception ('NICK: nickname already in use, all retries failed')

def expect (what):
  commands = what if isinstance (what, (list, tuple)) else [what]
  return waitfor (lambda r: r.command in commands)

usage_help = '''
Simple IRC bot for short messages.
A password for authentication can be set via $IRCBOT_PASS.
Connection failures before sending are retried up to 3 times.
Sending requires echo-message support; each message must be echoed back.
Once sending starts, failures stop the bot without resending messages.
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

def register_connection ():
  # CAP negotiation for echo-message (delivery verification), then USER/NICK
  global have_echo_message
  # echo-message makes the server send back our own messages, this is
  # how deliveries are verified without channel operator privileges.
  # List-only sessions need no echo verification, so they skip CAP entirely
  # and rely on register_nick() plus expect('251') below.
  if args.message:
    sendline ("CAP LS 302") # IRCv3: CAP negotiation starts with CAP LS
    sendline ("CAP REQ :echo-message")
    ackline = waitfor (lambda r: r.command == 'CAP' and len (r.params) >= 3 and
      r.params[1] in ('ACK', 'NAK') and 'echo-message' in r.params[-1].split())
    have_echo_message = ackline.params[1] == 'ACK' and 'echo-message' in ackline.params[-1].split()
    if not have_echo_message:
      raise Fatal ('server lacks echo-message; refusing unverified delivery')
  else:
    have_echo_message = False
  ircbot_pass = os.getenv ("IRCBOT_PASS")
  if ircbot_pass:
    sendline ("PASS " + ircbot_pass)
  sendline ("USER " + args.n + " localhost " + args.s + " :" + args.n)
  sendline ("NICK " + args.n)
  if args.message:
    sendline ("CAP END")
  register_nick()
  expect ('251') # LUSER reply

def run_session ():
  # One IRC session: connect, register, join, send (and verify) the message
  global delivery_started, notified
  reset_session_state()
  validate_commands()
  connect (args.s, args.p)
  readall (500)
  register_connection()

  if args.ping:
    sendline ("PING :pleasegetbacktome")
    waitfor (lambda r: r.command == 'PONG' and r.params and r.params[-1] == 'pleasegetbacktome')

  if args.j:
    sendline ("JOIN " + args.j)
    target = args.j.split (' ')[0]
    reply = waitfor (lambda r:
      (r.command == 'JOIN' and r.prefix.split ('!', 1)[0] == args.n and r.params == [target]) or
      (r.command in ('403', '471', '473', '474', '475') and len (r.params) >= 2 and r.params[1] == target))
    if reply.command == '471':
      raise Exception ('JOIN rejected, channel full (471)')
    if reply.command != 'JOIN':
      raise Fatal ('JOIN rejected: ' + reply.command)

  target, lines = message_lines()
  for line in lines:
    throttle() # Libera.Chat allows 1 message per 2 seconds
    delivery_started = True
    sendline ("PRIVMSG " + target + " :" + line)
    waitfor (lambda r: r.command == 'PRIVMSG' and r.prefix.split ('!', 1)[0] == args.n and r.params == [target, line])
  if lines:
    notified = True

  if args.l:
    sendline ("LIST")
    expect ('323')

  try:
    readall (500)
    sendline ("QUIT :Bye Bye")
    expect (['QUIT', 'ERROR'])
  except Exception:
    pass
  close_socket()

def main (sysargs):
  global args, github_event_data, delivery_started, notified
  args = parse_args (sysargs)
  github_event_data = None
  delivery_started = False
  notified = False

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
  orig_nick = args.n
  delivered = False
  attempts = 3
  for attempt in range (1, attempts + 1):
    try:
      args.n = orig_nick
      run_session()
      delivered = True
      result = 'delivered (echo verified)' if notified else 'channel list received'
      print (f'IRC: {result} on attempt {attempt}/{attempts}', file = sys.stderr, flush = True)
      break
    except Fatal as e:
      print (f'IRC: fatal: {e}', file = sys.stderr, flush = True)
      close_socket()
      break # don't retry a server ban
    except Exception as e:
      if notified:
        # All message echoes were verified; a later LIST or QUIT failure
        # must not report the notification as failed or resend it.
        print (f'IRC: delivered (echo verified) on attempt {attempt}/{attempts}', file = sys.stderr, flush = True)
        delivered = True
        break
      print (f'IRC: attempt {attempt}/{attempts} failed: {e}', file = sys.stderr, flush = True)
      close_socket()
      if delivery_started:
        print ('IRC: delivery may have occurred; not retrying', file = sys.stderr, flush = True)
        break
      if attempt < attempts:
        time.sleep (5 * 2 ** (attempt - 1)) # exponential backoff before reconnecting
  # Nonzero exit is reserved for notification failures; a failed build is
  # communicated via the [FAILURE] tag and GitHub's own run conclusion
  if not delivered:
    sys.exit (1)


if __name__ == "__main__":
  main (sys.argv[1:])
