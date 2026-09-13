import contextlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import unittest
from unittest import mock


def deny_network (event, args):
  if event.startswith ("socket."):
    raise AssertionError ("IRC tests must not use real sockets: " + event)


sys.addaudithook (deny_network)


def load_bot ():
  spec = importlib.util.spec_from_file_location ("ircbot", Path (__file__).with_name ("ircbot.py"))
  bot = importlib.util.module_from_spec (spec)
  spec.loader.exec_module (bot)
  return bot


class PartialSocket:
  def __init__ (self, stalled = False):
    self.data = bytearray()
    self.timeout = None
    self.write_timeout = None
    self.stalled = stalled

  def gettimeout (self):
    return self.timeout

  def settimeout (self, value):
    self.timeout = value

  def send (self, data):
    self.write_timeout = self.timeout
    if self.stalled:
      raise TimeoutError ("mock write timed out")
    count = min (3, len (data))
    self.data.extend (data[:count])
    return count

  def sendall (self, data):
    while data:
      data = data[self.send (data):]


class FakeClock:
  def __init__ (self):
    self.now = 100.0

  def monotonic (self):
    return self.now

  def sleep (self, seconds):
    self.now += seconds


class MockServer (PartialSocket):
  def __init__ (self):
    super().__init__()
    self.incoming = []
    self.sent = []
    self.nick = 'YYBOT'
    self.closed = False
    self.address = None
    self.respond = self.default_response

  def connect (self, address):
    self.address = address

  def setblocking (self, blocking):
    self.timeout = None if blocking else 0

  def close (self):
    self.closed = True

  def pending (self):
    return len (self.incoming)

  def recv (self, size):
    return self.incoming.pop (0)

  def queue (self, *lines):
    self.incoming.append (('\r\n'.join (lines) + '\r\n').encode())

  def sendall (self, data):
    super().sendall (data)
    line = data.decode().removesuffix ('\r\n')
    self.sent.append (line)
    self.respond (line)

  def default_response (self, line):
    if line == 'CAP REQ :echo-message':
      self.queue (':mock CAP * ACK :echo-message')
    elif line.startswith ('NICK '):
      self.nick = line[5:]
      self.queue (f':mock 001 {self.nick} :welcome', f':mock 251 {self.nick} :users')
    elif line == 'CAP END':
      self.queue (f':mock 001 {self.nick} :welcome', f':mock 251 {self.nick} :users')
    elif line.startswith ('JOIN '):
      self.queue (f':{self.nick}!user@mock JOIN :' + line[5:].split (' ')[0])
    elif line.startswith ('PRIVMSG '):
      self.queue (f':{self.nick}!user@mock ' + line)
    elif line.startswith ('PING '):
      self.queue (':mock PONG mock ' + line[5:])
    elif line == 'LIST':
      self.queue (f':mock 323 {self.nick} :End of LIST')
    elif line.startswith ('QUIT '):
      self.queue ('ERROR :Closing link')


