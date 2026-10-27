// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdtempSync, mkdirSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import process from 'node:process';
import { test } from 'node:test';
import { fileURLToPath, pathToFileURL } from 'node:url';

const generator = fileURLToPath (new URL ('../gen-testcalls.ts', import.meta.url));
const run = (...args) => spawnSync (process.execPath, ['--experimental-strip-types', generator, ...args], { encoding: 'utf8' });

test ('test discovery and registry generation', async () => {
  const dir = mkdtempSync (join (tmpdir(), 'anklang-testcalls-'));
  try {
    mkdirSync (join (dir, 'src'));
    mkdirSync (join (dir, 'gen'));
    const source = join (dir, 'src/fixture.ts');
    writeFileSync (source, `
// export function ignored() {}
/* export const also_ignored = 0; */
export function regular() { return 1; }
export async function asynchronous() { return 2; }
export const arrow = () => 3;
export let mutable = () => 4;
export var variable = () => 5;
function named() { return 6; }
export { named };
export class Helper {}
export interface Description { name: string; }
export type Label = string;
`);
    const names = ['regular', 'asynchronous', 'arrow', 'mutable', 'variable', 'named'];
    const listed = run ('--list', source);
    assert.equal (listed.status, 0, listed.stderr);
    assert.equal (listed.stdout, names.map (name => `fixture.${name}\n`).join (''));
    const output = join (dir, 'gen/registry.ts');
    const generated = run ('--out', output, source);
    assert.equal (generated.status, 0, generated.stderr);
    const registry = await import (pathToFileURL (output).href);
    assert.deepEqual (registry.testcalls_names, names.map (name => `fixture.${name}`));
    for (const [index, name] of names.entries())
      assert.equal (await registry.testcalls_find (`fixture.${name}`)(), index + 1);
    assert.equal (registry.testcalls_find ('absent'), undefined);

    writeFileSync (source, 'export default function example() {}\nexport { example as alias };\n');
    assert.equal (run ('--list', source).stdout, 'fixture.default\nfixture.alias\n');
    writeFileSync (source, '\nexport declare function unsupported(): void;\n');
    const unsupported = run ('--list', source);
    assert.notEqual (unsupported.status, 0);
    assert.match (unsupported.stderr, /Unrecognized export pattern.*:2:/);
    assert.equal (run ('--list', join (dir, 'missing.ts')).status, 1);
  } finally {
    rmSync (dir, { recursive: true, force: true });
  }
});

test ('invalid generator arguments print usage', () => {
  for (const args of [[], ['--invalid'], ['--list'], ['--out'], ['--out', 'output.ts']]) {
    const result = run (...args);
    assert.equal (result.status, 1);
    assert.match (result.stderr, /^Usage:/);
  }
});
