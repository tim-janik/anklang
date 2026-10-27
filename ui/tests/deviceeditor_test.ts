// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { run_tests } from './runner.js';
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

async function test_deviceeditor_layout (): Promise<boolean>
{
  for (const [count, main_span, other_row, other_col] of [
    [1, 2, 3, 1], [6, 2, 3, 1], [7, 3, 4, 1], [12, 3, 4, 1],
    [13, 5, 1, 2], [18, 5, 1, 2], [19, 5, 1, 2], [24, 5, 1, 2],
    [64, 5, 1, 2], [65, 5, 3, 2], [128, 5, 1, 3], [129, 5, 3, 3],
  ]) {
    const properties = Array.from ({ length: count + 1 }, (_, i) => ({
      update_: async () => {}, fetch_: () => 0,
      group_: i < count ? 'Main' : 'Other', hints_: ':G:', nick_: String (i),
    }));
    const device = { ...mock_device ('Layout'), access_properties: async () => properties };
    const container = document.createElement ('div');
    document.body.appendChild (container);
    const dispose = render (() => createComponent (DeviceEditor, { device }), container);
    try {
      await Dom.ui_next_frame();
      const groups = [...container.querySelectorAll<HTMLElement> ('.b-propgroup')];
      const pages = Math.ceil (count / 64);
      const names = count <= 64 ? ['Main', 'Other'] : [
        ...Array.from ({ length: pages }, (_, i) => `Main Page ${i + 1}`), 'Other',
      ];
      if (groups.map (g => g.querySelector ('.b-propgroup-title')!.textContent!.trim()).join (',') !== names.join (','))
        throw new Error (`${count}: property groups or pages changed`);
      if (groups[0].style.gridRow !== `1 / span ${main_span}` || groups[0].style.gridColumnStart !== '1')
        throw new Error (`${count}: first property group moved or changed height`);
      if (groups.at (-1)!.style.gridRowStart !== String (other_row) || groups.at (-1)!.style.gridColumnStart !== String (other_col))
        throw new Error (`${count}: following property group moved`);
      if (container.querySelectorAll ('.b-propgroup-row > span.text-center').length !== count + 1)
        throw new Error (`${count}: layout lost properties`);
    } finally {
      dispose();
      container.remove();
    }
  }
  return true;
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
sub_tests.push (['layout', test_deviceeditor_layout]);

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

export async function test_deviceeditor (): Promise<boolean>
{
  return run_tests ('deviceeditor', sub_tests);
}
