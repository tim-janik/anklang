import contextlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import unittest
from unittest import mock


def deny_network (event, args):
  if event.startswith ('socket.'):
    raise AssertionError ('IRC tests must not use real sockets: ' + event)


sys.addaudithook (deny_network)


def load_bot ():
  spec = importlib.util.spec_from_file_location ('ircbot', Path (__file__).with_name ('ircbot.py'))
  bot = importlib.util.module_from_spec (spec)
  spec.loader.exec_module (bot)
  return bot


class FakeClock:
  def __init__ (self):
    self.now = 100.0

  def monotonic (self):
    return self.now

  def sleep (self, seconds):
    self.now += seconds


class MockServer:
  def __init__ (self, clock):
    self.clock = clock
    self.timeout = None
    self.incoming = []
    self.sent = []
    self.nick = 'YYBOT'
    self.closed = False
    self.respond = self.default_response

  def settimeout (self, value):
    self.timeout = value

  def sendall (self, data):
    if self.closed:
      raise ConnectionError ('mock socket is closed')
    line = data.decode().removesuffix ('\r\n')
    self.sent.append (line)
    self.respond (line)

  def recv (self, size):
    if self.closed:
      raise ConnectionError ('mock socket is closed')
    if self.incoming:
      return self.incoming.pop (0)
    self.clock.sleep (self.timeout)
    raise TimeoutError ('mock read timed out')

  def queue (self, *lines):
    self.incoming.append (('\r\n'.join (lines) + '\r\n').encode())

  def close (self):
    self.closed = True

  def default_response (self, line):
    if line == 'CAP REQ :echo-message':
      self.queue (':mock CAP * ACK :echo-message')
    elif line.startswith ('NICK '):
      self.nick = line[5:]
    elif line == 'CAP END':
      self.queue (f':mock 001 {self.nick} :welcome')
    elif line.startswith ('JOIN '):
      self.queue (f':{self.nick}!user@mock JOIN :' + line[5:])
    elif line.startswith ('PRIVMSG '):
      self.queue (f':{self.nick}!user@mock ' + line)


