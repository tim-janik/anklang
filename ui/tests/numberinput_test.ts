// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { createComponent, render } from 'solid-js/web';
import { createSignal } from 'solid-js';
import { NumberInput } from '../b/numberinput';
import * as Dom from '../dom';

/// Mount a NumberInput for testing and return helpers.
function mount_numberinput (props: {
  value?: number | (() => number);
  min?: number;
  max?: number;
  step?: number;
  allowfloat?: boolean;
  readonly?: boolean;
  class?: string;
  'on:valuechange'?: (e: Event) => void;
  [key: string]: any;
})
{
  const container = document.createElement ('div');
  document.body.appendChild (container);

  const dispose = render (() => createComponent (NumberInput, props as any), container);

  const root = () => container.querySelector ('label.b-numberinput') as HTMLLabelElement | null;
  const slider = () => container.querySelector ('input[type=range]') as HTMLInputElement | null;
  const number_input = () => container.querySelector ('input[type=number]') as HTMLInputElement | null;
  const cleanup = () => {
    dispose();
    container.remove();
  };
  return { container, root, slider, number_input, cleanup };
}

/// Dispatch an `input` event on an element carrying a fresh `.value`.
function send_input (el: HTMLInputElement, value: string)
{
  el.value = value;
  el.dispatchEvent (new Event ('input', { bubbles: true, cancelable: true }));
}

// == Test registry ==
const sub_tests: [string, () => Promise<any>][] = [];

/// Test that typing beyond `max` is clamped and reported via valuechange.
async function test_numberinput_clamp_on_input (): Promise<boolean>
{
  let emitted: any = undefined;
  const ni = mount_numberinput ({
    value: 5, min: 0, max: 10,
    'on:valuechange': e => { emitted = (e.target as any).value; },
  });
  try {
    await Dom.ui_next_frame();
    const num = ni.number_input();
    const sld = ni.slider();
    if (!num || !sld) throw new Error ('NumberInput fields not rendered');
    send_input (num, '999');
    await Dom.ui_next_frame();
    // value is clamped to max=10 on both fields and in the event payload
    if (num.value !== '10')
      throw new Error (`number field not clamped: "${num.value}"`);
    if (sld.value !== '10')
      throw new Error (`slider not clamped: "${sld.value}"`);
    if (emitted !== 10)
      throw new Error (`valuechange payload wrong: ${emitted}`);
  } finally {
    ni.cleanup();
  }
  return true;
}
sub_tests.push (['clamp_on_input', test_numberinput_clamp_on_input]);

/// Test that integer-only mode rounds and clamps on input.
async function test_numberinput_integer_rounding (): Promise<boolean>
{
  let emitted: any = undefined;
  const ni = mount_numberinput ({
    value: 0, min: 0, max: 10, allowfloat: false,
    'on:valuechange': e => { emitted = (e.target as any).value; },
  });
  try {
    await Dom.ui_next_frame();
    const num = ni.number_input();
    if (!num) throw new Error ('number field not rendered');
    // 3.7 must round to 4 (v|0 of v+0.5)
    send_input (num, '3.7');
    await Dom.ui_next_frame();
    if (num.value !== '4')
      throw new Error (`integer rounding wrong: "${num.value}"`);
    if (emitted !== 4)
      throw new Error (`valuechange payload wrong: ${emitted}`);
    // 12.4 clamps to max=10
    send_input (num, '12.4');
    await Dom.ui_next_frame();
    if ((num.value as any) !== '10')
      throw new Error (`clamp after rounding wrong: "${num.value}"`);
    if (emitted !== 10)
      throw new Error (`valuechange payload wrong: ${emitted}`);
  } finally {
    ni.cleanup();
  }
  return true;
}
sub_tests.push (['integer_rounding', test_numberinput_integer_rounding]);

/// Test that an out-of-range value pushed by the parent is enforced back via valuechange
async function test_numberinput_external_enforce (): Promise<boolean>
{
  let emitted: any = undefined;
  const [get_value, set_value] = createSignal<number> (5);
  const ni = mount_numberinput ({
    get value () { return get_value(); },
    min: 0, max: 10,
    'on:valuechange': e => { emitted = (e.target as any).value; },
  });
  try {
    await Dom.ui_next_frame();
    const num = ni.number_input();
    const sld = ni.slider();
    if (!num || !sld) throw new Error ('NumberInput fields not rendered');
    // parent pushes an out-of-range value: component must push back valuechange=10
    set_value (999);
    await Dom.ui_next_frame();
    if (num.value !== '10')
      throw new Error (`display not enforced: "${num.value}"`);
    if (sld.value !== '10')
      throw new Error (`slider not enforced: "${sld.value}"`);
    if (emitted !== 10)
      throw new Error (`external enforce did not emit valuechange: ${emitted}`);
  } finally {
    ni.cleanup();
  }
  return true;
}
sub_tests.push (['external_enforce', test_numberinput_external_enforce]);

/// Test that a value already within range does not fire a spurious valuechange.
async function test_numberinput_no_spurious_emit (): Promise<boolean>
{
  let emit_count = 0;
  const ni = mount_numberinput ({
    value: 5, min: 0, max: 10,
    'on:valuechange': () => { emit_count++; },
  });
  try {
    await Dom.ui_next_frame();
    if (emit_count !== 0)
      throw new Error (`in-range initial value emitted ${emit_count} events`);
    const num = ni.number_input();
    if (!num) throw new Error ('number field not rendered');
    // typing the same value should not emit
    send_input (num, '5');
    await Dom.ui_next_frame();
    if (emit_count !== 0)
      throw new Error (`unchanged value emitted ${emit_count} events`);
  } finally {
    ni.cleanup();
  }
  return true;
}
sub_tests.push (['no_spurious_emit', test_numberinput_no_spurious_emit]);

// == Master runner ==
/// Single exported entry point runs all sub-tests in sequence.
export async function test_numberinput (): Promise<boolean>
{
  if (!sub_tests.length)
    throw new Error ('numberinput: no sub-tests registered');
  const failures: string[] = [];
  for (const [name, fn] of sub_tests) {
    try {
      await fn();
    } catch (e) {
      failures.push (`${name}: ${(e as Error)?.message ?? e}`);
    }
  }
  if (failures.length)
    throw new Error ('numberinput failures:\n  ' + failures.join ('\n  '));
  return true;
}
