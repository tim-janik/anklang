# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

from pathlib import Path
import subprocess
import tempfile
import unittest


SCRIPT = Path (__file__).with_name ('check-agents-md.sh').resolve()


class AgentsMdTests (unittest.TestCase):
  def setUp (self):
    temporary = tempfile.TemporaryDirectory()
    self.addCleanup (temporary.cleanup)
    self.root = Path (temporary.name)
    for directory in ['ase', 'devices', 'jsonipc', 'ui', 'trkn', 'misc']:
      (self.root / directory).mkdir()
    (self.root / 'GNUmakefile').write_text ('include Makefile.mk\n')
    (self.root / 'Makefile.mk').write_text ('check lint:\n\t@true\nvariable := value\n')
    (self.root / 'misc/Makefile.mk').write_text ('check-extra:\n')
    (self.root / 'ase/api.hh').write_text ('class ProjectImpl {};\nclass UndoManager {};\n')
    (self.root / 'ui/shell.tsx').write_text ('class Shell {}\n')

  def run_check (self, text):
    (self.root / 'AGENTS.md').write_text (text)
    return subprocess.run (['bash', str (SCRIPT), 'AGENTS.md'], cwd = self.root, capture_output = True, text = True)

  def test_valid_references_without_git (self):
    result = self.run_check ('`ase/api.hh` `ui/` `make check` `make lint` `make check-extra`\n'
                             '`ProjectImpl` `UndoManager*` `Shell`\n'
                             '`Shell.project` `App.project` `IDs::` `.cc` `.g.` `*Impl` `foo()` `notify:<prop>`\n')
    self.assertEqual (result.returncode, 0, result.stderr)

  def test_stale_references (self):
    result = self.run_check ('`ase/missing.hh` `make missing` `MissingImpl` `MissingImpl`\n')
    self.assertEqual (result.returncode, 1)
    self.assertEqual (sorted (result.stderr.splitlines()), [
      'AGENTS.md: missing make target: missing',
      'AGENTS.md: missing path: ase/missing.hh',
      'AGENTS.md: unknown name: MissingImpl',
    ])

  def test_make_target_must_be_a_literal_rule (self):
    for target in ['check.extra', 'variable']:
      with self.subTest (target = target):
        result = self.run_check (f'`make {target}`\n')
        self.assertEqual (result.returncode, 1)
        self.assertIn (f'missing make target: {target}', result.stderr)

  def test_name_must_be_a_whole_word_in_source (self):
    (self.root / 'ase/notes.md').write_text ('Project\n')
    result = self.run_check ('`Project`\n')
    self.assertEqual (result.returncode, 1)
    self.assertIn ('unknown name: Project', result.stderr)

  def test_no_references (self):
    result = self.run_check ('# Plain text\n')
    self.assertEqual (result.returncode, 0, result.stderr)

  def test_missing_document (self):
    result = subprocess.run (['bash', str (SCRIPT), 'missing.md'], cwd = self.root, capture_output = True, text = True)
    self.assertNotEqual (result.returncode, 0)


if __name__ == '__main__':
  unittest.main()
