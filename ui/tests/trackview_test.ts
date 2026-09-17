import { createSignal } from 'solid-js';
import { createComponent, render } from 'solid-js/web';
import { TrackView } from '../b/trackview';
import * as Dom from '../dom';

export async function test_trackview_subscription (): Promise<boolean>
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
  return true;
}
