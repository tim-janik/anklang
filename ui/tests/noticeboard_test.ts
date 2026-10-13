// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { createComponent, render } from 'solid-js/web';
import { Noticeboard, create_note } from '../b/noticeboard';

export async function test_noticeboard (): Promise<boolean>
{
  const container = document.createElement ('div');
  document.body.appendChild (container);
  const dispose = render (() => createComponent (Noticeboard, {}), container);
  const original_set = window.setTimeout, original_clear = window.clearTimeout;
  const timers = new Map<number, () => void>();
  let next_id = 0;
  window.setTimeout = ((callback: () => void) => {
    timers.set (++next_id, callback);
    return next_id;
  }) as typeof window.setTimeout;
  window.clearTimeout = id => { timers.delete (id!); };
  const tick = () => {
    const pending = [...timers];
    for (const [id, callback] of pending) {
      if (!timers.delete (id)) continue;
      callback();
    }
  };
  try {
    const dismiss = create_note ('dismiss during fade-in');
    const note = container.querySelector ('.note-board-note')!;
    dismiss();
    dismiss();
    if (timers.size !== 1)
      throw new Error ('dismissal left extra timers');
    tick();
    if (note.isConnected || timers.size)
      throw new Error ('dismissed notice left DOM or timers');

    create_note ('duplicate');
    create_note ('duplicate');
    if (container.querySelectorAll ('.note-board-fadeout').length !== 1)
      throw new Error ('duplicate notice was not dismissed');
    tick();
    if (container.querySelectorAll ('.note-board-note').length !== 1)
      throw new Error ('duplicate notice was not removed');
    tick();
    tick();
    if (container.querySelector ('.note-board-note') || timers.size)
      throw new Error ('automatic dismissal left DOM or timers');

    create_note ('persistent', -1);
    tick();
    if (timers.size || !container.querySelector ('.note-board-note'))
      throw new Error ('persistent notice scheduled dismissal');
    create_note ('pending fade-in');
    create_note ('pending removal')();
    dispose();
    if (timers.size || container.querySelector ('.note-board-note'))
      throw new Error ('noticeboard cleanup left DOM or timers');
    create_note ('no board')();
    if (timers.size)
      throw new Error ('unmounted noticeboard scheduled a timer');
  } finally {
    dispose();
    window.setTimeout = original_set;
    window.clearTimeout = original_clear;
    container.remove();
  }
  return true;
}
