// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { createComponent, render } from 'solid-js/web';
import { Noticeboard, create_note } from '../b/noticeboard';
import { TestTimers } from './timers';

export async function test_noticeboard (): Promise<boolean>
{
  const container = document.createElement ('div');
  document.body.appendChild (container);
  const dispose = render (() => createComponent (Noticeboard, {}), container);
  const timers = new TestTimers();
  try {
    const dismiss = create_note ('dismiss during fade-in');
    const note = container.querySelector ('.note-board-note')!;
    dismiss();
    dismiss();
    if (timers.pending !== 1)
      throw new Error ('dismissal left extra timers');
    timers.run();
    if (note.isConnected || timers.pending)
      throw new Error ('dismissed notice left DOM or timers');

    create_note ('duplicate');
    create_note ('duplicate');
    if (container.querySelectorAll ('.note-board-fadeout').length !== 1)
      throw new Error ('duplicate notice was not dismissed');
    timers.run();
    if (container.querySelectorAll ('.note-board-note').length !== 1)
      throw new Error ('duplicate notice was not removed');
    timers.run();
    timers.run();
    if (container.querySelector ('.note-board-note') || timers.pending)
      throw new Error ('automatic dismissal left DOM or timers');

    create_note ('persistent', -1);
    timers.run();
    if (timers.pending || !container.querySelector ('.note-board-note'))
      throw new Error ('persistent notice scheduled dismissal');
    create_note ('pending fade-in');
    create_note ('pending removal')();
    dispose();
    if (timers.pending || container.querySelector ('.note-board-note'))
      throw new Error ('noticeboard cleanup left DOM or timers');
    create_note ('no board')();
    if (timers.pending)
      throw new Error ('unmounted noticeboard scheduled a timer');
  } finally {
    dispose();
    timers.restore();
    container.remove();
  }
  return true;
}
