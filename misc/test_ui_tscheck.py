# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

from pathlib import Path
import subprocess
import tempfile
import unittest


UI_MAKEFILE = Path (__file__).resolve().parent.parent / 'ui/Makefile.mk'


class UiTscheckTests (unittest.TestCase):
  def test_installs_compiler_before_type_check (self):
    with tempfile.TemporaryDirectory() as directory:
      root = Path (directory)
      (root / 'ui/tests').mkdir (parents = True)
      (root / 'ui/Makefile.mk').write_text (UI_MAKEFILE.read_text())
      (root / 'ui/gen-testcalls.ts').touch()
      (root / 'ui/tests/empty.ts').touch()
      (root / 'ui/tests/TestList.g.mk').write_text ('UI_TEST_LIST :=\n')
      (root / 'tsconfig.json').write_text ('{}\n')
      (root / 'GNUmakefile').write_text (
        '> := out\n'
        'QECHO := @echo\n'
        'include ui/Makefile.mk\n'
        'node_modules/.npm.done:\n'
        '\t@mkdir -p node_modules/.bin\n'
        '\t@printf "%s\\n" "#!/bin/sh" "test -f node_modules/.npm.done && touch checked" > node_modules/.bin/tsc\n'
        '\t@chmod +x node_modules/.bin/tsc\n'
        '\t@touch $@\n')
      result = subprocess.run (['make', '--no-print-directory', '-j2', 'ui/tscheck'],
                               cwd = root, capture_output = True, text = True)
      self.assertEqual (result.returncode, 0, result.stdout + result.stderr)
      self.assertTrue ((root / 'checked').exists())


if __name__ == '__main__':
  unittest.main()
