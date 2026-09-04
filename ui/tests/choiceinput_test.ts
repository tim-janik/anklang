// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { createComponent, render } from 'solid-js/web';
import { ChoiceInput } from '../b/choiceinput';
import * as Util from '../util.js';
import * as Dom from '../dom';

/// Choices shared across tests: uri `a`→label `A`, `b`→`B`, `c`→`C`.
function sample_choices()
{
  return [
    { ident: 'a', label: 'Aaa', icon: 'Ⓐ' },
    { ident: 'b', label: 'Bbb', icon: 'Ⓑ' },
    { ident: 'c', label: 'Ccc', icon: 'Ⓒ' },
  ];
}

/// Mount a ChoiceInput for testing and return helpers.
function mount_choiceinput (props: {
  value?: string;
  choices?: any[];
  title?: string;
  label?: string;
  small?: boolean;
  disabled?: boolean;
  class?: string;
  onValueChange?: (uri: string) => void;
  'on:valuechange'?: (e: Event) => void;
  [key: string]: any;
})
{
  const container = document.createElement ('div');
  document.body.appendChild (container);

  const dispose = render (() => createComponent (ChoiceInput, props as any), container);

  const root = () => container.querySelector ('div.b-choiceinput') as HTMLDivElement | null;
  const dialog = () => container.querySelector ('dialog.b-contextmenu') as HTMLDialogElement | null;
  const cleanup = () => {
    // close any stray menu before disposing
    const d = dialog();
    if (d && d.open) d.close();
    dispose();
    container.remove();
  };
  return { container, root, dialog, cleanup };
}

/// Mimic the real open interaction: a mousedown followed by a click.
/// mousedown triggers the popup (prevent_event stops propagation), the
/// subsequent click is a no-op because the menu is already open.
function open_menu (root_el: Element)
{
  root_el.dispatchEvent (new MouseEvent ('mousedown', { bubbles: true, cancelable: true }));
  root_el.dispatchEvent (new MouseEvent ('click', { bubbles: true, cancelable: true }));
}

/// Dispatch a keydown event with the given keyCode on an element.
function send_keydown (el: Element, keyCode: number)
{
  el.dispatchEvent (new KeyboardEvent ('keydown', {
    keyCode, bubbles: true, cancelable: true,
  }));
}

// == Test registry ==
const sub_tests: [string, () => Promise<any>][] = [];

/// Test that the initial value is rendered from `choices` via the current-span.
async function test_choiceinput_initial_value (): Promise<boolean>
{
  const ci = mount_choiceinput ({
    value: 'b',
    choices: sample_choices(),
  });
  try {
    await Dom.ui_next_frame();
    const root_el = ci.root();
    if (!root_el) throw new Error ('ChoiceInput root not rendered');
    const current = root_el.querySelector ('span.-current') as HTMLElement | null;
    if (!current) throw new Error ('current span not found');
    if (current.textContent !== 'Bbb')
      throw new Error (`initial display wrong: "${current.textContent}"`);
  } finally {
    ci.cleanup();
  }
  return true;
}
sub_tests.push (['initial_value', test_choiceinput_initial_value]);

/// Test that the small layout swaps label/icon and applies the b-choice-small class.
async function test_choiceinput_small_layout (): Promise<boolean>
{
  const ci = mount_choiceinput ({
    value: 'a',
    choices: sample_choices(),
    small: true,
  });
  try {
    await Dom.ui_next_frame();
    const root_el = ci.root();
    if (!root_el) throw new Error ('ChoiceInput root not rendered');
    if (!root_el.classList.contains ('b-choice-small'))
      throw new Error (`small class missing: "${root_el.className}"`);
    if (root_el.classList.contains ('b-choice-big'))
      throw new Error (`big class present in small mode: "${root_el.className}"`);
    // small shows the icon rather than the label
    const current = root_el.querySelector ('span.-current') as HTMLElement | null;
    if (!current) throw new Error ('current span not found');
    if (current.textContent !== 'Ⓐ')
      throw new Error (`small display should be icon: "${current.textContent}"`);
  } finally {
    ci.cleanup();
  }
  return true;
}
sub_tests.push (['small_layout', test_choiceinput_small_layout]);

/// Test that the `class` prop is forwarded onto the root element.
async function test_choiceinput_class_forwarding (): Promise<boolean>
{
  const ci = mount_choiceinput ({
    value: 'a',
    choices: sample_choices(),
    class: 'b-objecteditor--ident',
  });
  try {
    await Dom.ui_next_frame();
    const root_el = ci.root();
    if (!root_el) throw new Error ('ChoiceInput root not rendered');
    if (!root_el.classList.contains ('b-choiceinput'))
      throw new Error ('root missing b-choiceinput class');
    if (!root_el.classList.contains ('b-objecteditor--ident'))
      throw new Error (`class not forwarded: "${root_el.className}"`);
  } finally {
    ci.cleanup();
  }
  return true;
}
sub_tests.push (['class_forwarding', test_choiceinput_class_forwarding]);

