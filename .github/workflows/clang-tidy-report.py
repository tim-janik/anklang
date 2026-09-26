import argparse
import collections
import os
from pathlib import Path
import re


def main ():
  parser = argparse.ArgumentParser (
    description = 'Summarize clang-tidy logs into report.md and diagnostics.txt in LOG_DIR.',
    formatter_class = argparse.RawDescriptionHelpFormatter,
    epilog = '''Environment, all optional:
  BUILD_RESULT         Build step outcome, e.g. success, failure or skipped.
  TIDY_RESULT          Analysis step outcome, e.g. success, failure or skipped.
                       Both default to unknown; neither changes the exit status.
  GITHUB_SHA           Commit shown in the report; defaults to unknown.
  GITHUB_STEP_SUMMARY  File to append the Markdown summary to, if set.
  GITHUB_OUTPUT        File to append findings=<count> to, if set.

Local example:
  BUILD_RESULT=success TIDY_RESULT=success python3 .github/workflows/clang-tidy-report.py out/clang-tidy

Tests:
  python3 -m unittest discover -s .github/workflows -p test_clang_tidy_report.py

Reads *.log recursively. Counts warning/error occurrences, including repeats
from shared headers. Missing logs are reported explicitly. Analyzer failures
may exist in raw logs even when the Make step succeeds. Findings do not fail
this script. The report is also printed to stdout.''')
  parser.add_argument ('log_dir', nargs = '?', default = 'out/clang-tidy', metavar = 'LOG_DIR')
  root = Path (parser.parse_args().log_dir)
  logs = sorted (root.rglob ('*.log'))
  counts = collections.Counter()
  diagnostics = []
  for log in logs:
    for line in log.read_text (errors = 'replace').splitlines():
      match = re.search (r': (warning|error|fatal error): (.*)', line)
      if match:
        check = re.search (r'\[([^\]]+)\]$', match[2])
        counts[check[1] if check else match[1]] += 1
        diagnostics.append (line)
  report = ['# Weekly clang-tidy', '', 'Advisory only. Findings never block CI.', '',
            f"Commit: `{os.environ.get ('GITHUB_SHA', 'unknown')}`", '',
            f"Build: {os.environ.get ('BUILD_RESULT', 'unknown')}. Analysis: {os.environ.get ('TIDY_RESULT', 'unknown')}.", '',
            f'{len(logs)} source logs, {len(diagnostics)} diagnostic occurrences.', '',
            'Counts include repeated diagnostics from shared headers. A completed Make step does not guarantee every file was analyzed.', '',
            'Download the clang-tidy-report artifact for this report, full diagnostics, raw logs and YAML fixes.', '']
  if counts:
    report += ['| Check or severity | Count |', '| --- | ---: |']
    report += [f'| `{check}` | {count} |' for check, count in counts.most_common()]
  if not logs:
    report += ['No analysis logs were produced. See the build steps for details.']
  root.mkdir (parents = True, exist_ok = True)
  text = '\n'.join (report) + '\n'
  (root / 'report.md').write_text (text)
  (root / 'diagnostics.txt').write_text ('\n'.join (diagnostics) + '\n')
  for variable, content in [('GITHUB_STEP_SUMMARY', text), ('GITHUB_OUTPUT', f'findings={len(diagnostics)}\n')]:
    if os.environ.get (variable):
      with open (os.environ[variable], 'a') as output:
        output.write (content)
  print (text, end = '')


if __name__ == '__main__':
  main()
