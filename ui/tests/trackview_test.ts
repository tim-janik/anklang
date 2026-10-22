// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
import { createSignal } from 'solid-js';
import { createComponent, render } from 'solid-js/web';
import { TrackView } from '../b/trackview';
import * as Dom from '../dom';

export async function test_trackview_menu (): Promise<boolean>
{
  const track = Shell.current_track;
  const container = document.createElement ('div');
  document.body.appendChild (container);
  const dispose = render (() => createComponent (TrackView, { track, trackindex: 0 }), container);
  try {
    await Dom.ui_next_frame();
    const menu = container.querySelector ('dialog')!;
    if (menu.querySelector ('button[uri^="mc-"]') || menu.textContent?.includes ('MIDI Channel'))
      throw new Error ('track menu still offers the unused MIDI channel');
    for (const uri of ['add-track', 'add-midi-clip', 'rename-track', 'delete-track'])
      if (!menu.querySelector (`button[uri="${uri}"]`))
        throw new Error (`track menu lost ${uri}`);
    if (menu.open)
      throw new Error ('mounting a track opened its menu');
  } finally {
    dispose();
    container.remove();
  }
  await test_trackview_subscription();
  return true;
}

async function test_trackview_subscription ()
{
  const [name, set_name] = createSignal ('Test');
  let reads = 0;
  const container = document.createElement ('div');
  document.body.appendChild (container);
  const dispose = render (() => createComponent (TrackView, {
    track: {
      get name () { return name(); },
      telemetry: async () => { reads++; return []; },
    },
    trackindex: 0,
  }), container);
  try {
    await Dom.ui_next_frame();
    set_name ('Renamed');
    await Dom.ui_next_frame();
    if (reads !== 1)
      throw new Error ('track name change restarted track telemetry');
  } finally {
    dispose();
    container.remove();
  }
}
