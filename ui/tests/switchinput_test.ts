// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { createComponent, render } from 'solid-js/web';
import { createSignal } from 'solid-js';
import { SwitchInput } from '../b/switchinput';
import * as Dom from '../dom';

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

/**
 * NOTE on SwitchInput's controlled-component behavior:
 * `emit_input_value` only dispatches 'valuechange' when the constrained
 * boolean differs from the current prop value (`local.value`).  This means:
 *   - toggling `false→true` with prop `false` → emits `true`  (differs)
 *   - toggling `true→false` with prop `false` → NO emit (matches prop)
 * The parent is expected to update the prop upon receiving valuechange.
 * Tests below respect this design.
 */

// =============================================================================
// Toggle tests (each tests one direction to match the controlled semantics)
// =============================================================================

/// Test that toggling from false to true emits valuechange=true.
export async function test_switchinput_toggle_on (): Promise<boolean>
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

/// Test that toggling from true to false emits valuechange=false.
export async function test_switchinput_toggle_off (): Promise<boolean>
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

// =============================================================================
// Readonly tests
// =============================================================================

/// Test that readonly mode prevents toggling.
export async function test_switchinput_readonly (): Promise<boolean>
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

// =============================================================================
// Keyboard tests
// =============================================================================

/// Test keyboard navigation: RIGHT/DOWN check, LEFT/UP uncheck.
export async function test_switchinput_keyboard (): Promise<boolean>
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
    // After toggle, checkbox is checked but prop is still false.
    // LEFT would uncheck but boolvalue=false == prop=false → no emit.
    // Instead, test DOWN from unchecked state.
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

/// Test that keyboard events are ignored in readonly mode.
export async function test_switchinput_keyboard_readonly (): Promise<boolean>
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

// =============================================================================
// Constrain / normalization tests
// =============================================================================

/// Test that the constrain function normalizes string values correctly.
export async function test_switchinput_constrain_strings (): Promise<boolean>
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
      // The createEffect normalizes the raw string (raw !== v) and emits valuechange=false.
      // Accept that emitted may be undefined if the effect hasn't flushed yet, or false.
      if (emitted !== undefined && emitted !== false)
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
      if (emitted !== undefined && emitted !== true)
        throw new Error (`string "${s}" valuechange payload wrong: ${emitted}`);
    } finally {
      si.cleanup();
    }
  }
  return true;
}

/// Test that the createEffect normalizes a raw non-boolean value and emits.
export async function test_switchinput_normalize_emit (): Promise<boolean>
{
  let emitted: any = undefined;
  const si = mount_switchinput ({
    value: 'nope',
    'on:valuechange': e => { emitted = (e.target as any).value; },
  });
  try {
    await Dom.ui_next_frame();
    const cb = si.checkbox();
    if (!cb) throw new Error ('SwitchInput not rendered');
    // 'nope' starts with 'n', so constrain returns false
    if (cb.checked) throw new Error ('"nope" should constrain to false');
    // The createEffect should have emitted valuechange=false (raw='nope' !== v=false)
    if (emitted !== false)
      throw new Error (`normalize did not emit valuechange: ${emitted}`);
  } finally {
    si.cleanup();
  }
  return true;
}

/// Test that an external value change via signal emits valuechange when
/// the constrained value actually changes.
export async function test_switchinput_external_enforce (): Promise<boolean>
{
  let emitted: any = undefined;
  // Use a signal that produces a string needing normalization
  const [get_value, set_value] = createSignal<string | boolean> ('false');
  const si = mount_switchinput ({
    get value () { return get_value(); },
    'on:valuechange': e => { emitted = (e.target as any).value; },
  });
  try {
    await Dom.ui_next_frame();
    const cb = si.checkbox();
    if (!cb) throw new Error ('SwitchInput not rendered');
    if (cb.checked) throw new Error ('initial "false" should constrain to false');
    // The effect normalizes the raw string (raw='false' !== v=false) and emits.
    // We don't care about the initial emission for this test.
    emitted = undefined;
    // Parent pushes a truthy string — the effect sees raw='true', v=true, raw !== v, so emits.
    set_value ('true');
    await Dom.ui_next_frame();
    if (!cb.checked) throw new Error ('checkbox did not reflect external "true"');
    if (emitted !== true)
      throw new Error (`external enforce did not emit valuechange: ${emitted}`);
    // Switch back to a falsy string
    emitted = undefined;
    set_value ('no');
    await Dom.ui_next_frame();
    if (cb.checked) throw new Error ('checkbox did not reflect external "no"');
    if (emitted !== false)
      throw new Error (`external enforce did not emit valuechange: ${emitted}`);
  } finally {
    si.cleanup();
  }
  return true;
}
