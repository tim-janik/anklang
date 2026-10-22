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
  const result = spawnSync (process.execPath, [join (import.meta.dirname, 'synsmell.ts'), '--errors', '--none', ...options, ...files],
                            { encoding: 'utf8' });
  assert.ifError (result.error);
  assert.equal (result.stderr, '');
  return result;
}

const samples = {
  'ban-printf': 'int main () { printf ("test"); }\n',
  'separate-body': 'function example (value) {\n  return value;\n}\n',
  'whitespace-before-parenthesis': 'function example(value) {}\n',
  'ban-fixme': '// FIXME: unfinished code\n',
  'ban-todo': '// TODO: open issue\n',
};

for (const [check_name, source] of Object.entries (samples))
  test (`${check_name} can be waived for one file`, t => {
    const extension = check_name === 'ban-printf' ? 'cc' : 'js';
    const allowed_file = `allowed.${extension}`, denied_file = `denied.${extension}`;
    const allowed_source = `// synsmell: allow ${check_name} -- deliberate exception\n${source}`;
    const options = [`--${check_name}=1`];
    const allowed = lint (t, { [allowed_file]: allowed_source }, options);
    assert.equal (allowed.status, 0, allowed.stdout);
    assert.equal (allowed.stdout, '');
    const mixed = lint (t, { [allowed_file]: allowed_source, [denied_file]: source }, options);
    assert.equal (mixed.status, 1, mixed.stdout);
    assert.ok (!mixed.stdout.includes (`${allowed_file}:`), mixed.stdout);
    assert.ok (mixed.stdout.includes (`${denied_file}:`), mixed.stdout);
  });

test ('a waiver leaves other checks enabled', t => {
  const result = lint (t, { 'example.cc': '// synsmell: allow ban-printf\nint main () { printf("test"); }\n' },
                       ['--ban-printf=1', '--whitespace-before-parenthesis=1']);
  assert.equal (result.status, 1, result.stdout);
  assert.match (result.stdout, /missing whitespace before parenthesis/);
  assert.doesNotMatch (result.stdout, /invalid call/);
});

test ('waivers require a line comment and a complete check name', t => {
  for (const prefix of [
    'const char *message = "// synsmell: allow ban-printf";\n',
    'const char *message = R"(\n// synsmell: allow ban-printf\n)";\n',
    '/* example: // synsmell: allow ban-printf */\n',
    '// synsmell: allow ban-printf-extra\n',
    '// synsmell: allow ban-printf2\n',
  ]) {
    const result = lint (t, { 'example.cc': prefix + samples['ban-printf'] }, ['--ban-printf=1']);
    assert.equal (result.status, 1, prefix + result.stdout);
    assert.match (result.stdout, /invalid call to printf/);
  }
});
