// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import * as Dom from '../dom';
import { TestTimers } from './timers';

export async function test_relayout (): Promise<boolean>
{
  const old_project = App.project;
  const project = await Ase.server.create_project ('ResizeTest');
  const height = document.body.style.height;
  const dpr = Object.getOwnPropertyDescriptor (window, 'devicePixelRatio');
  let timers: TestTimers;
  let unsubscribe = () => {};
  try {
    await App.assign_project (project, 'b-app');
    const track = App.current_track;
    const clip = await track.create_midi_clip ('resize-clip', 0, 4);
    App.open_piano_roll (clip);
    await clip.$refetch (() => [clip.name, clip.end_tick, clip.all_notes]);
    await Dom.ui_next_frame();
    await Dom.ui_next_frame();
    const root = Shell.shell_element;
    const piano = Shell.piano_roll;
    piano.querySelector ('button[uri="S"]').click();
    Shell.r.show_about_dialog_ = true;
    await Dom.ui_next_frame();
    const tool = piano.querySelector ('[data-tip^="**EDITOR TOOL**"]').getAttribute ('data-tip');
    const scroll = piano.querySelector ('.-hextend').parentElement;
    scroll.scrollLeft = 150;
    const position = scroll.scrollLeft;
    let calls = 0;
    const count = () => calls;
    unsubscribe = Shell.on_resize (() => { calls++; });
    timers = new TestTimers();
    document.body.style.height = (document.body.clientHeight - 1) + 'px';
    await Dom.ui_next_frame();
    await Dom.ui_next_frame();
    if (!timers.pending)
      throw new Error ('body resize did not notify Shell');
    Shell.resize();
    Shell.resize();
    timers.run();
    await Dom.ui_next_frame();
    await Dom.ui_next_frame();
    if (count() !== 1)
      throw new Error ('repeated resizes did not produce one notification');
    if (!root.isConnected || Shell.shell_element !== root || Shell.piano_roll !== piano)
      throw new Error ('resize replaced the Shell or piano roll');
    if (App.current_track !== track || Shell.r.piano_roll_source !== clip || !Shell.r.show_about_dialog_)
      throw new Error ('resize lost Shell state');
    if (scroll.scrollLeft !== position || piano.querySelector ('[data-tip^="**EDITOR TOOL**"]').getAttribute ('data-tip') !== tool)
      throw new Error ('resize reset the piano scroll position or tool');
    const ratio = window.devicePixelRatio * 2;
    Object.defineProperty (window, 'devicePixelRatio', { value: ratio, configurable: true });
    Shell.resize();
    timers.run();
    await Dom.ui_next_frame();
    await Dom.ui_next_frame();
    for (const canvas of [document.querySelector ('.b-clipview canvas'), piano.querySelector ('.-notes_canvas')]) {
      if (canvas.width !== Math.round (canvas.clientWidth * ratio))
        throw new Error ('display scaling did not resize a canvas');
    }
    if (!root.isConnected || Shell.shell_element !== root || count() !== 2)
      throw new Error ('display scaling replaced the Shell or skipped its callbacks');
    unsubscribe();
    Shell.resize();
    timers.run();
    if (count() !== 2)
      throw new Error ('removed resize callback still ran');
  } finally {
    unsubscribe();
    if (dpr)
      Object.defineProperty (window, 'devicePixelRatio', dpr);
    else
      delete (window as any).devicePixelRatio;
    document.body.style.height = height;
    timers?.restore();
    await App.assign_project (old_project, 'b-app');
    await project.discard();
  }
  return true;
}
