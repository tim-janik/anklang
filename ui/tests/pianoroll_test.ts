// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { createSignal } from 'solid-js';
import { createComponent, render } from 'solid-js/web';
import { PianoRoll } from '../b/pianoroll';
import * as Dom from '../dom';
import * as PianoCtrl from '../b/piano-ctrl.js';
import { START, MOVE, STOP } from '../util.js';

export async function test_pianoroll_shortcuts (): Promise<boolean>
{
  const [hidden, set_hidden] = createSignal (false);
  const container = document.createElement ('div');
  document.body.appendChild (container);
  const dispose = render (() => createComponent (PianoRoll, {
    get hidden () { return hidden(); },
  }), container);
  let tool_clicks = 0;
  let action_clicks = 0;
  const counts = () => [tool_clicks, action_clicks].join (',');
  const press_keys = async () => {
    document.dispatchEvent (new KeyboardEvent ('keydown', {
      key: 'x', code: 'KeyX', ctrlKey: true, bubbles: true, cancelable: true,
    }));
    document.dispatchEvent (new KeyboardEvent ('keydown', {
      key: '1', code: 'Digit1', bubbles: true, cancelable: true,
    }));
    await Dom.ui_next_frame();
  };
  try {
    await Dom.ui_next_frame();
    const grid = container.querySelector<HTMLElement> ('.b-pianoroll-grid')!;
    const tool = container.querySelector ('button[uri="S"]')!;
    const action = container.querySelector<HTMLButtonElement> ('.-pianorollmenu button[kbd="Ctrl+X"]')!;
    tool.addEventListener ('click', () => { tool_clicks++; });
    action.addEventListener ('click', () => { action_clicks++; });
    grid.dispatchEvent (new PointerEvent ('pointerenter'));
    grid.focus();
    grid.dispatchEvent (new FocusEvent ('focus'));
    if (document.activeElement !== grid)
      throw new Error (`grid did not take focus: ${document.activeElement?.outerHTML.slice (0, 200)}`);
    await press_keys();
    if (counts() !== '1,1')
      throw new Error (`visible piano shortcuts failed: ${counts()}`);
    set_hidden (true);
    await Dom.ui_next_frame();
    await press_keys();
    if (counts() !== '1,1')
      throw new Error (`hidden piano shortcuts remained active: ${counts()}`);
    if (!tool.isConnected || !action.isConnected)
      throw new Error ('hiding the piano roll removed its menu items');
    set_hidden (false);
    await Dom.ui_next_frame();
    await press_keys();
    if (counts() !== '1,1')
      throw new Error ('showing the piano roll restored stale hover or focus');
    grid.dispatchEvent (new PointerEvent ('pointerenter'));
    grid.focus();
    grid.dispatchEvent (new FocusEvent ('focus'));
    if (document.activeElement !== grid)
      throw new Error (`grid did not take focus: ${document.activeElement?.outerHTML.slice (0, 200)}`);
    await press_keys();
    if (counts() !== '2,2')
      throw new Error (`piano shortcuts did not resume: ${counts()}`);
  } finally {
    dispose();
    container.remove();
  }
  return true;
}

