// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { createComponent, render } from 'solid-js/web';
import { createSignal } from 'solid-js';
import { DeviceEditor } from '../b/deviceeditor';
import * as Dom from '../dom';

function mock_device (name: string)
{
  return {
    name,
    device_info: () => Promise.resolve ({ name }),
    access_properties: () => Promise.resolve ([]),
    remove_self: () => {},
  };
}

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

async function wait_for_name (name: () => string, expected: string, timeout_ms = 5000): Promise<void>
{
  const deadline = Date.now () + timeout_ms;
  while (name() !== expected) {
    if (Date.now () >= deadline)
      throw new Error (`device name did not become "${expected}" within ${timeout_ms}ms: "${name()}"`);
    await Dom.ui_wait (20);
  }
}

// == Test registry ==
const sub_tests: [string, () => Promise<any>][] = [];

async function test_deviceeditor_renders_name (): Promise<boolean>
{
  const dev = mock_device ('AlphaDev');
  const { root, name, cleanup } = mount_deviceeditor (() => dev);
  try {
    await Dom.ui_next_frame();
    const r = root();
    if (!r) throw new Error ('DeviceEditor root not rendered');
    await wait_for_name (name, 'AlphaDev');
  } finally {
    cleanup();
  }
  return true;
}
sub_tests.push (['renders_name', test_deviceeditor_renders_name]);

async function test_deviceeditor_refetch_on_prop_change (): Promise<boolean>
{
  const [device, set_device] = createSignal (mock_device ('FirstDev'));
  const { name, cleanup } = mount_deviceeditor (device);
  try {
    await wait_for_name (name, 'FirstDev');

    set_device (mock_device ('SecondDev'));
    await wait_for_name (name, 'SecondDev');
  } finally {
    cleanup();
  }
  return true;
}
sub_tests.push (['refetch_on_prop_change', test_deviceeditor_refetch_on_prop_change]);

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
