// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { createComponent, render } from 'solid-js/web';
import { createSignal } from 'solid-js';
import { TextInput } from '../b/textinput';
import * as Dom from '../dom';
import { TestTimers } from './timers';
import { Signal } from '../signal';

function make_prop (init: { value?: string; metadata?: string[] } = {})
{
  const [value, set_value] = createSignal (init.value ?? '');
  return {
    metadata: init.metadata ?? [],
    get value () { return value(); },
    set value (v: string) { set_value (v); },
    apply_: (v: string) => { set_value (v); },
    notify_: () => set_value (value()),
  };
}

/// Mount a TextInput for testing and return helpers.
function mount_textinput (props: {
  prop?: any;
  value?: string | (() => string);
  placeholder?: string;
  readonly?: boolean;
  disabled?: boolean;
  label?: string;
  title?: string;
  class?: string;
  'on:valuechange'?: (e: Event) => void;
  [key: string]: any;
})
{
  const container = document.createElement ('div');
  document.body.appendChild (container);

  const dispose = render (() => createComponent (TextInput, props as any), container);

  const root = () => container.querySelector ('div.b-textinput') as HTMLDivElement | null;
  const input = () => container.querySelector ('input[type=text]') as HTMLInputElement | null;
  const cleanup = () => {
    dispose();
    container.remove();
  };
  return { container, root, input, cleanup };
}

/// Dispatch an `input` event on an element carrying a fresh `.value`.
function send_input (el: HTMLInputElement, value: string)
{
  el.value = value;
  el.dispatchEvent (new Event ('input', { bubbles: true, cancelable: true }));
}

// == Test registry ==
const sub_tests: [string, () => Promise<any>][] = [];

/// Test that the initial value comes from the `value` prop (no prop supplied).
async function test_textinput_initial_value (): Promise<boolean>
{
  const ti = mount_textinput ({
    value: 'hello',
    'on:valuechange': () => { throw new Error ('initial value should not emit'); },
  });
  try {
    await Dom.ui_next_frame();
    const inp = ti.input();
    if (!inp) throw new Error ('TextInput field not rendered');
    if (inp.value !== 'hello')
      throw new Error (`initial value wrong: "${inp.value}"`);
  } finally {
    ti.cleanup();
  }
  return true;
}
sub_tests.push (['initial_value', test_textinput_initial_value]);

/// Test that the initial value comes from the backend property.
async function test_textinput_initial_from_prop (): Promise<boolean>
{
  const prop = make_prop ({ value: 'via-prop' });
  const ti = mount_textinput ({ prop });
  try {
    await Dom.ui_next_frame();
    const inp = ti.input();
    if (!inp) throw new Error ('TextInput field not rendered');
    if (inp.value !== 'via-prop')
      throw new Error (`initial value from prop wrong: "${inp.value}"`);
  } finally {
    ti.cleanup();
  }
  return true;
}
sub_tests.push (['initial_from_prop', test_textinput_initial_from_prop]);

/// Test that typing forwards the value to `prop.apply_()` and emits
/// `valuechange` with the correct `event.target.value`, only on actual change.
async function test_textinput_input_emits_valuechange (): Promise<boolean>
{
  let apply_count = 0;
  const applied: string[] = [];
  const emitted: any[] = [];
  let emit_count = 0;
  const prop = make_prop ({ value: 'foo' });
  prop.apply_ = (v: string) => { applied.push (v); apply_count++; };
  const ti = mount_textinput ({
    prop,
    'on:valuechange': e => { emitted.push ((e.target as any).value); emit_count++; },
  });
  try {
    await Dom.ui_next_frame();
    const inp = ti.input();
    if (!inp) throw new Error ('TextInput field not rendered');
    // typing a different value → emits + applies
    send_input (inp, 'bar');
    if (applied[0] !== 'bar')
      throw new Error (`apply_ not called with 'bar': ${applied[0]}`);
    if (emitted[0] !== 'bar')
      throw new Error (`valuechange payload wrong: ${emitted[0]}`);
    if (emit_count !== 1)
      throw new Error (`first edit emitted ${emit_count} events`);
    // typing the same value again → must NOT emit (and not apply either)
    const emit_before = emit_count, apply_before = apply_count;
    send_input (inp, 'bar');
    if (emit_count !== emit_before)
      throw new Error (`unchanged value emitted ${emit_count - emit_before} events`);
    if (apply_count !== apply_before)
      throw new Error (`unchanged value called apply_ ${apply_count - apply_before} times`);
    // typing a different value again → emits + applies
    send_input (inp, 'baz');
    if (emit_count !== emit_before + 1)
      throw new Error (`changed value did not emit`);
    if (emitted[1] !== 'baz')
      throw new Error (`second valuechange payload wrong: ${emitted[1]}`);
  } finally {
    ti.cleanup();
  }
  return true;
}
sub_tests.push (['input_emits_valuechange', test_textinput_input_emits_valuechange]);