export async function test_pianoroll_note_units (): Promise<boolean>
{
  let next_id = 2;
  let batches = 0;
  const first = { id: 1, note_number: 60, start_beat: 0.125, length_beats: 0.375, velocity: 127, colour: 12, mute: true };
  const clip = {
    all_notes: [first, { ...first, id: 2, note_number: 62, velocity: 0, colour: 3, mute: false }],
    $props: { $promise: Promise.resolve() },
    async change_batch (batch) {
      batches++;
      const changed = [];
      for (const note of batch) {
        if ('selected' in note || 'fine_tune' in note || 'channel' in note)
          throw new Error ('UI state or removed note fields reached the engine');
        const updated = { ...note, id: note.id > 0 ? note.id : ++next_id };
        clip.all_notes = clip.all_notes.filter (old => old.id !== updated.id);
        if (updated.length_beats !== 0)
          clip.all_notes.push (updated);
        changed.push (updated);
      }
      return changed;
    },
  };
  const note_count = () => clip.all_notes.length;
  const roll = { clip, stepping: [0.25] };
  const controller = new PianoCtrl.PianoCtrl (roll);
  for (const step of [0.25, 2]) {
    roll.stepping = [step];
    const grid = Math.min (step, 1);
    if (controller.quantization() !== grid || controller.quantize (grid * 1.75) !== grid * 2 ||
        controller.quantize (grid * 1.75, false) !== grid)
      throw new Error ('piano grid rounding or truncation changed');
  }
  roll.stepping = [0.25];
  const press = (keyCode, ctrlKey = false, altKey = false) => controller.keydown ({
    keyCode, ctrlKey, shiftKey: false, altKey, preventDefault() {}, stopPropagation() {},
  });
  const action = uri => PianoCtrl.list_actions().find (item => item.uri === uri)!;
  await press (65, true);
  if (batches || PianoCtrl.list_notes (clip).filter (note => note.selected).length !== 2)
    throw new Error ('selecting notes changed engine state or lost the selection');
  if (PianoCtrl.list_notes ({ all_notes: clip.all_notes }).some (note => note.selected))
    throw new Error ('note selection leaked to another clip');
  await press (39);
  if (clip.all_notes.some (note => note.start_beat !== 0.375))
    throw new Error ('note movement did not use fractional beats');
  await press (39, true);
  if (clip.all_notes.some (note => note.length_beats !== 0.625))
    throw new Error ('note resize did not use fractional beats');
  await action ('copy-notes').func (action ('copy-notes'), roll, clip);
  const before_paste = batches;
  await action ('paste-notes').func (action ('paste-notes'), roll, clip);
  const selected = PianoCtrl.list_notes (clip).filter (note => note.selected);
  if (batches !== before_paste + 1 || note_count() !== 4 || selected.length !== 2 || selected.some (note => note.id <= 2))
    throw new Error ('pasted notes did not get distinct selected handles');
  if (!selected.some (note => note.velocity === 127 && note.colour === 12 && note.mute) ||
      !selected.some (note => note.velocity === 0 && note.colour === 3 && !note.mute))
    throw new Error ('copy and paste changed velocity, colour or mute');
  await press (8);
  if (note_count() !== 2 || clip.all_notes.some (note => note.id > 2) || PianoCtrl.list_notes (clip).some (note => note.selected))
    throw new Error ('delete did not use the selected note handles');
  const later = clip.all_notes.find (note => note.id === 2)!;
  later.start_beat += 0.000001;
  later.note_number = 127;
  await press (65, true);
  await press (39, false, true);
  const focused = PianoCtrl.list_notes (clip).filter (note => note.selected);
  if (focused.length !== 1 || focused[0].id !== later.id)
    throw new Error ('note focus sorted close beat positions by pitch');
  return true;
}

export async function test_pianoroll_coalescing (): Promise<boolean>
{
  const track = await Shell.project.create_track();
  const clip = await track.create_midi_clip ('Coalesce Notes', 0, 4);
  const canvas = document.createElement ('canvas');
  const roll = {
    clip, notes_canvas: canvas, pianotool: 'S', stepping: [0.25],
    layout: { beat_from_x: x => x / 100, midinote_from_y: () => 60, xscroll: () => 0, yscroll: () => 0 },
  };
  const controller = new PianoCtrl.PianoCtrl (roll);
  const select = (shiftKey = false) => controller.keydown ({
    keyCode: 65, ctrlKey: true, shiftKey, altKey: false, preventDefault() {}, stopPropagation() {},
  });
  const action = async uri => {
    const item = PianoCtrl.list_actions().find (item => item.uri === uri)!;
    await item.func (item, roll, clip);
  };
  const read_notes = async () => {
    clip.all_notes;
    await clip.$props.$promise;
    return PianoCtrl.list_notes (clip);
  };
  const pointer = (beat, ctrlKey = false) => ({
    target: canvas, offsetX: beat * 100, offsetY: 0, ctrlKey, shiftKey: false,
    preventDefault() {}, stopPropagation() {}, stopImmediatePropagation() {},
  });
  try {
    const [first] = await clip.change_batch ([{
      id: 0, note_number: 60, start_beat: 0.125, length_beats: 0.5, velocity: 100, colour: 7, mute: true,
    }], 'Add Note');
    await select();
    await action ('copy-notes');
    let previous = first.id;
    for (let i = 0; i < 4; i++) {
      await action ('paste-notes');
      const notes = await read_notes();
      const selected = notes.filter (note => note.selected);
      const unselected = notes.filter (note => !note.selected);
      if (notes.length !== 2 || selected.length !== 1 || unselected[0]?.id !== previous || selected[0].id <= previous)
        throw new Error ('repeated paste accumulated identical notes or lost the selected copy');
      if (notes.some (note => note.start_beat !== 0.125 || note.length_beats !== 0.5 ||
                             note.velocity !== 100 || note.colour !== 7 || !note.mute))
        throw new Error ('collapsing notes changed their Tracktion values');
      previous = selected[0].id;
    }
    await select (true);
    const deselected = await read_notes();
    if (deselected.length !== 1 || deselected[0].id !== previous || deselected[0].selected)
      throw new Error ('deselecting copies did not keep the newest note');
    await select();
    for (const { delta, copy } of [{ delta: 0, copy: true }, { delta: 0, copy: true },
                                   { delta: 0, copy: false }, { delta: 1, copy: false }]) {
      const before = (await read_notes()).find (note => note.selected)!;
      const start = pointer (before.start_beat + 0.1, copy);
      const tool = PianoCtrl.notes_canvas_tool_from_hover (roll, start);
      const drag = tool.drag_start (roll);
      drag.drag_event (start, START);
      await action ('copy-notes');
      const dragging = await read_notes();
      if (dragging.length !== 2 || dragging.filter (note => note.selected).length !== 1 ||
          !dragging.some (note => note.id === before.id && !note.selected))
        throw new Error ('starting a drag collapsed the original into its selected copy');
      const end = pointer (before.start_beat + delta + 0.1, copy);
      drag.drag_event (end, MOVE);
      await action ('copy-notes');
      drag.drag_event (end, STOP);
      await action ('copy-notes');
      const after = await read_notes();
      const selected = after.filter (note => note.selected);
      if (after.length !== (copy ? 2 : 1) || selected.length !== 1 ||
          selected[0].id <= before.id || selected[0].start_beat !== before.start_beat + delta)
        throw new Error ('copy or move accumulated identical notes or removed the dragged copy');
    }
  } finally {
    await track.remove_self();
  }
  return true;
}

