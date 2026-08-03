// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { createComponent, render } from 'solid-js/web';
import { createSignal } from 'solid-js';
import { DeviceEditor } from '../b/deviceeditor';
import * as Dom from '../dom';

/// Build a minimal mock device. The two async methods the editor uses are
/// `device_info()` (resolved to an object with a `name`) and
/// `access_properties()` (resolved to an empty list keeps the editor's
/// property-groups path trivial). `remove_self` is a no-op.
function mock_device (name: string)
{
  return {
    name,
    device_info: () => Promise.resolve ({ name }),
    access_properties: () => Promise.resolve ([]),
    remove_self: () => { /* no-op */ },
  };
}

/// Mount a DeviceEditor with a reactive `device` prop (a getter) so the
/// device-prop effect re-runs when the signal changes.
function mount_deviceeditor (get_device: () => any)
{
  const container = document.createElement ('div');
  document.body.appendChild (container);
  const dispose = render (() => createComponent (DeviceEditor, {
    get device () { return get_device(); },
  } as any), container);
  const root = () => container.querySelector ('div.b-deviceeditor') as HTMLElement | null;
  const name = () => container.querySelector ('span.b-deviceeditor-sw')?.textContent ?? '';
  const cleanup = () => { dispose(); container.remove(); };
  return { root, name, cleanup };
}

// == Test registry ==
const sub_tests: [string, () => Promise<any>][] = [];

/// Smoke test: the editor renders the device name once the async fetch settles.
/// The getter must return a STABLE object so the editor's `device_ !== props.device`
/// guard doesn't discard the fetched results.
async function test_deviceeditor_renders_name (): Promise<boolean>
{
  const dev = mock_device ('AlphaDev');
  const { root, name, cleanup } = mount_deviceeditor (() => dev);
  try {
    await Dom.ui_next_frame();
    const r = root();
    if (!r) throw new Error ('DeviceEditor root not rendered');
    // allow the async fetch_device to settle
    await Dom.ui_wait (50);
    await Dom.ui_next_frame();
    if (name() !== 'AlphaDev')
      throw new Error (`device name not rendered: "${name()}"`);
  } finally {
    cleanup();
  }
  return true;
}
sub_tests.push (['renders_name', test_deviceeditor_renders_name]);

/// Regression test for M10: after the `device` prop changes, the editor must
/// re-fetch and render the *new* device's info. The original shared `disposed`
/// flag was set to true by effect cleanup and never reset, so the second
/// device's async results were silently discarded and the name never updated.
async function test_deviceeditor_refetch_on_prop_change (): Promise<boolean>
{
  const [device, set_device] = createSignal (mock_device ('FirstDev'));
  const { name, cleanup } = mount_deviceeditor (device);
  try {
    await Dom.ui_wait (50);
    await Dom.ui_next_frame();
    if (name() !== 'FirstDev')
      throw new Error (`initial name not rendered: "${name()}"`);

    // Switch to a second device — must re-fetch and render its name.
    set_device (mock_device ('SecondDev')); // stable: signal stores the object
    await Dom.ui_wait (80);
    await Dom.ui_next_frame();
    if (name() !== 'SecondDev')
      throw new Error (`name not updated after prop change (M10 regression): "${name()}"`);
  } finally {
    cleanup();
  }
  return true;
}
sub_tests.push (['refetch_on_prop_change', test_deviceeditor_refetch_on_prop_change]);

/// The editor must tolerate a missing/null device prop without throwing.
async function test_deviceeditor_no_device (): Promise<boolean>
{
  const { root, cleanup } = mount_deviceeditor (() => null);
  try {
    await Dom.ui_next_frame();
    const r = root();
    if (!r) throw new Error ('DeviceEditor root not rendered without device');
  } finally {
    cleanup();
  }
  return true;
}
sub_tests.push (['no_device', test_deviceeditor_no_device]);

// == Master runner ==
/// Single exported entry point runs all sub-tests in sequence.
export async function test_deviceeditor (): Promise<boolean>
{
  if (!sub_tests.length)
    throw new Error ('deviceeditor: no sub-tests registered');
  const failures: string[] = [];
  for (const [name, fn] of sub_tests) {
    try {
      await fn();
    } catch (e) {
      failures.push (`${name}: ${(e as Error)?.message ?? e}`);
    }
  }
  if (failures.length)
    throw new Error ('deviceeditor failures:\n  ' + failures.join ('\n  '));
  return true;
}