class BotTests (unittest.TestCase):
  def setUp (self):
    self.bot = load_bot()
    self.clock = FakeClock()

  def server (self):
    return MockServer (self.clock)

  @contextlib.contextmanager
  def connections (self, *servers):
    with contextlib.ExitStack() as stack:
      connect = stack.enter_context (mock.patch.object (self.bot, 'open_connection', side_effect = servers))
      stack.enter_context (mock.patch.object (self.bot.time, 'monotonic', side_effect = self.clock.monotonic))
      stack.enter_context (mock.patch.object (self.bot.time, 'sleep', side_effect = self.clock.sleep))
      stack.enter_context (contextlib.redirect_stderr (io.StringIO()))
      yield connect

  def test_import_has_no_cli_or_network_side_effects (self):
    with mock.patch.object (sys, 'argv', ['ircbot.py', '--invalid-option']):
      load_bot()

  def test_network_guard_blocks_socket_creation (self):
    with self.assertRaisesRegex (AssertionError, 'must not use real sockets'):
      self.bot.socket.socket()

  def test_dry_run_formats_ansi_colors_without_connecting (self):
    output = io.StringIO()
    with mock.patch.object (self.bot, 'open_connection', side_effect = AssertionError ('unexpected connection')):
      with contextlib.redirect_stdout (output):
        status = self.bot.main (['-n', '-S', 'success', '-R', 'owner/repo', '-U', 'author', '-D', 'trunk', '#test', 'hello'])
    self.assertEqual (status, 0)
    self.assertIn ('\x1b[', output.getvalue())
    self.assertIn ('owner/repo', output.getvalue())
    self.assertIn ('SUCCESS', output.getvalue())
    self.assertNotIn ('\x03', output.getvalue())

  def test_remote_message_uses_irc_colors (self):
    args = self.bot.parse_args (['-S', 'failure', '-R', 'owner/repo', '#test', 'hello'])
    message = self.bot.format_message (args)
    server = self.server()
    with self.connections (server):
      self.bot.send_notification ('#test', message)
    sent = next (line for line in server.sent if line.startswith ('PRIVMSG '))
    self.assertIn ('\x03', sent)
    self.assertNotIn ('\x1b[', sent)

  def test_github_event_supplies_message_fields (self):
    event = {
      'repository': {'full_name': 'owner/repo'},
      'pusher': {'name': 'author\r\nQUIT'},
      'ref': 'refs/heads/trunk',
      'head_commit': {'message': 'Fix the bot\nDetails', 'url': 'https://example.invalid/commit'},
    }
    output = io.StringIO()
    environment = {'GITHUB_EVENT_PATH': 'mock-event.json', 'IRCBOT_JOBS': 'success failure'}
    with mock.patch.dict (self.bot.os.environ, environment, clear = True):
      with mock.patch ('builtins.open', mock.mock_open (read_data = json.dumps (event))):
        with contextlib.redirect_stdout (output):
          status = self.bot.main (['-n', '-G', '#test'])
    self.assertEqual (status, 0)
    self.assertIn ('FAILURE', output.getvalue())
    self.assertIn ('author  QUIT', output.getvalue())
    self.assertIn ('Fix the bot - https://example.invalid/commit', output.getvalue())
    self.assertNotIn ('Details', output.getvalue())

  def test_invalid_channel_fails_before_connecting (self):
    for channel in ('test', '#bad channel', '#bad\r\nQUIT', '#bad,channel'):
      with self.subTest (channel = channel):
        with mock.patch.object (self.bot, 'open_connection') as connect:
          with self.assertRaisesRegex (self.bot.Fatal, 'invalid IRC channel'):
            self.bot.send_notification (channel, 'hello')
        connect.assert_not_called()

  def test_one_unicode_message_is_bounded_and_closed (self):
    server = self.server()
    with self.connections (server):
      self.bot.send_notification ('#test', 'Grüße 🌻 ' * 200)
    messages = [line for line in server.sent if line.startswith ('PRIVMSG ')]
    self.assertEqual (len (messages), 1)
    self.assertLessEqual (len ((messages[0] + '\r\n').encode()), self.bot.max_privmsg_bytes)
    self.assertTrue (messages[0].endswith ('...'))
    self.assertTrue (server.closed)

  def test_control_characters_cannot_add_commands (self):
    server = self.server()
    with self.connections (server):
      self.bot.send_notification ('#test', 'hello\r\nQUIT :bad')
    messages = [line for line in server.sent if line.startswith ('PRIVMSG ')]
    self.assertEqual (messages, ['PRIVMSG #test :hello  QUIT :bad'])
    self.assertFalse (any (line.startswith ('QUIT ') for line in server.sent))

  def test_registration_completes_at_001 (self):
    server = self.server()
    with self.connections (server):
      self.bot.send_notification ('#test', 'hello')
    self.assertIn ('PRIVMSG #test :hello', server.sent)

  def test_erroneous_nickname_fails_immediately (self):
    server = self.server()

    def respond (line):
      if line.startswith ('NICK '):
        server.queue (':mock 432 * YYBOT :Erroneous nickname')
      elif line != 'CAP END':
        server.default_response (line)

    server.respond = respond
    with self.connections (server) as connect:
      with self.assertRaisesRegex (self.bot.Fatal, '432:.*Erroneous nickname'):
        self.bot.send_notification ('#test', 'hello')
    self.assertEqual (connect.call_count, 1)
    self.assertFalse (any (line.startswith ('PRIVMSG ') for line in server.sent))

  def test_nickname_collision_uses_a_suffix (self):
    server = self.server()

    def respond (line):
      if line == 'NICK YYBOT':
        server.nick = 'YYBOT'
        server.queue (':mock 433 * YYBOT :Nickname in use')
      elif line == 'NICK YYBOT_':
        server.nick = 'YYBOT_'
        server.queue (':mock 001 YYBOT_ :welcome')
      elif line != 'CAP END':
        server.default_response (line)

    server.respond = respond
    with self.connections (server):
      self.bot.send_notification ('#test', 'hello')
    self.assertIn ('NICK YYBOT_', server.sent)
    self.assertIn ('PRIVMSG #test :hello', server.sent)

  def test_modified_echo_acknowledges_delivery (self):
    server = self.server()

    def respond (line):
      if line.startswith ('PRIVMSG '):
        server.queue (':yybot!user@mock PRIVMSG #TEST :server changed the text')
      else:
        server.default_response (line)

    server.respond = respond
    with self.connections (server):
      self.bot.send_notification ('#test', 'original text')
    self.assertIn ('PRIVMSG #test :original text', server.sent)

  def test_wrong_echo_does_not_acknowledge_delivery (self):
    server = self.server()

    def respond (line):
      if line.startswith ('PRIVMSG '):
        server.queue (':other!user@mock PRIVMSG #test :hello')
      else:
        server.default_response (line)

    server.respond = respond
    with self.connections (server) as connect:
      with self.assertRaisesRegex (self.bot.Fatal, 'delivery may have occurred'):
        self.bot.send_notification ('#test', 'hello')
    self.assertEqual (connect.call_count, 1)

  def test_privmsg_errors_fail_immediately_with_reason (self):
    for response in (':mock 401 YYBOT #test :No such nick or channel',
                     ':mock 404 YYBOT #test :Cannot send to channel',
                     ':mock FAIL PRIVMSG INVALID_TARGET #test :Invalid target'):
      with self.subTest (response = response):
        server = self.server()

        def respond (line):
          if line.startswith ('PRIVMSG '):
            server.queue (response)
          else:
            server.default_response (line)

        server.respond = respond
        with self.connections (server) as connect:
          with self.assertRaisesRegex (self.bot.Fatal, 'PRIVMSG failed') as error:
            self.bot.send_notification ('#test', 'hello')
        self.assertIn (response.rpartition (' :')[2], str (error.exception))
        self.assertEqual (connect.call_count, 1)
        self.assertLess (self.clock.now, 101)

  def test_missing_echo_capability_prevents_sending (self):
    server = self.server()

    def respond (line):
      if line == 'CAP REQ :echo-message':
        server.queue (':mock CAP * NAK :echo-message')
      else:
        server.default_response (line)

    server.respond = respond
    with self.connections (server) as connect:
      with self.assertRaisesRegex (self.bot.Fatal, 'lacks echo-message'):
        self.bot.send_notification ('#test', 'hello')
    self.assertEqual (connect.call_count, 1)
    self.assertFalse (any (line.startswith ('PRIVMSG ') for line in server.sent))

  def test_lost_echo_does_not_resend (self):
    server = self.server()

    def respond (line):
      if not line.startswith ('PRIVMSG '):
        server.default_response (line)

    server.respond = respond
    with self.connections (server) as connect:
      with self.assertRaisesRegex (self.bot.Fatal, 'delivery may have occurred'):
        self.bot.send_notification ('#test', 'hello')
    self.assertEqual (connect.call_count, 1)
    self.assertEqual (server.sent.count ('PRIVMSG #test :hello'), 1)

  def test_retry_uses_a_fresh_socket (self):
    failed = self.server()
    working = self.server()
    failed.respond = lambda line: None
    with self.connections (failed, working) as connect:
      self.bot.send_notification ('#test', 'hello')
    self.assertEqual (connect.call_count, 2)
    self.assertTrue (failed.closed)
    self.assertTrue (working.closed)
    self.assertFalse (any (line.startswith ('PRIVMSG ') for line in failed.sent))
    self.assertIn ('PRIVMSG #test :hello', working.sent)

  def test_fragmented_utf8_reply_is_reassembled (self):
    server = self.server()
    client = self.bot.IrcClient (server)
    raw = ':mock NOTICE YYBOT :Grüße\r\n'.encode()
    server.incoming.extend (bytes ([byte]) for byte in raw)
    with mock.patch.object (self.bot.time, 'monotonic', side_effect = self.clock.monotonic):
      reply = client.wait (lambda message: message.command == 'NOTICE')
    self.assertEqual (reply.params, ['YYBOT', 'Grüße'])

  def test_continuous_pings_do_not_extend_deadline (self):
    server = self.server()
    client = self.bot.IrcClient (server)
    self.bot.reply_timeout = 0.05

    def recv (size):
      self.clock.sleep (0.01)
      return b'PING :still-here\r\n'

    server.recv = recv
    with mock.patch.object (self.bot.time, 'monotonic', side_effect = self.clock.monotonic):
      with self.assertRaises (TimeoutError):
        client.wait (lambda message: message.command == '001')
    self.assertLess (self.clock.now, 100.07)
    self.assertTrue (all (line == 'PONG :still-here' for line in server.sent))

  def test_receive_uses_the_remaining_deadline (self):
    server = self.server()
    client = self.bot.IrcClient (server)
    self.bot.reply_timeout = 1

    def recv (size):
      self.assertGreater (server.timeout, 0.9)
      self.clock.sleep (0.15)
      return b':mock 001 YYBOT :welcome\r\n'

    server.recv = recv
    with mock.patch.object (self.bot.time, 'monotonic', side_effect = self.clock.monotonic):
      reply = client.wait (lambda message: message.command == '001')
    self.assertEqual (reply.params[0], 'YYBOT')
    self.assertEqual (self.clock.now, 100.15)


if __name__ == '__main__':
  unittest.main()
