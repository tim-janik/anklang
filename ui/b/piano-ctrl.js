// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
//import * as PianoRoll from "./piano-roll.mjs";

import * as Util from '../util.js';

export const PIANO_OCTAVES = 11;
export const PIANO_KEYS = PIANO_OCTAVES * 12;
const MINDURATION = Util.PPQN / 64;
const MINTICKS = MINDURATION / 6;
const ADD = 'add', SUB = 'sub', ASSIGN = 'assign', NONE = -1;

export class PianoCtrl {
  constructor (piano_roll)
  {
    this.piano_roll = piano_roll;
  }
  dom_update()
  {
  }
  quantization ()
  {
    const roll = this.piano_roll, stepping = roll.stepping ? roll.stepping[0] : Util.PPQN;
    return Math.min (stepping, Util.PPQN);
  }
  quantize (tick, nearest = false)
  {
    const quant = this.quantization();
    const fract = tick / quant;
    return (nearest ? Math.round : Math.trunc) (fract) * quant;
  }
  tickdelta (event)
  {
    if (!event.shiftKey)
      return this.quantization();
    const roll = this.piano_roll, layout = roll.layout;
    const pixelstep = 2;
    let dist = Math.ceil (pixelstep / (layout.tickscale * MINTICKS));
    dist = Math.max (MINTICKS, dist * MINTICKS);
    return dist;
  }
  keydown (event)
  {
    const roll = this.piano_roll, msrc = roll.msrc;
    const LEFT = Util.KeyCode.LEFT, UP = Util.KeyCode.UP, RIGHT = Util.KeyCode.RIGHT, DOWN = Util.KeyCode.DOWN;
    const idx = find_note (roll.adata.pnotes, n => roll.adata.focus_noteid == n.id);
    let note = idx >= 0 ? roll.adata.pnotes[idx] : {};
    const big = 9e12; // assert: big * 1000 + 999 < Number.MAX_SAFE_INTEGER
    let nextdist = +Number.MAX_VALUE, nextid = -1, pred, sortscore;
    const SHIFT = 0x1000, CTRL = 0x2000, ALT = 0x4000;
    switch (event.keyCode + (event.shiftKey ? SHIFT : 0) + (event.ctrlKey ? CTRL : 0) + (event.altKey ? ALT : 0)) {
      case ALT + RIGHT:
	if (idx < 0)
	  note = { id: -1, tick: -1, note: -1, duration: 0 };
	pred  = n => n.tick > note.tick || (n.tick == note.tick && n.key >= note.key);
	sortscore = n => (n.tick - note.tick) * 1000 + n.key;
	// nextid = idx >= 0 && idx + 1 < roll.adata.pnotes.length ? roll.adata.pnotes[idx + 1].id : -1;
	break;
      case ALT + LEFT:
	if (idx < 0)
	  note = { id: -1, tick: +big, note: 1000, duration: 0 };
	pred  = n => n.tick < note.tick || (n.tick == note.tick && n.key <= note.key);
	sortscore = n => (note.tick - n.tick) * 1000 + 1000 - n.key;
	// nextid = idx > 0 ? roll.adata.pnotes[idx - 1].id : -1;
	break;
      case 81: // Q
	for (const note of find_notes (roll.adata.pnotes, n => n.selected))
	  msrc.change_note (note.id, this.quantize (note.tick, true), note.duration, note.key, note.fine_tune, note.velocity, note.selected);
	break;
      case LEFT: case SHIFT + LEFT: // ←
	for (const note of find_notes (roll.adata.pnotes, n => n.selected))
	  msrc.change_note (note.id, Math.max (0, note.tick - this.tickdelta (event)), note.duration, note.key, note.fine_tune, note.velocity, note.selected);
	break;
      case RIGHT: case SHIFT + RIGHT: // →
	for (const note of find_notes (roll.adata.pnotes, n => n.selected))
	  msrc.change_note (note.id, note.tick + this.tickdelta (event), note.duration, note.key, note.fine_tune, note.velocity, note.selected);
	break;
      case CTRL + LEFT: case SHIFT + CTRL + LEFT: // ←
	for (const note of find_notes (roll.adata.pnotes, n => n.selected))
	  msrc.change_note (note.id, note.tick, Math.max (MINDURATION, note.duration - this.tickdelta (event)), note.key, note.fine_tune, note.velocity, note.selected);
	break;
      case CTRL + RIGHT: case SHIFT + CTRL + RIGHT: // →
	for (const note of find_notes (roll.adata.pnotes, n => n.selected))
	  msrc.change_note (note.id, note.tick, note.duration + this.tickdelta (event), note.key, note.fine_tune, note.velocity, note.selected);
	break;
      case DOWN: // ↓
	for (const note of find_notes (roll.adata.pnotes, n => n.selected))
	  msrc.change_note (note.id, note.tick, note.duration, Math.max (note.key - 1, 0), note.fine_tune, note.velocity, note.selected);
	break;
      case UP: // ↑
	for (const note of find_notes (roll.adata.pnotes, n => n.selected))
	  msrc.change_note (note.id, note.tick, note.duration, Math.min (note.key + 1, PIANO_KEYS - 1), note.fine_tune, note.velocity, note.selected);
	break;
      case Util.KeyCode.BACKSPACE: case Util.KeyCode.DELETE: // ⌫
	for (const note of find_notes (roll.adata.pnotes, n => n.selected))
	  {
	    msrc.change_note (note.id, note.tick, 0, note.key, note.fine_tune, note.velocity, note.selected);
	    this.change_focus_selection (NONE);
	  }
	break;
    }
    if (note.id && pred && sortscore)
      {
	roll.adata.pnotes.forEach (n => {
	  if (n.id != note.id && pred (n))
	    {
	      const dist = sortscore (n);
	      if (dist < nextdist)
		{
		  nextdist = dist;
		  nextid = n.id;
		}
	    }
	});
      }
    if (nextid > 0)
      this.change_focus_selection (nextid);
    event.preventDefault();
  }
  async notes_click (event) {
    const roll = this.piano_roll, msrc = roll.msrc, layout = roll.layout;
    if (!msrc)
      return;
    event.preventDefault();
    const tick = layout.tick_from_x (event.offsetX);
    const midinote = layout.midinote_from_y (event.offsetY);
    const idx = find_note (roll.adata.pnotes,
			   n => tick >= n.tick && tick < n.tick + n.duration && n.key == midinote);
    if (idx >= 0 && roll.pianotool == 'E')
      {
	let note = roll.adata.pnotes[idx];
	msrc.change_note (note.id, note.tick, 0, note.key, note.fine_tune, note.velocity, note.selected);
      }
    else if (idx >= 0)
      {
	const note = roll.adata.pnotes[idx];
	this.change_focus_selection (note.id);
      }
    else if (roll.pianotool == 'P')
      {
	this.change_focus_selection (NONE);
	const note_id = await msrc.change_note (-1, this.quantize (tick), Util.PPQN / 4, midinote, 0, 1, true);
	if (!(roll.adata.focus_noteid > 0))
	  this.change_focus_selection (note_id);
      }
  }
  drag_event (ev, MODE) {
    const roll = this.piano_roll, layout = roll.layout; // msrc = roll.msrc;
    if (ev.target == roll.$refs.timeline_canvas)
      {
	const tick = layout.tick_from_x (event.offsetX);
	debug ("tdrag:", MODE, tick, ev);
      }
    else if (ev.target == roll.$refs.piano_canvas)
      debug ("pdrag:", MODE, roll.pianotool, ev);
    else if (ev.target == roll.$refs.notes_canvas && roll.pianotool == 'S')
      {
	if (MODE == Util.START)
	  {
	    roll.adata.srect.sx = event.offsetX;
	    roll.adata.srect.sy = event.offsetY;
	    this.change_focus_selection (ASSIGN, []);
	  }
	else if (MODE == Util.MOVE)
	  {
	    const srect = roll.adata.srect;
	    srect.x = Math.min (event.offsetX, srect.sx);
	    srect.y = Math.min (event.offsetY, srect.sy);
	    srect.w = Math.max (event.offsetX, srect.sx) - srect.x;
	    srect.h = Math.max (event.offsetY, srect.sy) - srect.y;
	    const t0 = layout.tick_from_x (srect.x), t1 = layout.tick_from_x (srect.x + srect.w);
	    const k0 = layout.midinote_from_y (srect.y + srect.h), k1 = layout.midinote_from_y (srect.y);
	    const list = find_notes (roll.adata.pnotes,
				     n => (n.key >= k0 && n.key <= k1 &&
					   ((t0 < n.tick && t1 >= n.tick) ||
					    (t0 >= n.tick && t0 < n.tick + n.duration))));
	    this.change_focus_selection (ASSIGN, list);
	  }
	else if (MODE == Util.STOP)
	  {
	    roll.adata.srect.w = 0;
	    roll.adata.srect.h = 0;
	  }
      }
    else
      debug ("odrag:", MODE, roll.pianotool, ev);
  }
  change_focus_selection (focusid, selectionlist) {
    const roll = this.piano_roll, msrc = roll.msrc;
    if (focusid == ADD)
      roll.adata.focus_noteid = undefined; // TODO
    else if (focusid == ASSIGN)
      {
	const ids = new Set (selectionlist.map (note => note.id));
	const allnotes = roll.adata.pnotes;
	for (let i = 0; i < allnotes.length; ++i)
	  {
	    const note = allnotes[i];
	    const s = ids.has (note.id);
	    if (note.selected != s)
	      msrc.toggle_note (note.id, s);
	    if (!s && roll.adata.focus_noteid == note.id)
	      roll.adata.focus_noteid = undefined;
	  }
	if (!(roll.adata.focus_noteid > 0) && selectionlist.length)
	  roll.adata.focus_noteid = selectionlist[0].id;
      }
    else if (focusid == SUB)
      roll.adata.focus_noteid = undefined; // TODO
    else if (focusid != roll.adata.focus_noteid) // numeric
      {
	const oldfocus = roll.adata.focus_noteid;
	roll.adata.focus_noteid = focusid;
	if (roll.adata.focus_noteid > 0)
	  {
	    // this may delete another note, in particular `oldfocus`
	    msrc.toggle_note (roll.adata.focus_noteid, true);
	  }
	if (oldfocus > 0)
	  msrc.toggle_note (oldfocus, false);
      }
  }
}

function find_note (allnotes, predicate) {
  for (let i = 0; i < allnotes.length; ++i)
    {
      const n = allnotes[i];
      if (predicate (n))
	return i;
    }
  return -1;
}
function find_notes (allnotes, predicate) {
  const l = [];
  for (let i = 0; i < allnotes.length; ++i)
    {
      const n = allnotes[i];
      if (predicate (n))
	l.push (n);
    }
  return l;
}