/// Backend updates refresh the field.
async function test_textinput_external_notify (): Promise<boolean>
{
  const prop = make_prop ({ value: 'initial' });
  const ti = mount_textinput ({ prop });
  try {
    await Dom.ui_next_frame();
    const inp = ti.input();
    if (!inp) throw new Error ('TextInput field not rendered');
    // Simulate an outside value change (undo/redo, reset button, preset load).
    prop.value = 'changed-by-backend';
    prop.notify_();
    await Dom.ui_next_frame();
    if (inp.value !== 'changed-by-backend')
      throw new Error (`display not refreshed by notify: "${inp.value}"`);
  } finally {
    ti.cleanup();
  }
  return true;
}
sub_tests.push (['external_notify', test_textinput_external_notify]);

/// Test that the file-picker click handler is guarded against readonly/disabled
/// inputs, so a readonly field cannot be mutated through the file dialog.
async function test_textinput_readonly_blocks_file_dialog (): Promise<boolean>
{
  let orig_select = Shell.select_file;
  let calls = 0;
  let applied: string | undefined = undefined;
  let emitted: any = undefined;
  Shell.select_file = async () => { calls++; return '/fake/path'; };
  const prop = make_prop ({ value: 'vf', metadata: ['extensions=wav'] });
  prop.apply_ = (v: string) => { applied = v; };
  try {
    // readonly path
    const ti = mount_textinput ({
      prop, readonly: true,
      'on:valuechange': e => { emitted = (e.target as any).value; },
    });
    try {
      await Dom.ui_next_frame();
      ti.input()!.click();
      await Dom.ui_next_frame();
      await Dom.ui_next_frame();
      if (calls !== 0) throw new Error ('readonly opened the file dialog');
      if (applied !== undefined) throw new Error ('readonly applied a value');
      if (emitted !== undefined) throw new Error ('readonly emitted valuechange');
    } finally {
      ti.cleanup();
    }

    // disabled path
    const ti2 = mount_textinput ({
      prop, disabled: true,
      'on:valuechange': () => { calls++; },
    });
    try {
      await Dom.ui_next_frame();
      ti2.input()!.click();
      await Dom.ui_next_frame();
      await Dom.ui_next_frame();
      if (calls !== 0) throw new Error ('disabled opened the file dialog');
    } finally {
      ti2.cleanup();
    }
  } finally {
    Shell.select_file = orig_select;
  }
  return true;
}
sub_tests.push (['readonly_blocks_file_dialog', test_textinput_readonly_blocks_file_dialog]);

/// Test that the file-picker click handler assigns the picked filename to prop
/// and that the notify roundtrip refreshes the displayed text.
async function test_textinput_file_picker (): Promise<boolean>
{
  let orig_select = Shell.select_file;
  Shell.select_file = async () => '/some/file.wav';
  const prop = make_prop ({ value: '', metadata: ['extensions=wav'] });
  const ti = mount_textinput ({ prop });
  try {
    await Dom.ui_next_frame();
    const inp = ti.input();
    if (!inp) throw new Error ('TextInput field not rendered');
    inp.click();
    await Dom.ui_next_frame();
    await Dom.ui_next_frame();
    if (prop.value !== '/some/file.wav')
      throw new Error (`file not assigned to prop: ${prop.value}`);
    prop.notify_();                 // emulate the notify roundtrip
    await Dom.ui_next_frame();
    if (inp.value !== '/some/file.wav')
      throw new Error (`field not refreshed after file pick: "${inp.value}"`);
  } finally {
    Shell.select_file = orig_select;
    ti.cleanup();
  }
  return true;
}
sub_tests.push (['file_picker', test_textinput_file_picker]);

/// Test that the `class` prop is merged onto the root `b-textinput` element.
async function test_textinput_class_forwarding (): Promise<boolean>
{
  const ti = mount_textinput ({ value: 'x', class: 'b-objecteditor--ident' });
  try {
    await Dom.ui_next_frame();
    const root = ti.root();
    if (!root) throw new Error ('TextInput root not rendered');
    if (!root.classList.contains ('b-textinput'))
      throw new Error ('root missing b-textinput class');
    if (!root.classList.contains ('b-objecteditor--ident'))
      throw new Error ('root class not forwarded: ' + root.className);
  } finally {
    ti.cleanup();
  }
  return true;
}
sub_tests.push (['class_forwarding', test_textinput_class_forwarding]);

