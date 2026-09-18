import { createSignal } from 'solid-js';
import { createComponent, render } from 'solid-js/web';
import { ClipList } from '../b/cliplist';
import * as Dom from '../dom';

export async function test_cliplist (): Promise<boolean>
{
  const first = { name: 'First', end_tick: 3840, all_notes: [] };
  const second = { name: 'Second', end_tick: 3840, all_notes: [] };
  const [clips, set_clips] = createSignal ([first]);
  const track = {
    get launcher_clips () { return clips(); },
    telemetry: async () => null,
  };
  const container = document.createElement ('div');
  document.body.appendChild (container);
  const dispose = render (() => createComponent (ClipList, { track, trackindex: 0 }), container);
  const original_open = App.open_piano_roll;
  let opened;
  App.open_piano_roll = clip => { opened = clip; };
  try {
    await Dom.ui_next_frame();
    if (container.querySelectorAll ('.b-clipview').length !== 1)
      throw new Error ('initial clip missing');
    set_clips ([second, first]);
    await Dom.ui_next_frame();
    const views = container.querySelectorAll<HTMLElement> ('.b-clipview');
    if (views.length !== 2)
      throw new Error ('added clip missing');
    views[0].click();
    if (opened !== second)
      throw new Error ('first thumbnail opened the wrong clip');
    set_clips ([]);
    await Dom.ui_next_frame();
    if (container.querySelector ('.b-clipview'))
      throw new Error ('removed clips remain visible');
  } finally {
    App.open_piano_roll = original_open;
    dispose();
    container.remove();
  }
  return true;
}