export async function test_pianoroll_resize_hover (): Promise<boolean>
{
  const canvas = document.createElement ('canvas');
  for (const dpr of [1, 2])
    for (const css_beatscale of [50, 150, 200])
      for (const length_beats of [1 / 64, 1 / 16, 1 / 8, 0.25, 4]) {
        const note = { id: 1, note_number: 60, start_beat: 0.5, length_beats };
        const roll = {
          pianotool: 'P', notes_canvas: canvas, clip: { all_notes: [note], $props: { $promise: Promise.resolve() } },
          layout: {
            DPR: dpr, beatscale: css_beatscale * dpr,
            beat_from_x: x => x / css_beatscale,
            midinote_from_y: () => note.note_number,
          },
        };
        await new PianoCtrl.PianoCtrl (roll).keydown ({
          keyCode: 65, ctrlKey: true, shiftKey: false, altKey: false, preventDefault() {}, stopPropagation() {},
        });
        roll.clip.all_notes.unshift ({ ...note, id: 2 });
        const hover = pixels => PianoCtrl.notes_canvas_tool_from_hover (roll, {
          target: canvas, offsetX: note.start_beat * css_beatscale + pixels, offsetY: 0,
        });
        const tail = hover (length_beats * css_beatscale * 0.9);
        if (tail.cursor !== 'var(--svg-cursor-hresize)' || tail.note_id !== note.id)
          throw new Error (`note tail did not select resize: length=${length_beats}, scale=${css_beatscale}, DPR=${dpr}`);
        const head = hover (length_beats * css_beatscale * 0.25);
        if (head.cursor !== 'var(--svg-cursor-move)' || head.note_id !== note.id)
          throw new Error (`note head did not select move: length=${length_beats}, scale=${css_beatscale}, DPR=${dpr}`);
        if (hover (length_beats * css_beatscale).note_id !== undefined)
          throw new Error ('hover matched beyond the note');
        roll.clip.all_notes = [];
        if (hover (0).note_id !== undefined)
          throw new Error ('hover matched an empty clip');
      }
  return true;
}

