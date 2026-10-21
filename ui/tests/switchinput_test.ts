// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { run_tests } from './runner.js';
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

async function test_switchinput_toggle (): Promise<boolean>
{
  for (const value of [false, true]) {
    let emitted: boolean | undefined;
    const si = mount_switchinput ({
      value,
      'on:valuechange': e => { emitted = (e.target as any).value; },
    });
    try {
      await Dom.ui_next_frame();
      const cb = si.checkbox();
      if (!cb || cb.checked !== value)
        throw new Error (`initial switch state should be ${value}`);
      click_checkbox (cb);
      if (cb.checked !== !value || emitted !== !value)
        throw new Error (`switch did not toggle from ${value} to ${!value}`);
    } finally {
      si.cleanup();
    }
  }
  return true;
}
sub_tests.push (['toggle', test_switchinput_toggle]);

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
  for (const [key, keyCode, value] of [
    ['ArrowRight', 39, false], ['ArrowLeft', 37, true],
    ['ArrowDown', 40, false], ['ArrowUp', 38, true],
  ] as const) {
    let emitted: boolean | undefined;
    const si = mount_switchinput ({
      value,
      'on:valuechange': e => { emitted = (e.target as any).value; },
    });
    try {
      await Dom.ui_next_frame();
      const cb = si.checkbox();
      const lb = si.label();
      if (!cb || !lb || cb.checked !== value)
        throw new Error (`${key}: unexpected initial switch state`);
      lb.dispatchEvent (new KeyboardEvent ('keydown', { key, keyCode, bubbles: true, cancelable: true }));
      if (cb.checked !== !value || emitted !== !value)
        throw new Error (`${key} did not set and emit ${!value}`);
    } finally {
      si.cleanup();
    }
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
  for (const [expected, values] of [
    [false, ['f', 'F', 'n', 'N', '']],
    [true, ['t', 'T', 'y', 'Y', 'true', '1', 'x']],
  ] as const) {
    for (const value of values) {
      let emitted: boolean | undefined;
      const si = mount_switchinput ({
        value,
        'on:valuechange': e => { emitted = (e.target as any).value; },
      });
      try {
        await Dom.ui_next_frame();
        const cb = si.checkbox();
        if (!cb || cb.checked !== expected)
          throw new Error (`string "${value}" should constrain to ${expected}`);
        if (emitted !== undefined)
          throw new Error (`string "${value}" emitted an edit: ${emitted}`);
      } finally {
        si.cleanup();
      }
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

export async function test_switchinput (): Promise<boolean>
{
  return run_tests ('switchinput', sub_tests);
}
