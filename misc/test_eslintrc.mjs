// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import assert from 'node:assert/strict';
import { test } from 'node:test';
import { ESLint } from 'eslint';

const eslint = new ESLint ({ overrideConfigFile: 'ui/eslintrc.js' });

async function check (file_path, source, expected_rules = [])
{
  const [result] = await eslint.lintText (source, { filePath: file_path });
  const errors = result.messages.filter (message => message.severity === 2);
  assert.deepEqual (errors.map (message => message.ruleId), expected_rules, JSON.stringify (result.messages));
}

test ('component rules cover JS, JSX, TS and TSX', async () => {
  for (const extension of ['js', 'jsx', 'ts', 'tsx']) {
    const file_path = `ui/b/lint_test.${extension}`;
    await check (file_path, 'let dialogRef;', ['id-match']);
    await check (file_path, 'const [value, setValue] = [0, () => {}];', ['id-match']);
    await check (file_path, 'export const project = App.project;', ['no-restricted-properties']);
    await check (file_path, 'export const agent = navigator.userAgent;', ['no-restricted-properties']);
    await check (file_path, 'class TestView {} const MAX_COUNT = 2; let dialog_ref; export const project = Shell.project;');
  }
});

test ('typed JSX keeps upstream APIs and callback props', async () => {
  await check ('ui/b/lint_test.ts', 'export const identity = <T> (value: T): T => value;');
  await check ('ui/b/lint_test.ts', 'let dialogRef: HTMLDialogElement;', ['id-match']);
  await check ('ui/b/lint_test.tsx', 'export const WrongView = () => <div>{App.project}</div>;', ['no-restricted-properties']);
  await check ('ui/b/lint_test.tsx', `
    import { createSignal, onMount } from 'solid-js';
    interface TestProps { onChange?: () => void }
    export function TestView (props: TestProps) {
      const [value, set_value] = createSignal (0);
      onMount (() => set_value (1));
      return <button onClick={props.onChange}>{value()}</button>;
    }
  `);
});

test ('HTML classes come from ui/tailwind.css', async () => {
  await check ('ui/lint_test.html', '<div class="bg-dim-500 flex b-app"></div>\n');
  await check ('ui/lint_test.html', '<div class="no-such-class"></div>\n', ['better-tailwindcss/no-unregistered-classes']);
});

test ('user agent access stays in About and browser setup', async () => {
  for (const file_path of ['ui/b/aboutdialog.jsx', 'ui/startup.js', 'ui/mouse.js'])
    await check (file_path, 'export const agent = navigator.userAgent;');
  await check ('ui/b/aboutdialog.jsx', 'export const project = App.project;', ['no-restricted-properties']);
  await check ('ui/util.js', 'export const agent = navigator.userAgent;', ['no-restricted-properties']);
  await check ('ui/grepdialog.js', 'export const project = App.project;');
});