export async function test_pianoroll_time_signature (): Promise<boolean>
{
  const project = Shell.project;
  const piano_current = Shell.piano_current();
  const [numerator, set_numerator] = createSignal (3);
  Shell.project = { get numerator () { return numerator(); }, denominator: 8 };
  const container = document.createElement ('div');
  document.body.appendChild (container);
  const labels = new Map<string, number>();
  const fill_text = CanvasRenderingContext2D.prototype.fillText;
  CanvasRenderingContext2D.prototype.fillText = function (text, x, y, ...args) {
    if (container.contains (this.canvas) && this.canvas.classList.contains ('-time_canvas'))
      labels.set (text, x);
    fill_text.call (this, text, x, y, ...args);
  };
  const dispose = render (() => createComponent (PianoRoll, {
    clip: { all_notes: [], end_beat: 8192 }, style: 'width: 1200px; height: 400px',
  }), container);
  try {
    await Dom.ui_next_frame();
    await Dom.ui_next_frame();
    const bar_width = labels.get ('2')! - labels.get ('1')!;
    if (!labels.has ('1.3') || labels.has ('1.4') || !(bar_width > 0))
      throw new Error (`piano grid did not show 3/8 bars: ${[...labels]}`);
    labels.clear();
    set_numerator (6);
    await Dom.ui_next_frame();
    if (!labels.has ('1.6') || !(Math.abs (labels.get ('2')! - labels.get ('1')! - 2 * bar_width) <= 1))
      throw new Error (`piano grid did not redraw for 6/8: ${[...labels]}`);
    const scrollbar = container.querySelector<HTMLElement> ('.-hextend')!.parentElement!;
    for (const bar of [800, 807]) {
      labels.clear();
      const scrolled = new Promise<void> (resolve => scrollbar.addEventListener ('scroll', () => resolve(), { once: true }));
      scrollbar.scrollTo ({ left: (bar * 6 + 0.5) / 8192 * (scrollbar.scrollWidth - scrollbar.clientWidth), behavior: 'instant' });
      await scrolled;
      await Dom.ui_next_frame();
      if (labels.keys().next().value !== '801' || !labels.has (String (bar + 1)) || labels.size > 64)
        throw new Error (`wrong scrolled bar group: first=${labels.keys().next().value}, labels=${labels.size}`);
    }
  } finally {
    dispose();
    CanvasRenderingContext2D.prototype.fillText = fill_text;
    container.remove();
    Shell.project = project;
    Shell.piano_current (...piano_current);
  }
  return true;
}

export async function test_pianoroll_note_colors (): Promise<boolean>
{
  const piano_current = Shell.piano_current();
  const clip = {
    all_notes: [{ id: 1, note_number: 60, start_beat: 0.25, length_beats: 0.5 }],
    end_beat: 16, $props: { $promise: Promise.resolve() },
  };
  const container = document.createElement ('div');
  document.body.appendChild (container);
  let note_rect: number[] = [];
  const fill_rect = CanvasRenderingContext2D.prototype.fillRect;
  CanvasRenderingContext2D.prototype.fillRect = function (x, y, width, height) {
    fill_rect.call (this, x, y, width, height);
    if (container.contains (this.canvas) && this.canvas.classList.contains ('-notes_canvas'))
      note_rect = [x, y, width, height];
  };
  const dispose = render (() => createComponent (PianoRoll, {
    clip, style: 'width: 600px; height: 400px',
  }), container);
  try {
    await Dom.ui_next_frame();
    await Dom.ui_next_frame();
    const canvas = container.querySelector<HTMLCanvasElement> ('.-notes_canvas')!;
    const sample = document.createElement ('canvas');
    sample.width = sample.height = 1;
    const sample_ctx = sample.getContext ('2d', { willReadFrequently: true })!;
    const pixel = () => {
      const [x, y, width, height] = note_rect;
      if (!(width > 0 && height > 0 && x >= 0 && y >= 0 && x + width <= canvas.width && y + height <= canvas.height))
        throw new Error (`test note was not painted inside the canvas: ${note_rect}`);
      sample_ctx.drawImage (canvas, Math.floor (x + width / 2), Math.floor (y + height / 2), 1, 1, 0, 0, 1, 1);
      return [...sample_ctx.getImageData (0, 0, 1, 1).data];
    };
    const brightness = rgb => 0.2126 * rgb[0] + 0.7152 * rgb[1] + 0.0722 * rgb[2];
    const normal = pixel();
    const controller = new PianoCtrl.PianoCtrl ({ clip });
    const select = async shiftKey => {
      await controller.keydown ({
        keyCode: 65, ctrlKey: true, shiftKey, altKey: false, preventDefault() {}, stopPropagation() {},
      });
      await Dom.ui_next_frame();
    };
    await select (false);
    const selected = pixel();
    if (!(brightness (selected) > brightness (normal)))
      throw new Error (`selected note was not brighter: normal=${normal}, selected=${selected}`);
    await select (true);
    if (pixel().join() !== normal.join())
      throw new Error ('deselecting a note did not restore its normal color');
  } finally {
    dispose();
    CanvasRenderingContext2D.prototype.fillRect = fill_rect;
    container.remove();
    Shell.piano_current (...piano_current);
  }
  return true;
}
