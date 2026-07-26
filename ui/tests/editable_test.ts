// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { createComponent, render } from 'solid-js/web';
import { Editable } from '../b/editable';
import * as Dom from '../dom';

/// Mount an Editable for testing and return helpers.
function mount_editable (props: {
  value?: string | number;
  clicks?: number;
  selectall?: boolean;
  class?: string;
  style?: string;
  onChange?: (e: CustomEvent) => void;
})
{
  const container = document.createElement ('div');
  document.body.appendChild (container);

  let input_ref: HTMLInputElement | undefined;
  const dispose = render (() => createComponent (Editable, {
    ...props,
    ref: (el: HTMLInputElement) => {
      input_ref = el;
      // expose activate() for tests (already attached on the element by Editable)
    },
  }), container);

  const span = () => container.querySelector ('span') as HTMLSpanElement | null;
  const input = () => container.querySelector ('input') as HTMLInputElement | null;
  const cleanup = () => {
    dispose();
    container.remove();
  };
  return { container, span, input, input_ref: () => input_ref, cleanup };
}

/// Dispatch a KeyboardEvent with the given key on an element.
function press_key (el: Element, key: string)
{
  el.dispatchEvent (new KeyboardEvent ('keydown', {
    key, code: key, bubbles: true, cancelable: true,
  }));
}

// == Test registry ==
const sub_tests: [string, () => Promise<any>][] = [];

/// Test that the initial value is shown and the input is inert (not editing).
async function test_editable_initial_value (): Promise<boolean>
{
  const ed = mount_editable ({ value: 'hello', clicks: 1 });
  try {
    await Dom.ui_next_frame();
    const input = ed.input();
    if (!input) throw new Error ('Editable input not rendered');
    if (input.value !== 'hello')
      throw new Error (`initial value wrong: "${input.value}"`);
    if (!input.inert)
      throw new Error ('input should be inert initially');
    // The `inert` property must reflect the content attribute so that the
    // `[&[inert]]` Tailwind selectors (bg-transparent, pointer-events-none) apply.
    if (!input.hasAttribute ('inert'))
      throw new Error ('inert property did not reflect the inert attribute');
    // The input must not be focused initially.
    if (input === document.activeElement)
      throw new Error ('input should not be focused initially');
  } finally {
    ed.cleanup();
  }
  return true;
}
sub_tests.push (['initial_value', test_editable_initial_value]);

/// Test that a numeric value is coerced to a string for display.
async function test_editable_numeric_value (): Promise<boolean>
{
  const ed = mount_editable ({ value: 123, clicks: 1 });
  try {
    await Dom.ui_next_frame();
    const input = ed.input();
    if (!input) throw new Error ('Editable input not rendered');
    if (input.value !== '123')
      throw new Error (`numeric value not coerced: "${input.value}"`);
  } finally {
    ed.cleanup();
  }
  return true;
}
sub_tests.push (['numeric_value', test_editable_numeric_value]);

/// Test that a single click activates editing (clicks=1).
async function test_editable_activate_click (): Promise<boolean>
{
  const ed = mount_editable ({ value: 'name', clicks: 1 });
  try {
    await Dom.ui_next_frame();
    const span = ed.span();
    const input = ed.input();
    if (!span || !input) throw new Error ('Editable not rendered');
    span.click();
    await Dom.ui_next_frame();
    if (input.inert)
      throw new Error ('input should not be inert after click activation');
    if (input !== document.activeElement)
      throw new Error ('input should be focused after click activation');
  } finally {
    ed.cleanup();
  }
  return true;
}
sub_tests.push (['activate_click', test_editable_activate_click]);

/// Test that a double click activates editing (clicks=2) while single click does not.
async function test_editable_activate_dblclick (): Promise<boolean>
{
  const ed = mount_editable ({ value: 'name', clicks: 2 });
  try {
    await Dom.ui_next_frame();
    const span = ed.span();
    const input = ed.input();
    if (!span || !input) throw new Error ('Editable not rendered');
    // Single click must not activate when clicks=2.
    span.click();
    await Dom.ui_next_frame();
    if (!input.inert)
      throw new Error ('single click wrongly activated editing at clicks=2');
    // Double click must activate.
    span.dispatchEvent (new MouseEvent ('dblclick', { bubbles: true, cancelable: true }));
    await Dom.ui_next_frame();
    if (input.inert)
      throw new Error ('double click failed to activate editing at clicks=2');
    if (input !== document.activeElement)
      throw new Error ('input should be focused after dblclick activation');
  } finally {
    ed.cleanup();
  }
  return true;
}
sub_tests.push (['activate_dblclick', test_editable_activate_dblclick]);

