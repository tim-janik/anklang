# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

from pathlib import Path
import os
import subprocess
import tempfile
import unittest


SCRIPT = Path (__file__).with_name ('check-removals.sh').resolve()
ENV = dict (os.environ, GIT_CONFIG_GLOBAL = '/dev/null', GIT_CONFIG_NOSYSTEM = '1',
            GIT_AUTHOR_NAME = 'Test', GIT_AUTHOR_EMAIL = 'test@example.com',
            GIT_COMMITTER_NAME = 'Test', GIT_COMMITTER_EMAIL = 'test@example.com')


class RemovalsTests (unittest.TestCase):
  def setUp (self):
    temporary = tempfile.TemporaryDirectory()
    self.addCleanup (temporary.cleanup)
    self.root = Path (temporary.name)
    (self.root / 'ui/tests').mkdir (parents = True)
    lines = ''.join (f'line{i};\n' for i in range (12))
    (self.root / 'ui/mouse.js').write_text (lines)
    (self.root / 'ui/tests/mouse_test.ts').write_text (lines)
    self.git ('init', '-q', '-b', 'trunk')
    self.commit ('ui: add files')
    self.git ('checkout', '-q', '-b', 'branch')

  def git (self, *args):
    subprocess.run (['git', *args], cwd = self.root, env = ENV, check = True)

  def commit (self, message):
    self.git ('add', '-A')
    self.git ('commit', '-q', '-m', message)

  def run_check (self, cwd = None, env = None):
    return subprocess.run (['bash', str (SCRIPT)], cwd = cwd or self.root, env = dict (ENV, **(env or {})), capture_output = True, text = True)

  def test_removal_needs_evidence (self):
    (self.root / 'ui/mouse.js').write_text ('')
    self.commit ('ui/mouse.js: remove the workaround')
    result = self.run_check()
    self.assertEqual (result.returncode, 1)
    self.assertRegex (result.stderr, r'Deletion of\s+12 lines: \w+ ui/mouse\.js: remove the workaround')

  def test_removal_with_evidence (self):
    (self.root / 'ui/mouse.js').write_text ('')
    self.commit ('ui/mouse.js: remove the workaround\n\nEvidence: Chrome 121 fixed movementX scaling.')
    self.assertEqual (self.run_check().returncode, 0)

  def test_ignores_scattered_removals (self):
    (self.root / 'ui/mouse.js').write_text ('line6;\n')
    self.commit ('ui/mouse.js: keep one line')
    self.assertEqual (self.run_check().returncode, 0)

  def test_ignores_rewrites (self):
    (self.root / 'ui/mouse.js').write_text (''.join (f'call{i}();\n' for i in range (12)))
    self.commit ('ui/mouse.js: rewrite')
    self.assertEqual (self.run_check().returncode, 0)

  def test_ignores_color_and_external_diff (self):
    (self.root / 'ui/mouse.js').write_text ('')
    self.commit ('ui/mouse.js: remove the workaround')
    color = {'GIT_CONFIG_COUNT': '1', 'GIT_CONFIG_KEY_0': 'color.ui', 'GIT_CONFIG_VALUE_0': 'always'}
    self.assertEqual (self.run_check (env = color).returncode, 1)
    self.assertEqual (self.run_check (env = {'GIT_EXTERNAL_DIFF': '/bin/true'}).returncode, 1)

  def test_ignores_diff_algorithm (self):
    (self.root / 'ui/probe.js').write_text ('e();\na();\na();\ni();\ni();\nd();\ni();\nh();\nd();\nh();\nb();\n')
    self.commit ('ui/probe.js: add probes')
    (self.root / 'ui/probe.js').write_text ('b();\nd();\nd();\n')
    self.commit ('ui/probe.js: drop the probes')
    # myers splits this into a 2 line block, histogram into a 10 line block
    myers = {'GIT_CONFIG_COUNT': '1', 'GIT_CONFIG_KEY_0': 'diff.algorithm', 'GIT_CONFIG_VALUE_0': 'myers'}
    self.assertEqual (self.run_check (env = myers).returncode, 1)

  def test_ignores_tests_and_trunk (self):
    (self.root / 'ui/tests/mouse_test.ts').write_text ('')
    self.commit ('ui/tests/mouse_test.ts: drop cases')
    self.assertEqual (self.run_check().returncode, 0)
    (self.root / 'ui/mouse.js').write_text ('')
    self.commit ('ui/mouse.js: remove the workaround')
    self.git ('branch', '-f', 'trunk')
    self.assertEqual (self.run_check().returncode, 0)

  def test_without_git (self):
    with tempfile.TemporaryDirectory() as directory:
      self.assertEqual (self.run_check (directory).returncode, 0)


if __name__ == '__main__':
  unittest.main()
