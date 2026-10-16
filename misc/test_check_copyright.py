# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

from pathlib import Path
import os
import subprocess
import tempfile
import unittest


SOURCE = Path (__file__).resolve().parent
ENV = dict (os.environ, GIT_CONFIG_GLOBAL = '/dev/null', GIT_CONFIG_NOSYSTEM = '1',
            GIT_AUTHOR_NAME = 'Test', GIT_AUTHOR_EMAIL = 'test@example.com',
            GIT_COMMITTER_NAME = 'Test', GIT_COMMITTER_EMAIL = 'test@example.com')


class CopyrightTests (unittest.TestCase):
  def test_parallel_checks_keep_their_lists (self):
    with tempfile.TemporaryDirectory() as directory:
      root = Path (directory)
      for name in ['misc', 'trkn', 'doc', 'build']:
        (root / name).mkdir()
      (root / 'doc/copyright').write_text ('Files:\n **\nLicense: MPL-2.0\n')
      (root / 'trkn/copyright').write_text ('')
      (root / 'GNUmakefile').write_text (f'''SHELL := /bin/bash -o pipefail
> := build
Q := @
GITCOMMITDEPS := .git/logs/HEAD
REPOCOMMITDEPS := $(GITCOMMITDEPS)
include {SOURCE / 'Makefile.mk'}
strict: stamp-cleaned
stamp-cleaned: $>/.copyright.check
\t@printf done > stamp-done
.PHONY: stamp-cleaned
''')
      checker = root / 'misc/checkcrlist.py'
      checker.write_text (f'''#!/usr/bin/env python3
import os
import sys

if '-p' in sys.argv:
  with open ('checker-ready', 'w') as ready:
    ready.write ('ready')
  with open ('stamp-done') as done:
    done.read()
else:
  with open ('checker-ready') as ready:
    ready.read()
os.execv (sys.executable, [sys.executable, {str (SOURCE / 'checkcrlist.py')!r}, *sys.argv[1:]])
''')
      checker.chmod (0o755)
      for args in [('init', '-q', '-b', 'trunk'), ('add', '.'), ('commit', '-qm', 'Add fixture')]:
        subprocess.run (['git', *args], cwd = root, env = ENV, check = True)
      os.mkfifo (root / 'checker-ready')
      os.mkfifo (root / 'stamp-done')
      result = subprocess.run (['make', '-j2', 'strict'], cwd = root, env = ENV,
                               capture_output = True, text = True, timeout = 20)
      self.assertEqual (result.returncode, 0, result.stdout + result.stderr)
      self.assertTrue ((root / 'build/.copyright.check').exists())
      self.assertEqual (list ((root / 'build').glob ('*.lst')), [])


if __name__ == '__main__':
  unittest.main()
