// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { createComponent, render } from 'solid-js/web';
import { createSignal, Show } from 'solid-js';
import { AboutDialog } from '../b/aboutdialog';
import { PreferencesDialog } from '../b/preferencesdialog';
import { CrawlerDialog } from '../b/crawlerdialog';
import * as Dom from '../dom';

// == Test registry ==
const sub_tests: [string, () => Promise<any>][] = [];

/// Closing AboutDialog fires onClose once (M8).
async function test_aboutdialog_close_once (): Promise<boolean>
{
  const [shown, set_shown] = createSignal (true);
  let close_count = 0;
  const container = document.createElement ('div');
  document.body.appendChild (container);
  const dispose = render (() => createComponent (Show, {
    get when () { return shown(); },
    children: () => createComponent (AboutDialog, {
      onClose: () => { close_count++; set_shown (false); },
    }),
  }), container);

  try {
    // Wait for onMount
    await Dom.ui_wait (100);
    await Dom.ui_next_frame();
    await Dom.ui_next_frame();

    const button = container.querySelector ('button.button-xl') as HTMLElement;
    if (!button)
      throw new Error ('AboutDialog close button not found');
    if (close_count !== 0)
      throw new Error (`onClose fired before close: ${close_count}`);

    button.click();
    await Dom.ui_next_frame();
    await Dom.ui_next_frame();

    if (close_count !== 1)
      throw new Error (`onClose fired ${close_count} times for one close`);
    if (container.querySelector ('.b-about-dialog'))
      throw new Error ('AboutDialog still mounted after close');
  } finally {
    dispose();
    container.remove();
  }

  return true;
}
sub_tests.push (['aboutdialog', test_aboutdialog_close_once]);

/// Closing PreferencesDialog fires onClose once (M8).
/// shown=false so the mount effect skips startViewTransition.
async function test_preferencesdialog_close_once (): Promise<boolean>
{
  const [shown, set_shown] = createSignal (true);
  let close_count = 0;
  const container = document.createElement ('div');
  document.body.appendChild (container);
  const dispose = render (() => createComponent (Show, {
    get when () { return shown(); },
    children: () => createComponent (PreferencesDialog, {
      shown: false,
      onClose: () => { close_count++; set_shown (false); },
    }),
  }), container);

  try {
    await Dom.ui_wait (100);
    await Dom.ui_next_frame();

    const dialog = container.querySelector ('dialog.b-preferencesdialog') as HTMLElement;
    if (!dialog)
      throw new Error ('PreferencesDialog dialog element not found');

    // Native 'close' event (Escape/backdrop/close())
    dialog.dispatchEvent (new Event ('close'));
    await Dom.ui_next_frame();
    await Dom.ui_next_frame();

    if (close_count !== 1)
      throw new Error (`onClose fired ${close_count} times for one close`);
    if (container.querySelector ('.b-preferencesdialog'))
      throw new Error ('PreferencesDialog still mounted after close');
  } finally {
    dispose();
    container.remove();
  }

  return true;
}
sub_tests.push (['preferencesdialog', test_preferencesdialog_close_once]);

/// CrawlerDialog Close button fires onClose once (M8).
async function test_crawlerdialog_close_once (): Promise<boolean>
{
  const [shown, set_shown] = createSignal (true);
  let close_count = 0;
  const container = document.createElement ('div');
  document.body.appendChild (container);
  const dispose = render (() => createComponent (Show, {
    get when () { return shown(); },
    children: () => createComponent (CrawlerDialog, {
      shown: true,
      cwd: '~MUSIC',
      onSelect: () => {},
      onClose: () => { close_count++; set_shown (false); },
    }),
  }), container);

  try {
    await Dom.ui_wait (100);
    await Dom.ui_next_frame();

    const buttons = container.querySelectorAll ('button.button-xl');
    const close_button = buttons[buttons.length - 1] as HTMLElement; // footer: Select, Close
    if (!close_button)
      throw new Error ('CrawlerDialog close button not found');

    close_button.click();
    await Dom.ui_next_frame();
    await Dom.ui_next_frame();

    if (close_count !== 1)
      throw new Error (`onClose fired ${close_count} times for one close`);
    if (container.querySelector ('.b-crawlerdialog'))
      throw new Error ('CrawlerDialog still mounted after close');
  } finally {
    dispose();
    container.remove();
  }

  return true;
}
sub_tests.push (['crawlerdialog_close', test_crawlerdialog_close_once]);

/// Selecting a file must not fire onClose (M8).
async function test_crawlerdialog_select_suppresses_close (): Promise<boolean>
{
  const [shown, set_shown] = createSignal (true);
  let select_count = 0, close_count = 0;
  const container = document.createElement ('div');
  document.body.appendChild (container);
  const dispose = render (() => createComponent (Show, {
    get when () { return shown(); },
    children: () => createComponent (CrawlerDialog, {
      shown: true,
      cwd: '~MUSIC',
      onSelect: () => { select_count++; set_shown (false); },
      onClose: () => { close_count++; },
    }),
  }), container);

  try {
    await Dom.ui_wait (100);
    await Dom.ui_next_frame();
    await Dom.ui_next_frame();

    const pathentry = container.querySelector ('input.-pathentry') as HTMLInputElement;
    if (!pathentry)
      throw new Error ('CrawlerDialog path entry not found');
    pathentry.value = 'test-file.wav';

    const buttons = container.querySelectorAll ('button.button-xl');
    const select_button = buttons[0] as HTMLButtonElement; // footer: Select, Close
    if (!select_button)
      throw new Error ('CrawlerDialog select button not found');

    // Wait for the crawler to settle
    const deadline = Date.now() + 4000;
    while (select_button.disabled && Date.now() < deadline)
      await Dom.ui_wait (100);
    if (select_button.disabled)
      throw new Error ('CrawlerDialog select button stayed disabled');

    select_button.click();
    await Dom.ui_next_frame();
    await Dom.ui_next_frame();

    if (select_count !== 1)
      throw new Error (`onSelect fired ${select_count} times for one selection`);
    if (close_count !== 0)
      throw new Error (`onClose fired ${close_count} times after selection`);
    if (container.querySelector ('.b-crawlerdialog'))
      throw new Error ('CrawlerDialog still mounted after selection');
  } finally {
    dispose();
    container.remove();
  }

  return true;
}
sub_tests.push (['crawlerdialog_select', test_crawlerdialog_select_suppresses_close]);

// == Master runner ==
/// Runs all sub-tests in sequence.
export async function test_dialog (): Promise<boolean>
{
  if (!sub_tests.length)
    throw new Error ('dialog: no sub-tests registered');
  const failures: string[] = [];
  for (const [name, fn] of sub_tests) {
    try {
      await fn();
    } catch (e) {
      failures.push (`${name}: ${(e as Error)?.message ?? e}`);
    }
  }
  if (failures.length)
    throw new Error ('dialog failures:\n  ' + failures.join ('\n  '));
  return true;
}