class BotTests (unittest.TestCase):
  def setUp (self):
    self.bot = load_bot()
    self.bot.args = self.bot.parse_args (["-j", "#test", "hello"])
    self.bot.ircsock = mock.Mock()

  @contextlib.contextmanager
  def mock_server (self, server = None):
    server = server or MockServer()
    clock = FakeClock()

    def select_ready (readers, writers, errors, seconds):
      clock.sleep (0.001 if server.incoming else seconds)
      return (readers if server.incoming else [], [], [])

    with contextlib.ExitStack() as stack:
      stack.enter_context (mock.patch.object (self.bot.socket, 'socket', return_value = server))
      tls = stack.enter_context (mock.patch.object (self.bot.ssl, 'create_default_context'))
      tls.return_value.wrap_socket.return_value = server
      stack.enter_context (mock.patch.object (self.bot.select, 'select', side_effect = select_ready))
      stack.enter_context (mock.patch.object (self.bot.time, 'monotonic', side_effect = clock.monotonic))
      stack.enter_context (mock.patch.object (self.bot.time, 'sleep', side_effect = clock.sleep))
      stack.enter_context (mock.patch.dict (self.bot.os.environ, {}, clear = True))
      self.bot.args = self.bot.parse_args (['-q', '-s', 'mock.invalid', '-j', '#test', 'hello'])
      yield server, clock

  def test_import_has_no_cli_or_network_side_effects (self):
    with mock.patch.object (sys, "argv", ["ircbot.py", "--invalid-option"]):
      with mock.patch ("time.sleep", side_effect = AssertionError ("unexpected sleep")):
        load_bot()

  def test_password_is_redacted_but_sent (self):
    for command in ("PASS", "pass"):
      with self.subTest (command = command):
        output = io.StringIO()
        with contextlib.redirect_stdout (output):
          self.bot.sendline (command + " dummy-password")
        self.assertNotIn ("dummy-password", output.getvalue())
        self.assertEqual (output.getvalue(), "PASS <redacted>\n")
        self.bot.ircsock.sendall.assert_called_with ((command + " dummy-password\r\n").encode())

  def test_quiet_password_has_no_log (self):
    self.bot.args.quiet = True
    output = io.StringIO()
    with contextlib.redirect_stdout (output):
      self.bot.sendline ("PASS dummy-password")
    self.assertEqual (output.getvalue(), "")

  def test_ordinary_command_is_logged (self):
    output = io.StringIO()
    with contextlib.redirect_stdout (output):
      self.bot.sendline ("NICK test")
    self.assertEqual (output.getvalue(), "NICK test\n")

  def test_partial_writes_send_complete_utf8_command (self):
    self.bot.args.quiet = True
    self.bot.ircsock = PartialSocket()
    self.bot.sendline ("PRIVMSG #test :Grüße")
    self.assertEqual (self.bot.ircsock.data, "PRIVMSG #test :Grüße\r\n".encode())
    self.assertEqual (self.bot.ircsock.write_timeout, self.bot.socket_timeout)
    self.assertIsNone (self.bot.ircsock.timeout)

  def test_stalled_write_fails_and_restores_timeout (self):
    self.bot.args.quiet = True
    self.bot.ircsock = PartialSocket (stalled = True)
    self.bot.ircsock.timeout = 7
    with self.assertRaises (TimeoutError):
      self.bot.sendline ("PING :test")
    self.assertEqual (self.bot.ircsock.write_timeout, self.bot.socket_timeout)
    self.assertEqual (self.bot.ircsock.timeout, 7)

  def test_parser_preserves_tagged_message_and_trailing_spaces (self):
    reply = self.bot.parse_line ('@label=test :nick!u@host PRIVMSG #test :text with spaces  ')
    self.assertEqual (reply.prefix, 'nick!u@host')
    self.assertEqual (reply.command, 'PRIVMSG')
    self.assertEqual (reply.params, ['#test', 'text with spaces  '])

  def test_session_uses_only_mock_server (self):
    with self.mock_server() as (server, clock):
      self.bot.run_session()
      self.assertEqual (server.address, ('mock.invalid', self.bot.port))
      self.assertIn ('PRIVMSG #test :hello', server.sent)
      self.assertTrue (server.closed)

  def test_notice_text_does_not_complete_registration (self):
    self.bot.args.quiet = True
    self.bot.gotline (':mock NOTICE * :001 welcome; 433 unavailable; 465 banned')
    self.bot.gotline (':mock 001 actual-nick :welcome')
    self.bot.register_nick()
    self.assertEqual (self.bot.args.n, 'actual-nick')

  def test_batched_replies_are_preserved (self):
    with self.mock_server() as (server, clock):
      self.bot.ircsock = server
      server.queue (':mock 001 YYBOT :welcome', ':mock 251 YYBOT :users')
      self.bot.register_nick()
      reply = self.bot.expect ('251')
      self.assertEqual (reply.command, '251')

  def test_fragmented_utf8_reply_is_reassembled (self):
    with self.mock_server() as (server, clock):
      self.bot.ircsock = server
      raw = ':mock NOTICE YYBOT :Grüße  \r\n'.encode()
      server.incoming.extend (bytes ([byte]) for byte in raw)
      reply = self.bot.expect ('NOTICE')
      self.assertEqual (reply.params, ['YYBOT', 'Grüße  '])

  def test_nickname_collision_waits_for_replacement (self):
    self.bot.args.quiet = True
    self.bot.gotline (':mock 433 * unrelated :in use')
    self.bot.gotline (':mock 433 * YYBOT :in use')
    self.bot.gotline (':mock 001 YYBOT_ :welcome')
    self.bot.register_nick()
    self.assertEqual (self.bot.args.n, 'YYBOT_')
    self.bot.ircsock.sendall.assert_called_once_with (b'NICK YYBOT_\r\n')

  def test_capability_ack_ignores_unrelated_replies (self):
    with self.mock_server() as (server, clock):
      def respond (line):
        if line == 'CAP REQ :echo-message':
          server.queue (':mock NOTICE * :CAP * ACK :echo-message', ':mock CAP * ACK :unrelated')
        server.default_response (line)

      server.respond = respond
      self.bot.run_session()
      self.assertTrue (self.bot.have_echo_message)

  def test_used_reply_cannot_satisfy_later_wait (self):
    with self.mock_server() as (server, clock):
      self.bot.ircsock = server
      server.queue (':mock PONG mock :token')
      self.bot.expect ('PONG')
      with self.assertRaises (TimeoutError):
        self.bot.expect ('PONG')

  def test_continuous_pings_do_not_extend_deadline (self):
    with self.mock_server() as (server, clock):
      self.bot.ircsock = server
      original_read = self.bot.readall

      def flood (milliseconds):
        clock.sleep (0.01)
        server.queue ('PING :still-here')
        return original_read (milliseconds)

      with mock.patch.object (self.bot, 'readall', side_effect = flood):
        start = clock.now
        with self.assertRaises (TimeoutError):
          self.bot.waitfor (lambda r: r.command == '001', 50)
      self.assertLess (clock.now - start, 0.1)
      self.assertTrue (all (line == 'PONG :still-here' for line in server.sent))

  def test_real_ban_is_fatal (self):
    self.bot.args.quiet = True
    with self.assertRaises (self.bot.Fatal):
      self.bot.gotline (':mock 465 YYBOT :banned')

  def test_join_matches_own_nick_and_requested_channel (self):
    with self.mock_server() as (server, clock):
      def respond (line):
        if line.startswith ('JOIN '):
          server.queue (':other!u@mock JOIN :#test', ':YYBOT!u@mock JOIN :#other', ':mock NOTICE YYBOT :JOIN #test')
        else:
          server.default_response (line)

      server.respond = respond
      with self.assertRaises (TimeoutError):
        self.bot.run_session()
      self.assertFalse (any (line.startswith ('PRIVMSG ') for line in server.sent))

  def test_echo_matches_sender_target_and_entire_text (self):
    for echo in (':other!u@mock PRIVMSG #test :hello', ':YYBOT!u@mock PRIVMSG #other :hello',
                 ':YYBOT!u@mock PRIVMSG #test :hello extra', ':mock NOTICE YYBOT :PRIVMSG #test :hello'):
      with self.subTest (echo = echo), self.mock_server() as (server, clock):
        def respond (line):
          if line.startswith ('PRIVMSG '):
            server.queue (echo)
          else:
            server.default_response (line)

        server.respond = respond
        with self.assertRaises (TimeoutError):
          self.bot.run_session()

  def test_echo_retains_trailing_spaces (self):
    with self.mock_server() as (server, clock):
      self.bot.args.message = ['hello  ']
      self.bot.run_session()
      self.assertIn ('PRIVMSG #test :hello  ', server.sent)

  def test_reset_discards_previous_session (self):
    self.bot.args.quiet = True
    self.bot.have_echo_message = True
    self.bot.gotline (':mock 251 YYBOT :users')
    self.bot.readall_buffer = b'partial'
    self.bot.reset_session_state()
    self.assertFalse (self.bot.replies)
    self.assertFalse (self.bot.have_echo_message)
    self.assertEqual (self.bot.readall_buffer, b'')

  def test_unicode_subject_is_split_without_losing_text (self):
    with self.mock_server() as (server, clock):
      subject = 'Grüße 🌻 ' * 200
      self.bot.args.message = [subject]
      self.bot.run_session()
      messages = [line for line in server.sent if line.startswith ('PRIVMSG ')]
      self.assertGreater (len (messages), 1)
      self.assertEqual (''.join (line.split (' :', 1)[1] for line in messages), subject)
      self.assertTrue (all (len ((line + '\r\n').encode()) <= self.bot.max_line_bytes for line in messages))

  def test_command_injection_is_rejected_before_socket_use (self):
    for value in ('PASS secret\r\nJOIN #bad', 'PRIVMSG #test :bad\0text', 'NICK ' + 'x' * 512):
      with self.subTest (value = value), self.assertRaises (self.bot.Fatal):
        self.bot.sendline (value)
    self.bot.ircsock.sendall.assert_not_called()

  def test_bad_connection_arguments_fail_before_connect (self):
    for name, value in (('j', '#test\r\nJOIN #bad'), ('n', 'bad nick'), ('n', 'x' * 512)):
      with self.subTest (name = name), self.mock_server() as (server, clock):
        setattr (self.bot.args, name, value)
        with self.assertRaises (self.bot.Fatal):
          self.bot.run_session()
        self.assertIsNone (server.address)

  def test_message_controls_are_text_not_commands (self):
    with self.mock_server() as (server, clock):
      self.bot.args.message = ['hello\r\nJOIN #bad\0\t\x1b']
      self.bot.run_session()
      messages = [line for line in server.sent if line.startswith ('PRIVMSG ')]
      self.assertEqual (messages, ['PRIVMSG #test :hello ', 'PRIVMSG #test :JOIN #bad   '])
      self.assertNotIn ('JOIN #bad', server.sent)

  def test_multiline_messages_keep_rate_limit (self):
    with self.mock_server() as (server, clock):
      self.bot.args.message = ['one\ntwo\nthree']
      sent_at = []

      def respond (line):
        if line.startswith ('PRIVMSG '):
          sent_at.append (clock.now)
        server.default_response (line)

      server.respond = respond
      self.bot.run_session()
      self.assertEqual (len (sent_at), 3)
      self.assertTrue (all (b - a >= self.bot.message_rate for a, b in zip (sent_at, sent_at[1:])))

  def run_main (self, *arguments):
    output = io.StringIO()
    status = 0
    with contextlib.redirect_stderr (output):
      try:
        self.bot.main (['-q', '-s', 'mock.invalid', '-j', '#test', *arguments])
      except SystemExit as error:
        status = error.code
    return status, output.getvalue()

  def test_network_guard_blocks_socket_creation (self):
    with self.assertRaisesRegex (AssertionError, 'must not use real sockets'):
      self.bot.socket.socket()

  def test_missing_echo_capability_prevents_sending (self):
    with self.mock_server() as (server, clock):
      def respond (line):
        if line == 'CAP REQ :echo-message':
          server.queue (':mock CAP * NAK :echo-message')
        else:
          server.default_response (line)

      server.respond = respond
      status, output = self.run_main ('hello')
      self.assertEqual (status, 1)
      self.assertIn ('refusing unverified delivery', output)
      self.assertEqual (self.bot.socket.socket.call_count, 1)
      self.assertFalse (any (line.startswith ('PRIVMSG ') for line in server.sent))

  def test_lost_second_echo_does_not_resend_or_continue (self):
    with self.mock_server() as (server, clock):
      def respond (line):
        if line != 'PRIVMSG #test :two':
          server.default_response (line)

      server.respond = respond
      status, output = self.run_main ('one\ntwo\nthree')
      self.assertEqual (status, 1)
      self.assertIn ('delivery may have occurred; not retrying', output)
      self.assertEqual (self.bot.socket.socket.call_count, 1)
      self.assertEqual ([line for line in server.sent if line.startswith ('PRIVMSG ')],
                        ['PRIVMSG #test :one', 'PRIVMSG #test :two'])

  def test_partial_message_write_does_not_retry (self):
    with self.mock_server() as (server, clock):
      original_send = server.sendall

      def partial_write (data):
        if data.startswith (b'PRIVMSG '):
          server.data.extend (data[:12])
          raise TimeoutError ('mock partial write')
        original_send (data)

      with mock.patch.object (server, 'sendall', side_effect = partial_write):
        status, output = self.run_main ('hello')
      self.assertEqual (status, 1)
      self.assertIn ('delivery may have occurred; not retrying', output)
      self.assertEqual (self.bot.socket.socket.call_count, 1)
      self.assertEqual (server.data.count (b'PRIVMSG '), 1)

  def test_disconnect_after_verified_delivery_is_success (self):
    with self.mock_server() as (server, clock):
      def respond (line):
        server.default_response (line)
        if line.startswith ('PRIVMSG '):
          server.incoming.append (b'')

      server.respond = respond
      status, output = self.run_main ('hello')
      self.assertEqual (status, 0)
      self.assertIn ('delivered (echo verified)', output)
      self.assertEqual (self.bot.socket.socket.call_count, 1)
      self.assertTrue (server.closed)

  def test_connection_failure_can_retry_before_sending (self):
    with self.mock_server() as (server, clock):
      with mock.patch.object (server, 'connect', side_effect = [ConnectionError ('mock failure'), None]):
        status, output = self.run_main ('hello')
      self.assertEqual (status, 0)
      self.assertIn ('attempt 2/3', output)
      self.assertEqual (self.bot.socket.socket.call_count, 2)
      self.assertEqual (server.sent.count ('PRIVMSG #test :hello'), 1)

  def test_connection_retries_are_bounded (self):
    with self.mock_server() as (server, clock):
      with mock.patch.object (server, 'connect', side_effect = ConnectionError ('mock failure')):
        status, output = self.run_main ('hello')
      self.assertEqual (status, 1)
      self.assertEqual (self.bot.socket.socket.call_count, 3)
      self.assertEqual (server.sent, [])

  def test_ban_stops_without_retry (self):
    with self.mock_server() as (server, clock):
      server.queue (':mock 465 YYBOT :banned')
      status, output = self.run_main ('hello')
      self.assertEqual (status, 1)
      self.assertIn ('server ban (465), not retrying', output)
      self.assertEqual (self.bot.socket.socket.call_count, 1)
      self.assertEqual (server.sent, [])

  def test_verified_multiline_delivery_succeeds_once (self):
    with self.mock_server() as (server, clock):
      status, output = self.run_main ('one\ntwo')
      self.assertEqual (status, 0)
      self.assertEqual (self.bot.socket.socket.call_count, 1)
      self.assertEqual (server.sent.count ('PRIVMSG #test :one'), 1)
      self.assertEqual (server.sent.count ('PRIVMSG #test :two'), 1)

  def test_list_only_skips_cap_and_succeeds_without_echo (self):
    with self.mock_server() as (server, clock):
      status, output = self.run_main ('-l', '-R', 'some/repository')
      self.assertEqual (status, 0)
      self.assertIn ('channel list received', output)
      self.assertEqual (server.sent.count ('LIST'), 1)
      self.assertFalse (any (line.startswith ('PRIVMSG ') for line in server.sent))
      self.assertFalse (any (line.startswith ('CAP ') for line in server.sent))

  def test_list_failure_after_delivery_reports_delivery (self):
    with self.mock_server() as (server, clock):
      def respond (line):
        if line != 'LIST':
          server.default_response (line)

      server.respond = respond
      status, output = self.run_main ('-l', 'hello')
      self.assertEqual (status, 0)
      self.assertIn ('delivered (echo verified)', output)
      self.assertEqual (self.bot.socket.socket.call_count, 1)
      self.assertEqual (server.sent.count ('PRIVMSG #test :hello'), 1)

  def test_empty_message_fails_before_connect (self):
    with self.mock_server() as (server, clock):
      status, output = self.run_main ('\n')
      self.assertEqual (status, 1)
      self.assertIn ('no text to send', output)
      self.bot.socket.socket.assert_not_called()

  def test_github_notification_uses_sanitized_event_fields (self):
    with self.mock_server() as (server, clock):
      event = {'repository': {'full_name': 'owner/repo'}, 'pusher': {'name': 'author\r\nQUIT'},
               'ref': 'refs/heads/trunk', 'head_commit': {'message': 'Fix the bot\nDetails', 'url': 'https://example.invalid/commit'}}
      self.bot.os.environ.update (GITHUB_EVENT_PATH = 'mock-event.json', IRCBOT_JOBS = 'success failure')
      with mock.patch.object (self.bot.os.path, 'exists', return_value = True):
        with mock.patch ('builtins.open', mock.mock_open (read_data = json.dumps (event))):
          status, output = self.run_main ('-G')
      self.assertEqual (status, 0)
      messages = [line for line in server.sent if line.startswith ('PRIVMSG ')]
      self.assertEqual (len (messages), 1)
      self.assertIn ('author  QUIT', messages[0])
      self.assertIn ('FAILURE', messages[0])
      self.assertIn ('Fix the bot - https://example.invalid/commit', messages[0])
      self.assertNotIn ('Details', messages[0])
      server.sent.clear()
      status, output = self.run_main ('plain message')
      self.assertEqual (status, 0)
      self.assertEqual ([line for line in server.sent if line.startswith ('PRIVMSG ')],
                        ['PRIVMSG #test :plain message'])

  def test_password_registration_never_logs_dummy_secret (self):
    with self.mock_server() as (server, clock):
      self.bot.args.quiet = False
      self.bot.os.environ['IRCBOT_PASS'] = 'dummy-secret'
      output = io.StringIO()
      with contextlib.redirect_stdout (output), contextlib.redirect_stderr (output):
        self.bot.run_session()
      self.assertIn ('PASS dummy-secret', server.sent)
      self.assertIn ('PASS <redacted>', output.getvalue())
      self.assertNotIn ('dummy-secret', output.getvalue())

  def test_ping_requires_its_own_pong_token (self):
    for correct in (False, True):
      with self.subTest (correct = correct), self.mock_server() as (server, clock):
        self.bot.args.ping = True

        def respond (line):
          if line.startswith ('PING '):
            server.queue (':mock PONG mock :wrong-token', ':mock NOTICE YYBOT :PONG pleasegetbacktome')
            if not correct:
              return
          server.default_response (line)

        server.respond = respond
        if correct:
          self.bot.run_session()
          self.assertIn ('PRIVMSG #test :hello', server.sent)
        else:
          with self.assertRaises (TimeoutError):
            self.bot.run_session()
          self.assertFalse (any (line.startswith ('PRIVMSG ') for line in server.sent))


if __name__ == "__main__":
  unittest.main()
