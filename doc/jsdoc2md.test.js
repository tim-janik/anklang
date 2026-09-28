// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import * as fs from 'node:fs';
import * as os from 'node:os';
import * as path from 'node:path';
import { test } from 'node:test';
import { fileURLToPath } from 'node:url';

const script = fileURLToPath (new URL ('./jsdoc2md.js', import.meta.url));

function fixture (t)
{
  const dir = fs.mkdtempSync (path.join (os.tmpdir(), 'jsdoc2md-'));
  t.after (() => fs.rmSync (dir, { recursive: true, force: true }));
  return {
    dir,
    write (name, source) {
      const filename = path.join (dir, name);
      fs.writeFileSync (filename, source);
      return filename;
    },
    run (...args) {
      return execFileSync (process.execPath, [script, ...args], { cwd: dir, encoding: 'utf8', stdio: 'pipe' });
    },
  };
}

test ('Markdown blocks preserve spacing and ignore API comments', t => {
  const f = fixture (t);
  const filename = f.write ('chapter.tsx', `
/** ## Chapter
 * A paragraph.
 *
 *     indented code
 */
/** An API description. */
export function View () { return <div/>; }
/// ### Details
/// More text.
///
///     more code`);
  assert.equal (f.run ('--markdown-only', filename),
                '## Chapter\nA paragraph.\n\n    indented code\n\n\n### Details\nMore text.\n\n    more code\n');
});

for (const ext of ['js', 'jsx', 'ts', 'tsx'])
  test (`Markdown and API extraction from ${ext}`, t => {
    const f = fixture (t);
    const typed = ext.startsWith ('ts');
    const jsx = ext.endsWith ('x');
    const filename = f.write ('component.' + ext, `
/** ## Usage
 * A handbook paragraph.
 */
const unrelated = 1;
/** @class Component
 * @description
 * Component properties.
 */
${typed ? 'export type Props = { label: string };' : ''}
/** Render a label. */
export function View (label${typed ? ': string' : ''}) {
  return ${jsx ? '<span>{label}</span>' : 'label'};
}
/// A public value.
export const answer = 42;
/** A model. */
export class Model {
  /// Update the model.
  update (value${typed ? ': number' : ''}) { return value; }
}
/// ### Last heading`);
    const output = f.run ('-d', '3', '-e', 'UI', filename);
    for (const description of ['A handbook paragraph.', 'Component properties.', 'Render a label.',
                               'A public value.', 'A model.', 'Update the model.', '### Last heading'])
      assert.equal (output.split (description).length - 1, 1, description);
    assert.match (output, /### \[Component\]/);
    assert.match (output, /#### UI Functions/);
    assert.ok (output.includes (`${filename}:UI.View;func`));
    assert.ok (output.includes ('*label*'));
    assert.ok (output.includes ('*value*'));
    assert.ok (!output.includes ('unrelated'));
    assert.ok (!output.includes ('<span>'));
  });

test ('multiple inputs keep their own API entries', t => {
  const f = fixture (t);
  const first = f.write ('first.js', '/** First API. */\nexport function first () {}');
  const second = f.write ('second.js', '/** Second API. */\nexport function second () {}');
  const output = f.run (first, second);
  assert.equal (output.split ('First API.').length - 1, 1);
  assert.equal (output.split ('Second API.').length - 1, 1);
});

test ('directory output replaces stale docs with an empty file', t => {
  const f = fixture (t);
  const filename = f.write ('chapter.jsx', '/** ## Chapter\n * Text.\n */');
  const odir = path.join (f.dir, 'docs');
  assert.equal (f.run ('--markdown-only', '-O', odir, filename), '');
  const output = path.join (odir, 'chapter.md');
  assert.equal (fs.readFileSync (output, 'utf8'), '## Chapter\nText.\n');
  fs.writeFileSync (filename, 'export const value = 1;');
  f.run ('--markdown-only', '-O', odir, filename);
  assert.equal (fs.readFileSync (output, 'utf8'), '');
});

test ('invalid TSX fails extraction', t => {
  const f = fixture (t);
  const filename = f.write ('broken.tsx', 'export function Broken () { return <div>; }');
  assert.throws (() => f.run (filename), /broken\.tsx/);
});

test ('documentation before a TypeScript interface survives type erasure', t => {
  const f = fixture (t);
  const filename = f.write ('interface.ts', `
/** @class Options
 * @description
 * Configuration options.
 */
export interface Options { value: number; }
`);
  assert.match (f.run (filename), /Configuration options\./);
});
