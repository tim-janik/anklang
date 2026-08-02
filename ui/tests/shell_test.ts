// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL-2.0

import { createComponent, render } from 'solid-js/web';
import { createSignal } from 'solid-js';
import { TrackList } from '../b/tracklist';
import { DevicePanel } from '../b/devicepanel';
import { TreeBrowser } from '../b/treebrowser';
import * as Dom from '../dom';

/// Minimal fake project that reports no tracks.
function make_fake_project (create_track = () => {})
{
  return {
    all_tracks: async () => [],
    on: () => () => {},
    create_track,
  };
}

// == Test registry ==
const sub_tests: [string, () => Promise<any>][] = [];

/// Test that TrackList forwards host attributes (class, style) to its root element.
async function test_shell_tracklist_forwarding (): Promise<boolean>
{
  let double_clicks = 0;
  let created_tracks = 0;
  const container = document.createElement ('div');
  document.body.appendChild (container);
  const dispose = render (() => createComponent (TrackList, {
    class: '-row2 -col2',
    style: 'overflow: hidden',
    project: make_fake_project (() => created_tracks++),
    onDblClick: () => double_clicks++,
  }), container);

  try {
    await Dom.ui_next_frame();
    await Dom.ui_next_frame();

    const root = container.querySelector ('.b-tracklist');
    if (!root)
      throw new Error ('TrackList root element not found');
    if (!root.classList.contains ('-row2') || !root.classList.contains ('-col2'))
      throw new Error (`TrackList did not forward class props: ${root.className}`);
    const style = (root as HTMLElement).getAttribute ('style') ?? '';
    if (!style.includes ('overflow: hidden'))
      throw new Error (`TrackList did not forward style props: ${style}`);
    const trackviews = container.querySelector ('.trackviews');
    if (!trackviews)
      throw new Error ('TrackList track views not found');
    trackviews.dispatchEvent (new MouseEvent ('dblclick', { bubbles: true }));
    if (double_clicks !== 1)
      throw new Error (`TrackList did not invoke onDblClick: ${double_clicks}`);
    if (created_tracks !== 1)
      throw new Error (`TrackList did not create a track on double-click: ${created_tracks}`);
  } finally {
    dispose();
    container.remove();
  }

  return true;
}
sub_tests.push (['tracklist_forwarding', test_shell_tracklist_forwarding]);

/// Test that DevicePanel forwards host attributes (class) to its root element.
async function test_shell_devicepanel_forwarding (): Promise<boolean>
{
  const container = document.createElement ('div');
  document.body.appendChild (container);
  const dispose = render (() => createComponent (DevicePanel, {
    class: '-row3 -col2',
    track: {
      access_device: async () => ({ list_device_types: async () => [] }),
    },
  }), container);

  try {
    await Dom.ui_next_frame();
    await Dom.ui_next_frame();

    const root = container.querySelector ('.b-devicepanel');
    if (!root)
      throw new Error ('DevicePanel root element not found');
    if (!root.classList.contains ('-row3') || !root.classList.contains ('-col2'))
      throw new Error (`DevicePanel did not forward class props: ${root.className}`);
  } finally {
    dispose();
    container.remove();
  }

  return true;
}
sub_tests.push (['devicepanel_forwarding', test_shell_devicepanel_forwarding]);

/// Test that TreeBrowser forwards and toggles the hidden attribute.
async function test_shell_treebrowser_hidden (): Promise<boolean>
{
  const [hidden, set_hidden] = createSignal (true);
  const container = document.createElement ('div');
  document.body.appendChild (container);
  const dispose = render (() => createComponent (TreeBrowser, {
    tree: [ { label: 'Root', entries: [ { label: 'Leaf', uri: 'x:leaf' } ] } ],
    get hidden () { return hidden(); },
  }), container);

  try {
    await Dom.ui_next_frame();
    await Dom.ui_next_frame();

    const root = container.querySelector ('.b-treebrowser');
    if (!root)
      throw new Error ('TreeBrowser root element not found');
    if (!root.hasAttribute ('hidden'))
      throw new Error ('TreeBrowser did not forward hidden=true');
    if (root.getAttribute ('hidden') !== 'true' && root.getAttribute ('hidden') !== '')
      throw new Error (`TreeBrowser hidden attribute has unexpected value: ${root.getAttribute ('hidden')}`);

    set_hidden (false);
    await Dom.ui_next_frame();
    await Dom.ui_next_frame();

    if (root.hasAttribute ('hidden'))
      throw new Error ('TreeBrowser did not remove hidden after toggling to false');
  } finally {
    dispose();
    container.remove();
  }

  return true;
}
sub_tests.push (['treebrowser_hidden', test_shell_treebrowser_hidden]);

// == Master runner ==
/// Single exported entry point runs all sub-tests in sequence.
export async function test_shell (): Promise<boolean>
{
  if (!sub_tests.length)
    throw new Error ('shell: no sub-tests registered');
  const failures: string[] = [];
  for (const [name, fn] of sub_tests) {
    try {
      await fn();
    } catch (e) {
      failures.push (`${name}: ${(e as Error)?.message ?? e}`);
    }
  }
  if (failures.length)
    throw new Error ('shell failures:\n  ' + failures.join ('\n  '));
  return true;
}
