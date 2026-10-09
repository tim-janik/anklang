# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


SCRIPT = Path (__file__).with_name ('clang-tidy-report.py').resolve()


class ReportTests (unittest.TestCase):
  def setUp (self):
    temporary = tempfile.TemporaryDirectory()
    self.addCleanup (temporary.cleanup)
    self.logs = Path (temporary.name) / 'clang-tidy'
    self.logs.mkdir()

  def run_report (self):
    result = subprocess.run ([sys.executable, str (SCRIPT), str (self.logs)], capture_output = True, text = True, check = True)
    self.assertEqual (result.stdout, (self.logs / 'report.md').read_text())
    return result.stdout

  def test_diagnostics (self):
    warning = 'ase/example.cc:1:2: warning: example [bugprone-example]\n'
    error = 'ase/example.cc:2:2: fatal error: missing header\n'
    excerpt = '   23 | fprintf (stderr, "%s:%d: warning: ", file, line);\n'
    (self.logs / 'example.cc.log').write_text (warning + excerpt + warning + error + 'ase/example.cc:1:2: note: detail\n')
    report = self.run_report()
    self.assertIn ('1 source logs, 0 failed clang-tidy runs, 3 diagnostics', report)
    self.assertIn ('| `bugprone-example` | 2 |', report)
    self.assertIn ('| `fatal error` | 1 |', report)
    self.assertEqual ((self.logs / 'diagnostics.txt').read_text(), warning + warning + error)

  def test_failed_runs (self):
    (self.logs / 'missing.cc.log').write_text ('bash: clang-tidy: command not found\nCLANG-TIDY FAILED: exit status 127\n')
    self.assertIn ('1 source logs, 1 failed clang-tidy runs, 0 diagnostics', self.run_report())

  def test_make_generates_report_before_strict_failure (self):
    log = self.logs / 'sample.cc.log'
    log.write_text ('sample.cc:1:2: warning: example [bugprone-example]\n')
    for target, expected in [('clang-tidy', 0), ('clang-tidy-check', 2)]:
      with self.subTest (target = target):
        (self.logs / 'report.md').unlink (missing_ok = True)
        result = subprocess.run (['make', '-f', 'misc/Makefile.mk', target, f'>={self.logs.parent}',
                                  f'CLANG_TIDY_LOGS={log}', 'Q=@', 'QGEN=:', 'SHELL=/bin/bash'],
                                 cwd = SCRIPT.parent.parent, capture_output = True, text = True)
        self.assertEqual (result.returncode, expected, result.stdout + result.stderr)
        self.assertIn ('| `bugprone-example` | 1 |', (self.logs / 'report.md').read_text())


if __name__ == '__main__':
  unittest.main()
