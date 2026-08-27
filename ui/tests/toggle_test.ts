// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { createComponent, render } from 'solid-js/web';
import { Toggle } from '../b/toggle';
import * as Dom from '../dom';

/// Mount a Toggle for testing and return helpers.
function mount_toggle (props: {
  value?: boolean;
  label?: string;
  disabled?: boolean;
  class?: string;
  onValueChange?: (value: boolean) => void;
  'on:valuechange'?: (e: Event) => void;
  [key: string]: any;
})
{
  const container = document.createElement ('div');
  document.body.appendChild (container);

  const dispose = render (() => createComponent (Toggle, props as any), container);

  const root = () => container.querySelector ('div.b-toggle') as HTMLDivElement | null;
  const label = () => container.querySelector ('.b-toggle-label') as HTMLDivElement | null;
  const cleanup = () => {
    dispose();
    container.remove();
  };
  return { container, root, label, cleanup };
}

/// Synthesize a primary-button press on the toggle root.
function press (root: HTMLElement)
{
  root.dispatchEvent (new PointerEvent ('pointerdown', { buttons: 1, bubbles: true, cancelable: true }));
}

/// Synthesize releasing the pointer over the toggle root.
function release (root: HTMLElement)
{
  root.dispatchEvent (new PointerEvent ('pointerup', { bubbles: true, cancelable: true }));
}

/// Press and release in sequence (a single user click).
function click_toggle (root: HTMLElement)
{
  press (root);
  release (root);
}

// == Test registry ==
const sub_tests: [string, () => Promise<any>][] = [];

// =============================================================================
// Click / visual-flip tests
// =============================================================================

/// Toggling from false to true flips the on/off classes and reports true.
async function test_toggle_on (): Promise<boolean>
{
  let emitted: any = undefined;
  const t = mount_toggle ({
    value: false,
    onValueChange: v => { emitted = v; },
  });
  try {
    await Dom.ui_next_frame();
    const root = t.root();
    const label = t.label();
    if (!root || !label) throw new Error ('Toggle not rendered');
    if (label.classList.contains ('b-toggle-on')) throw new Error ('initial state should be off');
    if (!label.classList.contains ('b-toggle-off')) throw new Error ('initial state missing b-toggle-off');
    click_toggle (root);
    await Dom.ui_next_frame();
    if (!label.classList.contains ('b-toggle-on')) throw new Error ('click did not apply b-toggle-on');
    if (label.classList.contains ('b-toggle-off')) throw new Error ('click did not remove b-toggle-off');
    if (emitted !== true)
      throw new Error (`onValueChange payload wrong: ${emitted}`);
  } finally {
    t.cleanup();
  }
  return true;
}
sub_tests.push (['toggle_on', test_toggle_on]);

/// Toggling from true to false flips the on/off classes and reports false.
async function test_toggle_off (): Promise<boolean>
{
  let emitted: any = undefined;
  const t = mount_toggle ({
    value: true,
    onValueChange: v => { emitted = v; },
  });
  try {
    await Dom.ui_next_frame();
    const root = t.root();
    const label = t.label();
    if (!root || !label) throw new Error ('Toggle not rendered');
    if (!label.classList.contains ('b-toggle-on')) throw new Error ('initial state should be on');
    click_toggle (root);
    await Dom.ui_next_frame();
    if (label.classList.contains ('b-toggle-on')) throw new Error ('click did not remove b-toggle-on');
    if (!label.classList.contains ('b-toggle-off')) throw new Error ('click did not apply b-toggle-off');
    if (emitted !== false)
      throw new Error (`onValueChange payload wrong: ${emitted}`);
  } finally {
    t.cleanup();
  }
  return true;
}
sub_tests.push (['toggle_off', test_toggle_off]);

// =============================================================================
// Event contract test
// =============================================================================

/// The valuechange event must expose the new value as event.target.value.
async function test_toggle_event_target_value (): Promise<boolean>
{
  let target_value: any = undefined;
  const t = mount_toggle ({
    value: true,
    'on:valuechange': e => { target_value = (e.target as any).value; },
  });
  try {
    await Dom.ui_next_frame();
    const root = t.root();
    if (!root) throw new Error ('Toggle not rendered');
    click_toggle (root);
    await Dom.ui_next_frame();
    if (target_value !== false)
      throw new Error (`event.target.value wrong: ${target_value}`);
    // The root element should carry the new value imperatively as well.
    if ((root as any).value !== false)
      throw new Error (`root element .value wrong: ${(root as any).value}`);
  } finally {
    t.cleanup();
  }
  return true;
}
sub_tests.push (['event_target_value', test_toggle_event_target_value]);

// =============================================================================
// Disabled test
// =============================================================================

/// A disabled toggle must not toggle or emit anything.
async function test_toggle_disabled (): Promise<boolean>
{
  let emit_count = 0;
  const t = mount_toggle ({
    value: false,
    disabled: true,
    onValueChange: () => { emit_count++; },
  });
  try {
    await Dom.ui_next_frame();
    const root = t.root();
    const label = t.label();
    if (!root || !label) throw new Error ('Toggle not rendered');
    if (root.getAttribute ('aria-disabled') !== 'true')
      throw new Error ('disabled toggle should set aria-disabled="true"');
    click_toggle (root);
    await Dom.ui_next_frame();
    if (label.classList.contains ('b-toggle-on')) throw new Error ('disabled toggle should not turn on');
    if (emit_count !== 0)
      throw new Error (`disabled toggle emitted ${emit_count} events`);
  } finally {
    t.cleanup();
  }
  return true;
}
sub_tests.push (['disabled', test_toggle_disabled]);

