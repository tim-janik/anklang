// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { PianoCtrl, notes_canvas_tool_from_hover } from '../b/piano-ctrl';
import { PPQN } from '../util';

export function test_piano_grid_hover (): boolean
{
  const canvas = document.createElement ('canvas');
  const roll = {
    stepping: [PPQN / 4], pianotool: 'P', notes_canvas: canvas,
    layout: { tick_from_x: x => x, midinote_from_y: y => y },
    clip: { all_notes: [
      { id: 11, key: 60, tick: 0, duration: 100, selected: false },
      { id: 22, key: 60, tick: 0, duration: 100, selected: true },
    ] },
  };
  const control = new PianoCtrl (roll);
  for (const step of [PPQN / 4, PPQN * 2]) {
    roll.stepping = [step];
    const grid = Math.min (step, PPQN);
    if (control.quantization() !== grid || control.quantize (grid * 1.75) !== grid * 2 ||
        control.quantize (grid * 1.75, false) !== grid)
      throw new Error ('piano grid rounding or truncation changed');
  }
  for (const [mode, tick, cursor] of [
    ['P', 1, 'move'], ['P', 75, 'hresize'], ['S', 75, 'move'], ['H', 1, 'move'],
  ] as const) {
    roll.pianotool = mode;
    const tool = notes_canvas_tool_from_hover (roll, { target: canvas, offsetX: tick, offsetY: 60 });
    if (tool.note_id !== 22 || tool.cursor !== `var(--svg-cursor-${cursor})`)
      throw new Error (`${mode}/${tick}: hover did not prefer the selected note or choose the right tool`);
  }
  roll.pianotool = 'P';
  for (const [tick, key] of [[100, 60], [0, 61]]) {
    const tool = notes_canvas_tool_from_hover (roll, { target: canvas, offsetX: tick, offsetY: key });
    if (tool.note_id !== undefined || tool.cursor !== 'var(--svg-cursor-pen)')
      throw new Error ('hover matched outside the note');
  }
  roll.clip.all_notes = [];
  if (notes_canvas_tool_from_hover (roll, { target: canvas, offsetX: 0, offsetY: 60 }).note_id !== undefined)
    throw new Error ('hover matched an empty clip');
  return true;
}
