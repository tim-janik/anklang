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
    if (name() !== 'AlphaDev')
      throw new Error (`device name not rendered: "${name()}"`);
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
    await Dom.ui_next_frame();
    if (name() !== 'FirstDev')
      throw new Error (`initial name not rendered: "${name()}"`);

    set_device (mock_device ('SecondDev'));
    await Dom.ui_next_frame();
    if (name() !== 'SecondDev')
      throw new Error (`name not updated after prop change: "${name()}"`);
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

async function test_deviceeditor_replaces_pending_device (): Promise<boolean>
{
  let finish: (value: any[]) => void;
  const properties = new Promise<any[]> (resolve => { finish = resolve; });
  const first = { ...mock_device ('Slow device'), access_properties: () => properties };
  const [device, set_device] = createSignal (first);
  const { name, cleanup } = mount_deviceeditor (device);
  try {
    set_device (mock_device ('Current device'));
    await Dom.ui_next_frame();
    finish ([]);
    await Dom.ui_next_frame();
    if (name() !== 'Current device')
      throw new Error ('an old device load replaced the current editor');
  } finally {
    cleanup();
  }
  return true;
}
sub_tests.push (['replaces_pending_device', test_deviceeditor_replaces_pending_device]);

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
