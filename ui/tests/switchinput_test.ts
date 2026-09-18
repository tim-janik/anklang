// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { createComponent, render } from 'solid-js/web';
import { createSignal } from 'solid-js';
import { SwitchInput } from '../b/switchinput';
import * as Dom from '../dom';
import { TestTimers } from './timers';

/// Mount a SwitchInput for testing and return helpers.
function mount_switchinput (props: {
  value?: boolean | string | (() => boolean | string);
  readonly?: boolean;
  class?: string;
  'on:valuechange'?: (e: Event) => void;
  [key: string]: any;
})
{
  const container = document.createElement ('div');
  document.body.appendChild (container);

  const dispose = render (() => createComponent (SwitchInput, props as any), container);

  const label = () => container.querySelector ('label.b-switchinput') as HTMLLabelElement | null;
  const checkbox = () => container.querySelector ('input[type=checkbox]') as HTMLInputElement | null;
  const cleanup = () => {
    dispose();
    container.remove();
  };
  return { container, label, checkbox, cleanup };
}

/// Click the checkbox (even though hidden) to trigger the native toggle + change event.
function click_checkbox (cb: HTMLInputElement)
{
  cb.click();
}

// == Test registry ==
const sub_tests: [string, () => Promise<any>][] = [];

// =============================================================================
// Toggle tests (each tests one direction to match the controlled semantics)
// =============================================================================

/// Test that toggling from false to true emits valuechange=true.
async function test_switchinput_toggle_on (): Promise<boolean>
{
  let emitted: any = undefined;
  const si = mount_switchinput ({
    value: false,
    'on:valuechange': e => { emitted = (e.target as any).value; },
  });
  try {
    await Dom.ui_next_frame();
    const cb = si.checkbox();
    if (!cb) throw new Error ('SwitchInput checkbox not rendered');
    if (cb.checked) throw new Error ('initial state should be unchecked');
    click_checkbox (cb);
    await Dom.ui_next_frame();
    if (!cb.checked) throw new Error ('checkbox not checked after click');
    if (emitted !== true)
      throw new Error (`valuechange payload wrong: ${emitted}`);
  } finally {
    si.cleanup();
  }
  return true;
}
sub_tests.push (['toggle_on', test_switchinput_toggle_on]);

/// Test that toggling from true to false emits valuechange=false.
async function test_switchinput_toggle_off (): Promise<boolean>
{
  let emitted: any = undefined;
  const si = mount_switchinput ({
    value: true,
    'on:valuechange': e => { emitted = (e.target as any).value; },
  });
  try {
    await Dom.ui_next_frame();
    const cb = si.checkbox();
    if (!cb) throw new Error ('SwitchInput checkbox not rendered');
    if (!cb.checked) throw new Error ('initial state should be checked');
    click_checkbox (cb);
    await Dom.ui_next_frame();
    if (cb.checked) throw new Error ('checkbox not unchecked after click');
    if (emitted !== false)
      throw new Error (`valuechange payload wrong: ${emitted}`);
  } finally {
    si.cleanup();
  }
  return true;
}
sub_tests.push (['toggle_off', test_switchinput_toggle_off]);

// =============================================================================
// Readonly tests
// =============================================================================

/// Test that readonly mode prevents toggling.
async function test_switchinput_readonly (): Promise<boolean>
{
  let emit_count = 0;
  const si = mount_switchinput ({
    value: false,
    readonly: true,
    'on:valuechange': () => { emit_count++; },
  });
  try {
    await Dom.ui_next_frame();
    const cb = si.checkbox();
    if (!cb) throw new Error ('SwitchInput not rendered');
    if (!cb.disabled) throw new Error ('checkbox should be disabled in readonly mode');
    // Click on a disabled checkbox is a no-op; it must not toggle or fire events.
    click_checkbox (cb);
    await Dom.ui_next_frame();
    if (cb.checked) throw new Error ('readonly checkbox should not toggle on click');
    if (emit_count !== 0)
      throw new Error (`readonly mode emitted ${emit_count} events`);
  } finally {
    si.cleanup();
  }
  return true;
}
sub_tests.push (['readonly', test_switchinput_readonly]);

// =============================================================================
// Keyboard tests
// =============================================================================

