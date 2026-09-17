import { createSignal } from 'solid-js';
import { createComponent, render } from 'solid-js/web';
import { TrackView } from '../b/trackview';
import * as Dom from '../dom';

export async function test_trackview_menu (): Promise<boolean>
{
  const track = App.current_track;
  const channel = track.midi_channel;
  const container = document.createElement ('div');
  document.body.appendChild (container);
  const dispose = render (() => createComponent (TrackView, { track, trackindex: 0 }), container);
  try {
    await Dom.ui_next_frame();
    const menu = container.querySelector ('dialog')!;
    if (menu.querySelectorAll ('button[uri^="mc-"]').length !== 17 || menu.querySelectorAll ('.b-menurow.noturn').length !== 4)
      throw new Error ('track menu lost its MIDI channel rows');
    track.midi_channel = 2;
    await track.$asyncs();
    await Dom.ui_next_frame();
    await Dom.ui_next_frame();
    if (menu.querySelector ('button[uri="mc-2"] .b-icon')?.textContent !== '√')
      throw new Error ('track menu did not update its channel mark');
    if (menu.open)
      throw new Error ('updating a track opened its menu');
  } finally {
    track.midi_channel = channel;
    await track.$asyncs();
    dispose();
    container.remove();
  }
  await test_trackview_subscription();
  return true;
}

async function test_trackview_subscription ()
{
  const [channel, set_channel] = createSignal (0);
  let reads = 0;
  const container = document.createElement ('div');
  document.body.appendChild (container);
  const dispose = render (() => createComponent (TrackView, {
    track: {
      name: 'Test',
      get midi_channel () { return channel(); },
      telemetry: async () => { reads++; return []; },
    },
    trackindex: 0,
  }), container);
  try {
    await Dom.ui_next_frame();
    set_channel (3);
    await Dom.ui_next_frame();
    if (reads !== 1)
      throw new Error ('MIDI channel change restarted track telemetry');
  } finally {
    dispose();
    container.remove();
  }
}