/// Test that the `title` prop is forwarded onto the root element as a native tooltip.
async function test_textinput_title_forwarding (): Promise<boolean>
{
  const ti = mount_textinput ({ value: 'x', title: 'Hover Tip' });
  try {
    await Dom.ui_next_frame();
    const root = ti.root();
    if (!root) throw new Error ('TextInput root not rendered');
    if (root.getAttribute ('title') !== 'Hover Tip')
      throw new Error (`title not forwarded: "${root.getAttribute ('title')}"`);
  } finally {
    ti.cleanup();
  }
  return true;
}
sub_tests.push (['title_forwarding', test_textinput_title_forwarding]);

/// Test that a parent-supplied `value` change (no prop) updates the display.
async function test_textinput_value_signal_update (): Promise<boolean>
{
  const [get_value, set_value] = createSignal<string> ('a');
  const ti = mount_textinput ({
    get value () { return get_value(); },
  });
  try {
    await Dom.ui_next_frame();
    const inp = ti.input();
    if (!inp) throw new Error ('TextInput field not rendered');
    if ((inp.value as string) !== 'a') throw new Error (`initial value wrong: "${inp.value}"`);
    set_value ('b');
    await Dom.ui_next_frame();
    if ((inp.value as string) !== 'b') throw new Error (`display not updated: "${inp.value}"`);
  } finally {
    ti.cleanup();
  }
  return true;
}
sub_tests.push (['value_signal_update', test_textinput_value_signal_update]);

async function test_textinput_backend_correction (): Promise<boolean>
{
  const prop = make_prop ({ value: 'old' });
  const edits: string[] = [];
  prop.apply_ = value => { edits.push (value); };
  const ti = mount_textinput ({ prop });
  const timers = new TestTimers();
  try {
    const input = ti.input()!;
    send_input (input, 'new');
    if (input.value !== 'new')
      throw new Error ('text edit did not stay visible');
    send_input (input, 'old');
    if (edits.join (',') !== 'new,old')
      throw new Error ('quick text change back was not sent');
    send_input (input, 'new');
    prop.notify_();
    await Dom.ui_next_frame();
    if (String (input.value) !== 'new')
      throw new Error ('backend value interrupted the edit');
    timers.run();
    if (String (input.value) !== 'old' || edits.length !== 3)
      throw new Error ('backend correction was lost or sent as an edit');
  } finally {
    ti.cleanup();
    timers.restore();
  }
  return true;
}
sub_tests.push (['backend_correction', test_textinput_backend_correction]);

async function test_textinput_local_grace (): Promise<boolean>
{
  const value = new (Signal.State as any) ('backend');
  const edits: string[] = [];
  const props = {
    get value () { return value.get(); },
    'on:valuechange': e => edits.push (e.target.value),
  };
  const first = mount_textinput (props);
  const second = mount_textinput (props);
  const timers = new TestTimers();
  try {
    send_input (first.input()!, 'edit');
    if (value.get() !== 'backend' || second.input()!.value !== 'backend')
      throw new Error ('optimistic text escaped the edited input');
    value.set ('remote');
    await Dom.ui_next_frame();
    if (first.input()!.value !== 'edit' || second.input()!.value !== 'remote')
      throw new Error ('backend update did not stay separate from the edit');
    send_input (first.input()!, 'newer');
    if (timers.pending !== 1)
      throw new Error ('text edit did not replace its timer');
    timers.run();
    if (first.input()!.value !== 'remote' || edits.join (',') !== 'edit,newer')
      throw new Error ('text input did not settle on the latest backend value');
    send_input (first.input()!, 'ignored');
    timers.run();
    if (first.input()!.value !== 'remote')
      throw new Error ('text input kept an edit without a notification');
    send_input (first.input()!, 'dispose');
    first.cleanup();
    if (timers.pending)
      throw new Error ('text input kept a timer after disposal');
  } finally {
    first.cleanup();
    second.cleanup();
    timers.restore();
  }
  return true;
}
sub_tests.push (['local_grace', test_textinput_local_grace]);

// == Master runner ==
/// Single exported entry point runs all sub-tests in sequence.
export async function test_textinput (): Promise<boolean>
{
  if (!sub_tests.length)
    throw new Error ('textinput: no sub-tests registered');
  const failures: string[] = [];
  for (const [name, fn] of sub_tests) {
    try {
      await fn();
    } catch (e) {
      failures.push (`${name}: ${(e as Error)?.message ?? e}`);
    }
  }
  if (failures.length)
    throw new Error ('textinput failures:\n  ' + failures.join ('\n  '));
  return true;
}
