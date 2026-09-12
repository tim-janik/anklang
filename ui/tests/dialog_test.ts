// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { createComponent, render } from 'solid-js/web';
import { createSignal, Show } from 'solid-js';
import { AboutDialog } from '../b/aboutdialog';
import { PreferencesDialog } from '../b/preferencesdialog';
import { CrawlerDialog } from '../b/crawlerdialog';
import * as Dom from '../dom';
import * as Ase from '../../ase/gen/api-jsonipc.g.ts';

// == Test registry ==
const sub_tests: [string, () => Promise<any>][] = [];

/// Closing AboutDialog fires onClose once (M8).
async function test_aboutdialog_close_once (): Promise<boolean>
{
  const [shown, set_shown] = createSignal (true);
  let close_count: number = 0;
  const get_close_count = () => close_count;
  const container = document.createElement ('div');
  document.body.appendChild (container);
  const dispose = render (() => createComponent (Show, {
    get when () { return shown(); },
    keyed: true,
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
    if (get_close_count() !== 0)
      throw new Error (`onClose fired before close: ${get_close_count()}`);

    button.click();
    await Dom.ui_next_frame();
    await Dom.ui_next_frame();

    if (get_close_count() !== 1)
      throw new Error (`onClose fired ${get_close_count()} times for one close`);
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
async function test_preferencesdialog_close_once (): Promise<boolean>
{
  const [shown, set_shown] = createSignal (true);
  let close_count: number = 0;
  const get_close_count = () => close_count;
  const container = document.createElement ('div');
  document.body.appendChild (container);
  const dispose = render (() => createComponent (Show, {
    get when () { return shown(); },
    keyed: true,
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

    if (get_close_count() !== 1)
      throw new Error (`onClose fired ${get_close_count()} times for one close`);
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
  let close_count: number = 0;
  const get_close_count = () => close_count;
  const container = document.createElement ('div');
  document.body.appendChild (container);
  const dispose = render (() => createComponent (Show, {
    get when () { return shown(); },
    keyed: true,
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

    if (get_close_count() !== 1)
      throw new Error (`onClose fired ${get_close_count()} times for one close`);
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
  let select_count: number = 0, close_count: number = 0;
  const get_close_count = () => close_count;
  const container = document.createElement ('div');
  document.body.appendChild (container);
  const dispose = render (() => createComponent (Show, {
    get when () { return shown(); },
    keyed: true,
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
    if (get_close_count() !== 0)
      throw new Error (`onClose fired ${get_close_count()} times after selection`);
    if (container.querySelector ('.b-crawlerdialog'))
      throw new Error ('CrawlerDialog still mounted after selection');
  } finally {
    dispose();
    container.remove();
  }

  return true;
}
sub_tests.push (['crawlerdialog_select', test_crawlerdialog_select_suppresses_close]);

async function wait_for (predicate: () => boolean)
{
  const deadline = Date.now() + 4000;
  while (!predicate()) {
    if (Date.now() >= deadline) throw new Error ('dialog update timed out');
    await Dom.ui_next_frame();
  }
}

async function test_dialog_reopen (): Promise<boolean>
{
  for (const component of [CrawlerDialog, PreferencesDialog]) {
    const [shown, set_shown] = createSignal (true);
    let close_count = 0;
    const get_close_count = () => close_count;
    const container = document.createElement ('div');
    document.body.appendChild (container);
    const dispose = render (() => createComponent (component, {
      get shown () { return shown(); },
      cwd: '/tmp',
      onClose: () => { close_count++; set_shown (false); },
    }), container);
    try {
      const dialog = container.querySelector ('dialog')!;
      for (let count = 1; count <= 2; count++) {
        await wait_for (() => dialog.open);
        dialog.dispatchEvent (new KeyboardEvent ('keydown', { key: 'Escape', bubbles: true, cancelable: true }));
        await wait_for (() => !dialog.open);
        await Dom.ui_next_frame();
        if (get_close_count() !== count)
          throw new Error (`${component.name} closed ${get_close_count()} times, expected ${count}`);
        if (count === 1) set_shown (true);
      }
    } finally {
      dispose();
      container.remove();
    }
    await Dom.ui_next_frame();
    if (get_close_count() !== 2)
      throw new Error (`${component.name} emitted another close during cleanup`);
  }
  return true;
}
sub_tests.push (['reopen', test_dialog_reopen]);

async function test_crawler_loading (): Promise<boolean>
{
  let release_folder!: () => void;
  let release_entries!: () => void;
  const folder_gate = new Promise<void> (resolve => { release_folder = resolve; });
  const entries_gate = new Promise<void> (resolve => { release_entries = resolve; });
  let pending = 0;
  const original_send = Ase.Jsonipc.send;
  Ase.Jsonipc.send = async function (method, params) {
    const result = await original_send.call (this, method, params);
    if (params[0] instanceof Ase.ResourceCrawler && ['get/folder', 'get/entries'].includes (method)) {
      pending++;
      await (method === 'get/folder' ? folder_gate : entries_gate);
    }
    return result;
  };
  const [shown, set_shown] = createSignal (true);
  let selected = '', select_count = 0, close_count = 0;
  const selections = () => select_count;
  const closes = () => close_count;
  const container = document.createElement ('div');
  document.body.appendChild (container);
  const dispose = render (() => createComponent (CrawlerDialog, {
    get shown () { return shown(); },
    cwd: '/tmp',
    existing: false,
    onSelect: uri => { selected = uri; select_count++; set_shown (false); },
    onClose: () => { close_count++; set_shown (false); },
  }), container);
  try {
    const select_button = container.querySelector ('button.button-xl') as HTMLButtonElement;
    const pathentry = container.querySelector ('input.-pathentry') as HTMLInputElement;
    pathentry.value = 'review-save.anklang';
    if (!select_button.disabled)
      throw new Error ('Select enabled before crawler creation');
    await wait_for (() => pending === 2);
    select_button.click();
    if (selections()) throw new Error ('Select accepted unresolved crawler properties');
    release_folder();
    await Dom.ui_next_frame();
    if (!select_button.disabled)
      throw new Error ('Select enabled before entries finished loading');
    release_entries();
    await wait_for (() => !select_button.disabled);
    select_button.click();
    select_button.click();
    await Dom.ui_next_frame();
    if (selected !== '/tmp/review-save.anklang' || selections() !== 1 || close_count !== 0)
      throw new Error (`unexpected selection: ${selected}, selected ${selections()}, closed ${close_count}`);
    set_shown (true);
    await wait_for (() => container.querySelector ('dialog')!.open);
    const buttons = container.querySelectorAll ('button.button-xl');
    (buttons[buttons.length - 1] as HTMLButtonElement).click();
    if (closes() !== 1)
      throw new Error ('Close suppressed after reopening a selected dialog');
  } finally {
    release_folder();
    release_entries();
    Ase.Jsonipc.send = original_send;
    dispose();
    container.remove();
  }
  return true;
}
sub_tests.push (['crawler_loading', test_crawler_loading]);

async function test_crawler_enter_selects (): Promise<boolean>
{
  let selected = '', close_count = 0;
  const container = document.createElement ('div');
  document.body.appendChild (container);
  const dispose = render (() => createComponent (CrawlerDialog, {
    shown: true,
    cwd: '/tmp',
    existing: false,
    onSelect: uri => { selected = uri; },
    onClose: () => { close_count++; },
  }), container);
  try {
    const select_button = container.querySelector ('button.button-xl') as HTMLButtonElement;
    await wait_for (() => !select_button.disabled);
    const pathentry = container.querySelector ('input.-pathentry') as HTMLInputElement;
    pathentry.value = '/tmp/review-enter.anklang';
    pathentry.dispatchEvent (new KeyboardEvent ('keydown', { key: 'Enter', bubbles: true, cancelable: true }));
    await wait_for (() => !!selected);
    if (selected !== '/tmp/review-enter.anklang')
      throw new Error (`Enter selected the wrong path: ${selected}`);
  } finally {
    dispose();
    container.remove();
  }
  if (close_count !== 0) throw new Error ('Enter selection also emitted close');
  return true;
}
sub_tests.push (['crawler_enter', test_crawler_enter_selects]);

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
