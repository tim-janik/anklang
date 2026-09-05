// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { createComponent, render } from 'solid-js/web';
import { DevicePanel } from '../b/devicepanel';
import * as Dom from '../dom';

/// Minimal fake track that returns a device with a fixed list of device types.
function make_fake_track (device_types: { uri: string; name: string; category: string }[])
{
  return {
    access_device: async () => ({
      list_device_types: async () => device_types,
    }),
  };
}

/// Mount a DevicePanel with a fake track and return cleanup helpers.
function mount_panel (device_types: { uri: string; name: string; category: string }[])
{
  const container = document.createElement ('div');
  document.body.appendChild (container);

  const dispose = render (() => createComponent (DevicePanel, {
    track: make_fake_track (device_types),
  }), container);

  return {
    container,
    dispose,
    cleanup: () => {
      dispose();
      container.remove();
    },
  };
}

async function wait_for_device_buttons (menu: Element, expected: string[], timeout_ms = 5000): Promise<HTMLButtonElement[]>
{
  const deadline = Date.now () + timeout_ms;
  let buttons: HTMLButtonElement[] = [];
  do {
    buttons = Array.from (menu.querySelectorAll ('button'));
    const labels = buttons.map (button => button.textContent?.trim() ?? '');
    if (expected.every (label => labels.includes (label)))
      return buttons;
    await Dom.ui_wait (50);
  } while (Date.now () < deadline);
  const labels = buttons.map (button => button.textContent?.trim() ?? '');
  throw new Error (`DevicePanel device-type buttons did not render within ${timeout_ms}ms: ${labels.join (', ')}`);
}

// == Test registry ==
const sub_tests: [string, () => Promise<any>][] = [];

/// Test that the device-type popup menu renders the tree with device-type buttons.
async function test_devicepanel_popup_menutypes (): Promise<boolean>
{
  const panel = mount_panel ([
    { uri: 'ase:synth', name: 'Synth', category: 'Instruments' },
    { uri: 'ase:fx', name: 'FX', category: 'Effects' },
  ]);

  try {
    await Dom.ui_next_frame();

    // Open the add-device menu by clicking the "more" button
    const more = panel.container.querySelector ('.b-more');
    if (!more)
      throw new Error ('DevicePanel .b-more button not found');
    more.dispatchEvent (new MouseEvent ('mousedown', { bubbles: true, cancelable: true }));
    await Dom.ui_next_frame();
    await Dom.ui_next_frame();

    const menu = panel.container.querySelector ('#g-devicepanelcmenu');
    if (!menu)
      throw new Error ('DevicePanel context menu not found');
    if (!(menu as HTMLDialogElement).open)
      throw new Error ('DevicePanel context menu did not open');

    const type_buttons = await wait_for_device_buttons (menu, ['Synth', 'FX']);
    const labels = type_buttons.map (b => b.textContent?.trim() ?? '');
    if (!labels.includes ('Synth'))
      throw new Error (`device-type menu lacks 'Synth' button: ${labels.join (', ')}`);
    if (!labels.includes ('FX'))
      throw new Error (`device-type menu lacks 'FX' button: ${labels.join (', ')}`);
  } finally {
    panel.cleanup();
  }

  return true;
}
sub_tests.push (['popup_menutypes', test_devicepanel_popup_menutypes]);

// == Master runner ==
/// Single exported entry point runs all sub-tests in sequence.
export async function test_devicepanel (): Promise<boolean>
{
  if (!sub_tests.length)
    throw new Error ('devicepanel: no sub-tests registered');
  const failures: string[] = [];
  for (const [name, fn] of sub_tests) {
    try {
      await fn();
    } catch (e) {
      failures.push (`${name}: ${(e as Error)?.message ?? e}`);
    }
  }
  if (failures.length)
    throw new Error ('devicepanel failures:\n  ' + failures.join ('\n  '));
  return true;
}
