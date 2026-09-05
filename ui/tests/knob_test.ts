// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL-2.0

import { createComponent, render } from 'solid-js/web';
import { Knob } from '../b/knob';
import * as Dom from '../dom';

/// Minimal fake knob prop with normalized value 0..1.
function make_fake_prop (value = 0.5)
{
  let val = value;
  const listeners: (() => void)[] = [];
  return {
    label_: undefined as any,
    nick_: undefined as any,
    hints_: '',
    get_text: async () => String (val),
    fetch_: () => val,
    get_normalized: async () => val,
    set_normalized: async (v: number) => { val = v; listeners.forEach (l => l()); },
    update_: async () => {},
    reset: async () => { val = 0; },
    on: (_ev: string, cb: () => void) => {
      listeners.push (cb);
      return () => {
        const i = listeners.indexOf (cb);
        if (i >= 0) listeners.splice (i, 1);
      };
    },
  };
}

/// Spy on Shell.data_bubble.force/unforce, restoring the previous handlers.
function stub_shell ()
{
  const force_calls: any[] = [];
  const unforce_calls: any[] = [];
  const shell: any = (globalThis as any).Shell;
  if (!shell?.data_bubble)
    throw new Error ('Shell.data_bubble not available in test environment');
  const db = shell.data_bubble;
  const prev_force = db.force.bind (db);
  const prev_unforce = db.unforce.bind (db);
  db.force = (el: any) => { force_calls.push (el); prev_force (el); };
  db.unforce = (el: any) => { unforce_calls.push (el); prev_unforce (el); };
  return {
    force_calls,
    unforce_calls,
    restore: () => {
      db.force = prev_force;
      db.unforce = prev_unforce;
    },
  };
}

/// Mount a Knob and return its DOM helpers.
function mount_knob (prop: any)
{
  const container = document.createElement ('div');
  document.body.appendChild (container);
  const dispose = render (() => createComponent (Knob, { prop }), container);
  const sprite = () => container.querySelector ('#sprite') as HTMLElement | null;
  // Avoid environment failures for pointer-lock machinery in headless mode.
  const prep_sprite = () => {
    const el = sprite();
    if (!el) throw new Error ('knob #sprite not found');
    (el as any).setPointerCapture = () => {};
    (el as any).releasePointerCapture = () => {};
    (el as any).requestPointerLock = () => {};
    return el;
  };
  return {
    container,
    sprite: prep_sprite,
    cleanup: () => {
      dispose();
      container.remove();
    },
  };
}

// == Test registry ==
const sub_tests: [string, () => Promise<any>][] = [];

/// Test that spin-drag state and data bubbles attach to the sprite, not the wrapper.
async function test_knob_spindrag_element (): Promise<boolean>
{
  const shell = stub_shell();
  const prop = make_fake_prop (0.5);
  const knob = mount_knob (prop);

  try {
    await Dom.ui_next_frame();
    const sprite = knob.sprite();

    // Primary-button pointerdown starts a drag on the sprite
    sprite.dispatchEvent (new PointerEvent ('pointerdown', {
      buttons: 1, pointerId: 1, bubbles: true, cancelable: true,
    }));

    if (shell.force_calls.length != 1)
      throw new Error (`data_bubble.force not called exactly once: ${shell.force_calls.length}`);
    if (shell.force_calls[0] !== sprite)
      throw new Error ('data_bubble.force targeted the wrapper instead of the sprite');

    // Ending the drag with pointerup must stop it on the sprite
    sprite.dispatchEvent (new PointerEvent ('pointerup', { bubbles: true, cancelable: true }));

    if (shell.unforce_calls.length != 1)
      throw new Error (`data_bubble.unforce not called exactly once: ${shell.unforce_calls.length}`);
    if (shell.unforce_calls[0] !== sprite)
      throw new Error ('data_bubble.unforce targeted the wrapper instead of the sprite');
  } finally {
    knob.cleanup();
    shell.restore();
  }

  return true;
}
sub_tests.push (['spindrag_element', test_knob_spindrag_element]);

/// Test that a drag stopped via the double-click path ends cleanly on the sprite.
async function test_knob_spindrag_dblclick (): Promise<boolean>
{
  const shell = stub_shell();
  const prop = make_fake_prop (0.5);
  const knob = mount_knob (prop);

  try {
    await Dom.ui_next_frame();
    const sprite = knob.sprite();

    // Start a drag
    sprite.dispatchEvent (new PointerEvent ('pointerdown', {
      buttons: 1, pointerId: 1, bubbles: true, cancelable: true,
    }));
    if (shell.force_calls.length != 1)
      throw new Error ('drag did not start');

    // A second pointerdown within the double-click window resets the value and stops the drag
    sprite.dispatchEvent (new PointerEvent ('pointerdown', {
      buttons: 1, pointerId: 1, bubbles: true, cancelable: true,
    }));

    if (shell.unforce_calls.length != 1)
      throw new Error (`double-click did not stop the drag: ${shell.unforce_calls.length} unforce calls`);
    if (shell.unforce_calls[0] !== sprite)
      throw new Error ('double-click stop targeted the wrapper instead of the sprite');

    // After stopping, a new drag must be possible (drag state was cleared)
    sprite.dispatchEvent (new PointerEvent ('pointerdown', {
      buttons: 1, pointerId: 2, bubbles: true, cancelable: true,
    }));
    if (shell.force_calls.length < 2)
      throw new Error (`drag state not cleared after double-click: ${shell.force_calls.length} force calls`);
    sprite.dispatchEvent (new PointerEvent ('pointerup', { bubbles: true, cancelable: true }));
  } finally {
    knob.cleanup();
    shell.restore();
  }

  return true;
}
sub_tests.push (['spindrag_dblclick', test_knob_spindrag_dblclick]);

/// Test that the wheel guard consumes only enabled scroll directions when not dragging.
async function test_knob_wheel_guard (): Promise<boolean>
{
  const shell = stub_shell();
  const prop = make_fake_prop (0.5);
  const knob = mount_knob (prop);

  try {
    await Dom.ui_next_frame();
    const sprite = knob.sprite();

    // Vertical wheel is enabled by default (vscroll) and must change the value
    sprite.dispatchEvent (new WheelEvent ('wheel', {
      deltaX: 0, deltaY: -100, bubbles: true, cancelable: true,
    }));
    await Dom.ui_wait (50); // debounced commit
    const v = await prop.get_normalized();
    if (!(v > 0.5))
      throw new Error (`vertical wheel did not increase value: ${v}`);

    // Horizontal wheel is disabled by default (hscroll=false) and must be ignored
    sprite.dispatchEvent (new WheelEvent ('wheel', {
      deltaX: -100, deltaY: 0, bubbles: true, cancelable: true,
    }));
    await Dom.ui_wait (50);
    const v2 = await prop.get_normalized();
    if (v2 != v)
      throw new Error (`horizontal wheel changed value while disabled: ${v2}`);
  } finally {
    knob.cleanup();
    shell.restore();
  }

  return true;
}
sub_tests.push (['wheel_guard', test_knob_wheel_guard]);

// == Master runner ==
/// Single exported entry point runs all sub-tests in sequence.
export async function test_knob (): Promise<boolean>
{
  if (!sub_tests.length)
    throw new Error ('knob: no sub-tests registered');
  const failures: string[] = [];
  for (const [name, fn] of sub_tests) {
    try {
      await fn();
    } catch (e) {
      failures.push (`${name}: ${(e as Error)?.message ?? e}`);
    }
  }
  if (failures.length)
    throw new Error ('knob failures:\n  ' + failures.join ('\n  '));
  return true;
}