/// Test keyboard navigation: RIGHT/DOWN check, LEFT/UP uncheck.
async function test_switchinput_keyboard (): Promise<boolean>
{
  // Part 1: start unchecked, RIGHT/DOWN should check
  let emitted: any = undefined;
  const si = mount_switchinput ({
    value: false,
    'on:valuechange': e => { emitted = (e.target as any).value; },
  });
  try {
    await Dom.ui_next_frame();
    const cb = si.checkbox();
    const lb = si.label();
    if (!cb || !lb) throw new Error ('SwitchInput not rendered');
    // RIGHT should check when unchecked (emits true because prop=false, boolvalue=true)
    emitted = undefined;
    lb.dispatchEvent (new KeyboardEvent ('keydown', {
      keyCode: 39, key: 'ArrowRight', bubbles: true, cancelable: true,
    }));
    await Dom.ui_next_frame();
    if (!cb.checked) throw new Error ('RIGHT did not check the switch');
    if (emitted !== true)
      throw new Error (`RIGHT valuechange payload wrong: ${emitted}`);
  } finally {
    // dispose of this mount before starting the next
    si.cleanup();
  }

  // Part 2: start checked, LEFT/UP should uncheck
  emitted = undefined;
  const si2 = mount_switchinput ({
    value: true,
    'on:valuechange': e => { emitted = (e.target as any).value; },
  });
  try {
    await Dom.ui_next_frame();
    const cb = si2.checkbox();
    const lb = si2.label();
    if (!cb || !lb) throw new Error ('SwitchInput not rendered');
    if (!cb.checked) throw new Error ('initial should be checked');
    // LEFT should uncheck when checked (emits false because prop=true, boolvalue=false)
    emitted = undefined;
    lb.dispatchEvent (new KeyboardEvent ('keydown', {
      keyCode: 37, key: 'ArrowLeft', bubbles: true, cancelable: true,
    }));
    await Dom.ui_next_frame();
    if (cb.checked) throw new Error ('LEFT did not uncheck the switch');
    if (emitted !== false)
      throw new Error (`LEFT valuechange payload wrong: ${emitted}`);
  } finally {
    si2.cleanup();
  }

  // Part 3: start unchecked, DOWN should check (same as RIGHT)
  emitted = undefined;
  const si3 = mount_switchinput ({
    value: false,
    'on:valuechange': e => { emitted = (e.target as any).value; },
  });
  try {
    await Dom.ui_next_frame();
    const cb = si3.checkbox();
    const lb = si3.label();
    if (!cb || !lb) throw new Error ('SwitchInput not rendered');
    if (cb.checked) throw new Error ('initial should be unchecked');
    // DOWN should check (emits true because prop=false, boolvalue=true)
    emitted = undefined;
    lb.dispatchEvent (new KeyboardEvent ('keydown', {
      keyCode: 40, key: 'ArrowDown', bubbles: true, cancelable: true,
    }));
    await Dom.ui_next_frame();
    if (!cb.checked) throw new Error ('DOWN did not check the switch');
    if (emitted !== true)
      throw new Error (`DOWN valuechange payload wrong: ${emitted}`);
  } finally {
    si3.cleanup();
  }

  // Part 4: start checked, UP should uncheck
  emitted = undefined;
  const si4 = mount_switchinput ({
    value: true,
    'on:valuechange': e => { emitted = (e.target as any).value; },
  });
  try {
    await Dom.ui_next_frame();
    const cb = si4.checkbox();
    const lb = si4.label();
    if (!cb || !lb) throw new Error ('SwitchInput not rendered');
    if (!cb.checked) throw new Error ('initial should be checked');
    // UP should uncheck (emits false because prop=true, boolvalue=false)
    emitted = undefined;
    lb.dispatchEvent (new KeyboardEvent ('keydown', {
      keyCode: 38, key: 'ArrowUp', bubbles: true, cancelable: true,
    }));
    await Dom.ui_next_frame();
    if (cb.checked) throw new Error ('UP did not uncheck the switch');
    if (emitted !== false)
      throw new Error (`UP valuechange payload wrong: ${emitted}`);
  } finally {
    si4.cleanup();
  }
  return true;
}
sub_tests.push (['keyboard', test_switchinput_keyboard]);