/// Test that pressing Enter commits the edit and emits the change event.
async function test_editable_enter_commits (): Promise<boolean>
{
  let changed_value: string | undefined = undefined;
  const ed = mount_editable ({ value: 'old', clicks: 1, onChange: e => { changed_value = e.detail.value; } });
  try {
    await Dom.ui_next_frame();
    const span = ed.span();
    const input = ed.input();
    if (!span || !input) throw new Error ('Editable not rendered');
    span.click();
    await Dom.ui_next_frame();
    input.value = 'new';
    press_key (input, 'Enter');
    await Dom.ui_next_frame();
    if (changed_value !== 'new')
      throw new Error (`Enter did not commit change event, got: ${changed_value}`);
    if (!input.inert)
      throw new Error ('input should be inert after Enter');
    if (input.value !== 'old')
      throw new Error (`value should revert to prop after Enter, got: "${input.value}"`);
  } finally {
    ed.cleanup();
  }
  return true;
}
sub_tests.push (['enter_commits', test_editable_enter_commits]);

/// Test that pressing Escape cancels the edit (no change event, value reverted).
async function test_editable_escape_cancels (): Promise<boolean>
{
  let change_count = 0;
  const ed = mount_editable ({ value: 'keep', clicks: 1, onChange: () => { change_count++; } });
  try {
    await Dom.ui_next_frame();
    const span = ed.span();
    const input = ed.input();
    if (!span || !input) throw new Error ('Editable not rendered');
    span.click();
    await Dom.ui_next_frame();
    input.value = 'discard';
    press_key (input, 'Escape');
    await Dom.ui_next_frame();
    if (change_count !== 0)
      throw new Error ('Escape must not emit a change event');
    if (!input.inert)
      throw new Error ('input should be inert after Escape');
    if (input.value !== 'keep')
      throw new Error (`value should revert after Escape, got: "${input.value}"`);
  } finally {
    ed.cleanup();
  }
  return true;
}
sub_tests.push (['escape_cancels', test_editable_escape_cancels]);

/// Test that blurring the input (clicking away) commits the edit.
async function test_editable_blur_commits (): Promise<boolean>
{
  let changed_value: string | undefined = undefined;
  const ed = mount_editable ({ value: 'orig', clicks: 1, onChange: e => { changed_value = e.detail.value; } });
  try {
    await Dom.ui_next_frame();
    const span = ed.span();
    const input = ed.input();
    if (!span || !input) throw new Error ('Editable not rendered');
    span.click();
    await Dom.ui_next_frame();
    input.value = 'edited';
    // Headless Electron windows are unfocused, so real blur events don't fire;
    // dispatch a synthetic blur event to exercise the commit handler directly.
    input.dispatchEvent (new FocusEvent ('blur'));
    await Dom.ui_next_frame();
    if (changed_value !== 'edited')
      throw new Error (`blur did not commit change event, got: ${changed_value}`);
    if (!input.inert)
      throw new Error ('input should be inert after blur');
    if (input.value !== 'orig')
      throw new Error (`value should revert to prop after blur, got: "${input.value}"`);
  } finally {
    ed.cleanup();
  }
  return true;
}
sub_tests.push (['blur_commits', test_editable_blur_commits]);

/// Test that the ref callback receives the input element and activate() works.
async function test_editable_ref_activate (): Promise<boolean>
{
  const ed = mount_editable ({ value: 'x', clicks: 2 });
  try {
    await Dom.ui_next_frame();
    const input = ed.input();
    const ref = ed.input_ref();
    if (!input || !ref) throw new Error ('ref was not populated with the input element');
    if (input !== ref) throw new Error ('ref element does not match rendered input');
    if (typeof (ref as any).activate !== 'function')
      throw new Error ('activate() method not attached to ref element');
    (ref as any).activate();
    await Dom.ui_next_frame();
    if (input.inert)
      throw new Error ('activate() via ref failed to enable editing');
    if (input !== document.activeElement)
      throw new Error ('activate() via ref failed to focus the input');
  } finally {
    ed.cleanup();
  }
  return true;
}
sub_tests.push (['ref_activate', test_editable_ref_activate]);

// == Master runner ==
/// Single exported entry point runs all sub-tests in sequence.
export async function test_editable (): Promise<boolean>
{
  if (!sub_tests.length)
    throw new Error ('editable: no sub-tests registered');
  const failures: string[] = [];
  for (const [name, fn] of sub_tests) {
    try {
      await fn();
    } catch (e) {
      failures.push (`${name}: ${(e as Error)?.message ?? e}`);
    }
  }
  if (failures.length)
    throw new Error ('editable failures:\n  ' + failures.join ('\n  '));
  return true;
}