/// Test popup → activate → onValueChange + self-targeted `valuechange` event.
async function test_choiceinput_activate_emits (): Promise<boolean>
{
  let changed_uri: string | undefined;
  let emitted_value: string | undefined;
  let valuechange_count = 0;
  const ci = mount_choiceinput ({
    value: 'a',
    choices: sample_choices(),
    onValueChange: uri => { changed_uri = uri; },
    'on:valuechange': e => { emitted_value = (e.target as any).value; valuechange_count++; },
  });
  try {
    await Dom.ui_next_frame();
    const root_el = ci.root();
    if (!root_el) throw new Error ('ChoiceInput root not rendered');
    open_menu (root_el);
    await Dom.ui_next_frame();
    const d = ci.dialog();
    if (!d || !d.open) throw new Error ('choice menu did not open');

    // Activate the `c` item.
    await Dom.ui_click_wait ('button', { uri: 'c' });
    await Dom.ui_next_frame();

    if (changed_uri !== 'c')
      throw new Error (`onValueChange not called with 'c': ${changed_uri}`);
    if (valuechange_count !== 1)
      throw new Error (`expected one valuechange event, got ${valuechange_count}`);
    if (emitted_value !== 'c')
      throw new Error (`valuechange target.value wrong: ${emitted_value}`);
    // root element value reflected
    if ((root_el as any).value !== 'c')
      throw new Error (`root value not updated: ${(root_el as any).value}`);
    // current span now shows Ccc
    const current = root_el.querySelector ('span.-current') as HTMLElement | null;
    if (!current) throw new Error ('current span not found');
    if (current.textContent !== 'Ccc')
      throw new Error (`display not updated after activate: "${current.textContent}"`);
  } finally {
    ci.cleanup();
  }
  return true;
}
sub_tests.push (['activate_emits', test_choiceinput_activate_emits]);

/// Regression test for C1: closing the menu (via activate) then reopening must
/// work — the stale `cmenu_el` ref previously prevented the second popup.
async function test_choiceinput_reopen_after_activate (): Promise<boolean>
{
  const ci = mount_choiceinput ({
    value: 'a',
    choices: sample_choices(),
    onValueChange: () => {},
  });
  try {
    await Dom.ui_next_frame();
    const root_el = ci.root()!;

    // First open + activate (closes the menu and disposes the ContextMenu).
    open_menu (root_el);
    await Dom.ui_next_frame();
    if (!ci.dialog()?.open) throw new Error ('first open failed');
    await Dom.ui_click_wait ('button', { uri: 'b' });
    await Dom.ui_next_frame();
    if (ci.dialog() !== null)
      throw new Error ('ContextMenu not disposed after activate');

    // Second open must recreate the ContextMenu and actually show the dialog.
    open_menu (root_el);
    await Dom.ui_next_frame();
    const d = ci.dialog();
    if (!d || !d.open)
      throw new Error ('menu did not reopen (stale cmenu_el regression)');
  } finally {
    ci.cleanup();
  }
  return true;
}
sub_tests.push (['reopen_after_activate', test_choiceinput_reopen_after_activate]);

/// Regression test for C1 via the native close path (Escape / dialog.close()):
/// the onclose handler must clear the stale ref so a subsequent open works.
async function test_choiceinput_reopen_after_native_close (): Promise<boolean>
{
  const ci = mount_choiceinput ({
    value: 'a',
    choices: sample_choices(),
  });
  try {
    await Dom.ui_next_frame();
    const root_el = ci.root()!;

    open_menu (root_el);
    await Dom.ui_next_frame();
    const d1 = ci.dialog();
    if (!d1 || !d1.open) throw new Error ('first open failed');
    // Simulate Escape / backdrop close: the dialog emits a native `close` event.
    d1.close();
    await Dom.ui_next_frame();
    if (ci.dialog() !== null)
      throw new Error ('ContextMenu not disposed after native close');

    // Reopen — must work despite the native close path.
    open_menu (root_el);
    await Dom.ui_next_frame();
    const d2 = ci.dialog();
    if (!d2 || !d2.open)
      throw new Error ('menu did not reopen after native close');
  } finally {
    ci.cleanup();
  }
  return true;
}
sub_tests.push (['reopen_after_native_close', test_choiceinput_reopen_after_native_close]);