/// Test that keyboard events are ignored in readonly mode.
async function test_switchinput_keyboard_readonly (): Promise<boolean>
{
  let emit_count = 0;
  const si = mount_switchinput ({
    value: false,
    readonly: true,
    'on:valuechange': () => { emit_count++; },
  });
  try {
    await Dom.ui_next_frame();
    const cb = si.checkbox();
    const lb = si.label();
    if (!cb || !lb) throw new Error ('SwitchInput not rendered');
    // Try all four direction keys
    for (const keyCode of [37, 38, 39, 40]) {
      lb.dispatchEvent (new KeyboardEvent ('keydown', {
        keyCode, bubbles: true, cancelable: true,
      }));
    }
    await Dom.ui_next_frame();
    if (cb.checked) throw new Error ('readonly switch toggled via keyboard');
    if (emit_count !== 0)
      throw new Error (`readonly keyboard emitted ${emit_count} events`);
  } finally {
    si.cleanup();
  }
  return true;
}
sub_tests.push (['keyboard_readonly', test_switchinput_keyboard_readonly]);

// =============================================================================
// Constrain / normalization tests
// =============================================================================

/// Test that the constrain function normalizes string values correctly.
async function test_switchinput_constrain_strings (): Promise<boolean>
{
  // 'f', 'F', 'n', 'N', empty string → false; other strings → true
  const false_strings = ['f', 'F', 'n', 'N', ''];
  for (const s of false_strings) {
    let emitted: any = undefined;
    const si = mount_switchinput ({
      value: s,
      'on:valuechange': e => { emitted = (e.target as any).value; },
    });
    try {
      await Dom.ui_next_frame();
      const cb = si.checkbox();
      if (!cb) throw new Error ('SwitchInput not rendered');
      if (cb.checked) throw new Error (`string "${s}" should constrain to false`);
      if (emitted !== undefined)
        throw new Error (`string "${s}" valuechange payload wrong: ${emitted}`);
    } finally {
      si.cleanup();
    }
  }
  // Truthy strings
  const true_strings = ['t', 'T', 'y', 'Y', 'true', '1', 'x'];
  for (const s of true_strings) {
    let emitted: any = undefined;
    const si = mount_switchinput ({
      value: s,
      'on:valuechange': e => { emitted = (e.target as any).value; },
    });
    try {
      await Dom.ui_next_frame();
      const cb = si.checkbox();
      if (!cb) throw new Error ('SwitchInput not rendered');
      if (!cb.checked) throw new Error (`string "${s}" should constrain to true`);
      if (emitted !== undefined)
        throw new Error (`string "${s}" valuechange payload wrong: ${emitted}`);
    } finally {
      si.cleanup();
    }
  }
  return true;
}
sub_tests.push (['constrain_strings', test_switchinput_constrain_strings]);

async function test_switchinput_backend_updates (): Promise<boolean>
{
  const [value, set_value] = createSignal (false);
  const edits: boolean[] = [];
  const si = mount_switchinput ({
    get value () { return value(); },
    'on:valuechange': e => edits.push ((e.target as any).value),
  });
  const timers = new TestTimers();
  try {
    const cb = si.checkbox()!;
    cb.click();
    if (!cb.checked)
      throw new Error ('edit did not stay visible');
    cb.click();
    if (edits.join (',') !== 'true,false')
      throw new Error ('quick change back was not sent');
    cb.click();
    set_value (false);
    await Dom.ui_next_frame();
    if (!cb.checked || timers.pending !== 1)
      throw new Error ('switch edit did not keep one grace timer');
    timers.run();
    if (cb.checked)
      throw new Error ('backend correction did not replace the edit');
    set_value (true);
    await Dom.ui_next_frame();
    if (!cb.checked || edits.length !== 3)
      throw new Error ('backend update was lost or emitted as an edit');
    cb.click();
    set_value (false);
    set_value (true);
    await Dom.ui_next_frame();
    if (cb.checked)
      throw new Error ('backend update interrupted a switch edit');
    timers.run();
    if (!cb.checked)
      throw new Error ('switch did not settle on the latest backend value');
    cb.click();
    si.cleanup();
    if (timers.pending)
      throw new Error ('switch kept a timer after disposal');
  } finally {
    si.cleanup();
    timers.restore();
  }
  return true;
}
sub_tests.push (['backend_updates', test_switchinput_backend_updates]);

// == Master runner ==
/// Single exported entry point runs all sub-tests in sequence.
export async function test_switchinput (): Promise<boolean>
{
  if (!sub_tests.length)
    throw new Error ('switchinput: no sub-tests registered');
  const failures: string[] = [];
  for (const [name, fn] of sub_tests) {
    try {
      await fn();
    } catch (e) {
      failures.push (`${name}: ${(e as Error)?.message ?? e}`);
    }
  }
  if (failures.length)
    throw new Error ('switchinput failures:\n  ' + failures.join ('\n  '));
  return true;
}
