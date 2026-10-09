# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import collections
from pathlib import Path
import re
import sys


def main ():
  root = Path (sys.argv[1] if len (sys.argv) > 1 else 'out/clang-tidy')
  logs = sorted (root.rglob ('*.log'))
  counts = collections.Counter()
  diagnostics = []
  failed = 0
  for log in logs:
    lines = log.read_text (encoding = 'utf-8', errors = 'replace').splitlines()
    failed += any (line.startswith ('CLANG-TIDY FAILED:') for line in lines)
    for line in lines:
      match = re.match (r'(?:\S+:\d+:\d+: )?(warning|error|fatal error): (.*)', line)
      if match:
        check = re.search (r'\[([^\]]+)\]$', match[2])
        counts[check[1] if check else match[1]] += 1
        diagnostics.append (line)
  report = ['# Clang-tidy report', '',
            f'{len(logs)} source logs, {failed} failed clang-tidy runs, {len(diagnostics)} diagnostics (listed in diagnostics.txt).', '',
            '| Check or severity | Count |', '| --- | ---: |']
  report += [f'| `{check}` | {count} |' for check, count in counts.most_common()]
  text = '\n'.join (report) + '\n'
  (root / 'report.md').write_text (text, encoding = 'utf-8')
  (root / 'diagnostics.txt').write_text ('\n'.join (diagnostics) + '\n', encoding = 'utf-8')
  print (text, end = '')


if __name__ == '__main__':
  main()