/// Test UP/DOWN keyboard navigation while the menu is closed: it cycles through
/// the choices by activating them without opening the popup.
async function test_choiceinput_keyboard_nav (): Promise<boolean>
{
  const changed: string[] = [];
  const ci = mount_choiceinput ({
    value: 'a',
    choices: sample_choices(),
    onValueChange: uri => { changed.push (uri); },
  });
  try {
    await Dom.ui_next_frame();
    const root_el = ci.root()!;
    if (root_el.querySelector ('span.-current')!.textContent !== 'Aaa')
      throw new Error ('unexpected initial display');

    // DOWN twice: a→b→c
    send_keydown (root_el, Util.KeyCode.DOWN);
    await Dom.ui_next_frame();
    if (changed[0] !== 'b') throw new Error (`DOWN should select 'b': ${changed[0]}`);
    send_keydown (root_el, Util.KeyCode.DOWN);
    await Dom.ui_next_frame();
    if (changed[1] !== 'c') throw new Error (`DOWN should select 'c': ${changed[1]}`);

    // UP once: c→b
    send_keydown (root_el, Util.KeyCode.UP);
    await Dom.ui_next_frame();
    if (changed[2] !== 'b') throw new Error (`UP should select 'b': ${changed[2]}`);

    // DOWN at the last item must not wrap/activate.
    send_keydown (root_el, Util.KeyCode.DOWN); // b→c
    await Dom.ui_next_frame();
    if (changed[3] !== 'c') throw new Error (`DOWN should select 'c': ${changed[3]}`);
    const before = changed.length;
    send_keydown (root_el, Util.KeyCode.DOWN); // c→(nothing)
    await Dom.ui_next_frame();
    if (changed.length !== before)
      throw new Error (`DOWN past end should not activate (got ${changed[before]})`);

    // Enter opens the popup.
    send_keydown (root_el, Util.KeyCode.ENTER);
    await Dom.ui_next_frame();
    if (!ci.dialog()?.open) throw new Error ('Enter should open the popup');
  } finally {
    ci.cleanup();
  }
  return true;
}
sub_tests.push (['keyboard_nav', test_choiceinput_keyboard_nav]);

/// Test the disabled state: aria-disabled and an inert (tabindex -1) focus element.
async function test_choiceinput_disabled_state (): Promise<boolean>
{
  const ci = mount_choiceinput ({
    value: 'a',
    choices: sample_choices(),
    disabled: true,
  });
  try {
    await Dom.ui_next_frame();
    const root_el = ci.root();
    if (!root_el) throw new Error ('ChoiceInput root not rendered');
    if (root_el.getAttribute ('aria-disabled') !== 'true')
      throw new Error (`aria-disabled not set: "${root_el.getAttribute ('aria-disabled')}"`);
    const pophere = root_el.querySelector ('.b-choice-current') as HTMLElement | null;
    if (!pophere) throw new Error ('focus element not found');
    if (pophere.getAttribute ('tabindex') !== '-1')
      throw new Error (`disabled focus tabindex wrong: "${pophere.getAttribute ('tabindex')}"`);
  } finally {
    ci.cleanup();
  }
  return true;
}
sub_tests.push (['disabled_state', test_choiceinput_disabled_state]);

/// Test that the `data-tip` attribute is computed from the current value/label
/// and refreshes reactively when the value changes.
async function test_choiceinput_data_tip_reactive (): Promise<boolean>
{
  const ci = mount_choiceinput ({
    value: 'a',
    choices: sample_choices(),
    label: 'Volume',
  });
  try {
    await Dom.ui_next_frame();
    const root_el = ci.root()!;
    const tip_of = () => root_el.getAttribute ('data-tip') ?? '';
    if (!/Volume/.test (tip_of()))
      throw new Error (`data-tip missing label: "${tip_of()}"`);
    if (!/Aaa/.test (tip_of()))
      throw new Error (`data-tip missing current choice label: "${tip_of()}"`);

    // change value via activate and check the tip updates reactively
    open_menu (root_el);
    await Dom.ui_next_frame();
    await Dom.ui_click_wait ('button', { uri: 'c' });
    await Dom.ui_next_frame();
    if (!/Ccc/.test (tip_of()))
      throw new Error (`data-tip not updated after activate: "${tip_of()}"`);
  } finally {
    ci.cleanup();
  }
  return true;
}
sub_tests.push (['data_tip_reactive', test_choiceinput_data_tip_reactive]);

// == Master runner ==
/// Single exported entry point runs all sub-tests in sequence.
export async function test_choiceinput (): Promise<boolean>
{
  if (!sub_tests.length)
    throw new Error ('choiceinput: no sub-tests registered');
  const failures: string[] = [];
  for (const [name, fn] of sub_tests) {
    try {
      await fn();
    } catch (e) {
      failures.push (`${name}: ${(e as Error)?.message ?? e}`);
    }
  }
  if (failures.length)
    throw new Error ('choiceinput failures:\n  ' + failures.join ('\n  '));
  return true;
}