// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { test } from 'node:test';

function lint (t, sources, options)
{
  const directory = mkdtempSync (join (tmpdir(), 'synsmell-'));
  t.after (() => rmSync (directory, { recursive: true, force: true }));
  const files = Object.entries (sources).map (([name, source]) => {
    const filename = join (directory, name);
    writeFileSync (filename, source);
    return filename;
  });
  const runts = process.env.RUNTS || 'node --experimental-strip-types';
  const result = spawnSync ('/bin/sh', ['-c', `exec ${runts} "$@"`, 'synsmell',
                                      join (import.meta.dirname, 'synsmell.ts'), '--errors', ...options, ...files],
                            { encoding: 'utf8' });
  assert.ifError (result.error);
  return result;
}

const samples = {
  'ban-printf': 'int main () { printf ("test"); }\n',
  'separate-body': 'function example (value) {\n  return value;\n}\n',
  'whitespace-before-parenthesis': 'function example(value) {}\n',
  'ban-fixme': '// FI' + 'XME: unfinished code\n',
  'ban-todo': '// TODO: open issue\n',
};

for (const [check_name, source] of Object.entries (samples))
  test (`${check_name} reports findings in every file`, t => {
    const extension = check_name === 'ban-printf' ? 'cc' : 'js';
    const first = `first.${extension}`, second = `second.${extension}`;
    const result = lint (t, {
      [first]: `// synsmell: allow ${check_name}\n${source}`,
      [second]: source,
    }, [`--${check_name}=1`]);
    assert.equal (result.status, 1, result.stderr + result.stdout);
    assert.ok (result.stdout.includes (`${first}:`), result.stdout);
    assert.ok (result.stdout.includes (`${second}:`), result.stdout);
  });

test ('source comments cannot suppress CRLF diagnostics', t => {
  const result = lint (t, { 'example.cc': '// synsmell: allow ban-printf\r\n' + samples['ban-printf'].replaceAll ('\n', '\r\n') }, []);
  assert.equal (result.status, 1, result.stderr + result.stdout);
  assert.match (result.stdout, /invalid call to printf/);
});

test ('clean code passes with the default checks enabled', t => {
  const result = lint (t, { 'example.js': 'function example (value)\n{\n  return value;\n}\n' }, []);
  assert.equal (result.status, 0, result.stderr + result.stdout);
  assert.equal (result.stdout, '');
});