// =============================================================================
// Press-class test
// =============================================================================

/// The b-toggle-press class is added on pointerdown and removed on pointerup.
async function test_toggle_press_class (): Promise<boolean>
{
  const t = mount_toggle ({ value: false });
  try {
    await Dom.ui_next_frame();
    const root = t.root();
    const label = t.label();
    if (!root || !label) throw new Error ('Toggle not rendered');
    if (label.classList.contains ('b-toggle-press')) throw new Error ('initial state should not be pressed');
    press (root);
    if (!label.classList.contains ('b-toggle-press')) throw new Error ('pointerdown did not add b-toggle-press');
    release (root);
    await Dom.ui_next_frame();
    if (label.classList.contains ('b-toggle-press')) throw new Error ('pointerup did not remove b-toggle-press');
  } finally {
    t.cleanup();
  }
  return true;
}
sub_tests.push (['press_class', test_toggle_press_class]);

// =============================================================================
// Rapid double-click test
// =============================================================================

/// Two rapid clicks must toggle twice (optimistic local state, not lagged prop).
async function test_toggle_rapid_double_click (): Promise<boolean>
{
  const emitted: boolean[] = [];
  const t = mount_toggle ({
    value: false,
    onValueChange: v => { emitted.push (v); },
  });
  try {
    await Dom.ui_next_frame();
    const root = t.root();
    const label = t.label();
    if (!root || !label) throw new Error ('Toggle not rendered');
    click_toggle (root);
    click_toggle (root);
    await Dom.ui_next_frame();
    if (emitted.length !== 2)
      throw new Error (`expected 2 emits, got ${emitted.length}`);
    if (emitted[0] !== true)
      throw new Error (`first click should emit true, got ${emitted[0]}`);
    if (emitted[1] !== false)
      throw new Error (`second click should emit false, got ${emitted[1]}`);
    // Final visual state is back to off.
    if (label.classList.contains ('b-toggle-on')) throw new Error ('two clicks should leave toggle off');
    if (!label.classList.contains ('b-toggle-off')) throw new Error ('two clicks should end with b-toggle-off');
  } finally {
    t.cleanup();
  }
  return true;
}
sub_tests.push (['rapid_double_click', test_toggle_rapid_double_click]);

// =============================================================================
// Label / empty-class test
// =============================================================================

/// A missing/empty label applies the b-toggle-empty class.
async function test_toggle_label_empty (): Promise<boolean>
{
  const t = mount_toggle ({ value: false });
  try {
    await Dom.ui_next_frame();
    const label = t.label();
    if (!label) throw new Error ('Toggle not rendered');
    if (!label.classList.contains ('b-toggle-empty')) throw new Error ('empty label should apply b-toggle-empty');
    if (label.textContent !== '') throw new Error (`empty label expected, got "${label.textContent}"`);
  } finally {
    t.cleanup();
  }
  return true;
}
sub_tests.push (['label_empty', test_toggle_label_empty]);

// =============================================================================
// Pointer-cancel test
// =============================================================================

/// After pointerdown → pointercancel, the toggle must not fire and a
/// subsequent click must still work (pressed state was reset properly).
async function test_toggle_cancel (): Promise<boolean>
{
  let emit_count = 0;
  const t = mount_toggle ({
    value: false,
    onValueChange: () => { emit_count++; },
  });
  try {
    await Dom.ui_next_frame();
    const root = t.root();
    const label = t.label();
    if (!root || !label) throw new Error ('Toggle not rendered');
    // Press then cancel.
    press (root);
    if (!label.classList.contains ('b-toggle-press')) throw new Error ('pointerdown should add b-toggle-press');
    root.dispatchEvent (new PointerEvent ('pointercancel', { bubbles: true, cancelable: true }));
    if (label.classList.contains ('b-toggle-press')) throw new Error ('pointercancel should remove b-toggle-press');
    if (emit_count !== 0)
      throw new Error (`cancel emitted ${emit_count} events`);
    // A subsequent click should toggle once.
    click_toggle (root);
    await Dom.ui_next_frame();
    if (emit_count !== 1)
      throw new Error (`after cancel, click emitted ${emit_count} events (expected 1)`);
    if (!label.classList.contains ('b-toggle-on')) throw new Error ('after cancel, click should turn on');
  } finally {
    t.cleanup();
  }
  return true;
}
sub_tests.push (['cancel', test_toggle_cancel]);

// == Master runner ==
/// Single exported entry point runs all sub-tests in sequence.
export async function test_toggle (): Promise<boolean>
{
  if (!sub_tests.length)
    throw new Error ('toggle: no sub-tests registered');
  const failures: string[] = [];
  for (const [name, fn] of sub_tests) {
    try {
      await fn();
    } catch (e) {
      failures.push (`${name}: ${(e as Error)?.message ?? e}`);
    }
  }
  if (failures.length)
    throw new Error ('toggle failures:\n  ' + failures.join ('\n  '));
  return true;
}
