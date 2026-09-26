import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


SCRIPT = Path (__file__).with_name ('clang-tidy-report.py')


class ReportTests (unittest.TestCase):
  def setUp (self):
    temporary = tempfile.TemporaryDirectory()
    self.addCleanup (temporary.cleanup)
    self.root = Path (temporary.name)
    self.logs = self.root / 'logs'
    self.env = {key: value for key, value in os.environ.items()
                if key not in ('BUILD_RESULT', 'TIDY_RESULT', 'GITHUB_SHA', 'GITHUB_STEP_SUMMARY', 'GITHUB_OUTPUT')}

  def run_report (self):
    result = subprocess.run ([sys.executable, str (SCRIPT), str (self.logs)], env = self.env,
                             capture_output = True, text = True, check = True)
    self.assertEqual (result.stdout, (self.logs / 'report.md').read_text())
    return result.stdout

  def test_diagnostics_and_github_outputs (self):
    source = self.logs / 'ase/example.cc.log'
    source.parent.mkdir (parents = True)
    warning = 'ase/example.cc:1:2: warning: example [bugprone-example]\n'
    error = 'ase/example.cc:2:2: fatal error: missing header\n'
    source.write_text (warning + warning + error + 'ase/example.cc:1:2: note: detail\n')
    summary = self.root / 'summary'
    output = self.root / 'output'
    summary.write_text ('Existing summary\n')
    output.write_text ('existing=value\n')
    self.env.update (BUILD_RESULT = 'success', TIDY_RESULT = 'success', GITHUB_SHA = 'abc123',
                     GITHUB_STEP_SUMMARY = str (summary), GITHUB_OUTPUT = str (output))
    report = self.run_report()
    self.assertIn ('Commit: `abc123`', report)
    self.assertIn ('1 source logs, 3 diagnostic occurrences.', report)
    self.assertIn ('| `bugprone-example` | 2 |', report)
    self.assertIn ('| `fatal error` | 1 |', report)
    self.assertEqual ((self.logs / 'diagnostics.txt').read_text(), warning + warning + error)
    self.assertEqual (summary.read_text(), 'Existing summary\n' + report)
    self.assertEqual (output.read_text(), 'existing=value\nfindings=3\n')

  def test_failed_build_without_logs (self):
    self.env.update (BUILD_RESULT = 'failure', TIDY_RESULT = 'skipped')
    report = self.run_report()
    self.assertIn ('Build: failure. Analysis: skipped.', report)
    self.assertIn ('No analysis logs were produced.', report)

  def test_local_run_without_environment (self):
    report = self.run_report()
    self.assertIn ('Commit: `unknown`', report)
    self.assertIn ('Build: unknown. Analysis: unknown.', report)


if __name__ == '__main__':
  unittest.main()
