// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
//import * as PianoRoll from "./piano-roll.mjs";

import * as Util from '../util.js';

export const PIANO_OCTAVES = 11;
export const PIANO_KEYS = PIANO_OCTAVES * 12;
const MINDURATION = Util.PPQN / 64;
const MINTICKS = MINDURATION / 6;
const ADD = 'add', SUB = 'sub', ASSIGN = 'assign', NONE = -1;

let piano_clipboard = "[]";

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
  async keydown (event)
  {
    const roll = this.piano_roll, msrc = roll.msrc;
    if (!msrc) return;
    event.preventDefault(); // needed before *await*
    const allnotes = Util.freeze_deep (await msrc.list_all_notes());
    Data.project.group_undo ("Modify Notes");
    const LEFT = Util.KeyCode.LEFT, UP = Util.KeyCode.UP, RIGHT = Util.KeyCode.RIGHT, DOWN = Util.KeyCode.DOWN;
    const idx = find_note (allnotes, n => roll.adata.focus_noteid == n.id);
    let note = idx >= 0 ? allnotes[idx] : {};
    const big = 9e12; // assert: big * 1000 + 999 < Number.MAX_SAFE_INTEGER
    let nextdist = +Number.MAX_VALUE, nextid = -1, pred, sortscore;
    const SHIFT = 0x1000, CTRL = 0x2000, ALT = 0x4000;
    let newnotes;
    switch (event.keyCode + (event.shiftKey ? SHIFT : 0) + (event.ctrlKey ? CTRL : 0) + (event.altKey ? ALT : 0)) {
      case CTRL + "A".charCodeAt (0):
	newnotes = Util.copy_deep (allnotes);
	for (const note of newnotes)
	  note.selected = true;
	msrc.change_batch (newnotes);
	break;
      case SHIFT + CTRL + "A".charCodeAt (0):
	newnotes = Util.copy_deep (allnotes);
	for (const note of newnotes)
	  note.selected = false;
	msrc.change_batch (newnotes);
	break;
      case CTRL + "I".charCodeAt (0):
	newnotes = Util.copy_deep (allnotes);
	for (const note of newnotes)
	  note.selected = !note.selected;
	msrc.change_batch (newnotes);
	break;
      case 81: // Q
	newnotes = find_notes (allnotes, n => n.selected);
	for (const note of newnotes)
	  note.tick = this.quantize (note.tick, true);
	msrc.change_batch (newnotes);
	break;
      case Util.KeyCode.BACKSPACE: // ⌫
	newnotes = find_notes (allnotes, n => n.selected);
	for (const note of newnotes)
	  note.duration = 0;
	msrc.change_batch (newnotes);
	break;
      case DOWN: // ↓
	newnotes = find_notes (allnotes, n => n.selected);
	for (const note of newnotes)
	  note.key = Math.max (note.key - 1, 0);
	msrc.change_batch (newnotes);
	break;
      case UP: // ↑
	newnotes = find_notes (allnotes, n => n.selected);
	for (const note of newnotes)
	  note.key = Math.min (note.key + 1, PIANO_KEYS - 1);
	msrc.change_batch (newnotes);
	break;
      case LEFT: case SHIFT + LEFT: // ←
	newnotes = find_notes (allnotes, n => n.selected);
	for (const note of newnotes)
	  note.tick = Math.max (0, note.tick - this.tickdelta (event)); // TODO: cache tickdelta
	msrc.change_batch (newnotes);
	break;
      case RIGHT: case SHIFT + RIGHT: // →
	newnotes = find_notes (allnotes, n => n.selected);
	for (const note of newnotes)
	  note.tick += this.tickdelta (event);
	msrc.change_batch (newnotes);
	break;
      case CTRL + LEFT: case SHIFT + CTRL + LEFT: // ←←
	newnotes = find_notes (allnotes, n => n.selected);
	for (const note of newnotes)
	  note.duration = Math.max (MINDURATION, note.duration - this.tickdelta (event));
	msrc.change_batch (newnotes);
	break;
      case CTRL + RIGHT: case SHIFT + CTRL + RIGHT: // →→
	newnotes = find_notes (allnotes, n => n.selected);
	for (const note of newnotes)
	  note.duration = note.duration + this.tickdelta (event);
	msrc.change_batch (newnotes);
	break;
      case ALT + LEFT:
	if (idx < 0)
	  note = { id: -1, tick: +big, note: 1000, duration: 0 };
	pred  = n => n.tick < note.tick || (n.tick == note.tick && n.key <= note.key);
	sortscore = n => (note.tick - n.tick) * 1000 + 1000 - n.key;
	// nextid = idx > 0 ? allnotes[idx - 1].id : -1;
	break;
      case ALT + RIGHT:
	if (idx < 0)
	  note = { id: -1, tick: -1, note: -1, duration: 0 };
	pred  = n => n.tick > note.tick || (n.tick == note.tick && n.key >= note.key);
	sortscore = n => (n.tick - note.tick) * 1000 + n.key;
	// nextid = idx >= 0 && idx + 1 < allnotes.length ? allnotes[idx + 1].id : -1;
	break;
    }
    if (note.id && pred && sortscore)
      {
	allnotes.forEach (n => {
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
      await this.change_selection (nextid);
    Data.project.ungroup_undo();
  }
  notes_click (event) {
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
	const note = roll.adata.pnotes[idx];
	note.duration = 0; // deletes
	msrc.change_note (note);
      }
    else if (idx >= 0)
      {
	//const note = roll.adata.pnotes[idx];
	//this.change_selection (note.id);
      }
    else if (roll.pianotool == 'P')
      {
	(async () => {
	  Data.project.group_undo ("Insert Note");
	  await this.change_selection (NONE);
	  const note = { channel: 0, key: midinote, tick: this.quantize (tick), duration: Util.PPQN / 4, velocity: 1, fine_tune: 0, selected: true };
	  const note_id = await msrc.insert_note (note);
	  if (!(roll.adata.focus_noteid > 0))
	    await this.change_selection (note_id);
	  Data.project.ungroup_undo();
	}) ();
      }
  }
  drag_event (event, MODE) {
    const roll = this.piano_roll, layout = roll.layout, msrc = roll.msrc;
    if (!msrc || event.button >= 2) // contextmenu
      return;
    if (event.target == roll.$refs.notes_canvas && MODE == Util.START)
      roll.$refs.notes_canvas.setAttribute ('data-notehover', false);
    if (event.target == roll.$refs.timeline_canvas)
      {
	const tick = layout.tick_from_x (event.offsetX);
	debug ("tdrag:", MODE, tick, event);
      }
    else if (event.target == roll.$refs.piano_canvas)
      debug ("pdrag:", MODE, roll.pianotool, event);
    else if (event.target == roll.$refs.notes_canvas && roll.pianotool == 'S')
      {
	if (MODE == Util.START)
	  {
	    event.preventDefault();
	    roll.adata.srect.sx = event.offsetX;
	    roll.adata.srect.sy = event.offsetY;
	    this.change_selection (NONE);
	  }
	else if (MODE == Util.MOVE)
	  {
	    event.preventDefault();
	    this.debounced_drag_move (event);
	  }
	else if (MODE == Util.STOP)
	  {
	    event.preventDefault();
	    event.stopPropagation();
	    roll.adata.srect.w = 0;
	    roll.adata.srect.h = 0;
	  }
      }
    else
      debug ("odrag:", MODE, roll.pianotool, event);
  }
  drag_move (event) {
    const roll = this.piano_roll, layout = roll.layout; // , msrc = roll.msrc;
    const srect = roll.adata.srect;
    srect.x = Math.min (event.offsetX, srect.sx);
    srect.y = Math.min (event.offsetY, srect.sy);
    srect.w = Math.max (event.offsetX, srect.sx) - srect.x;
    srect.h = Math.max (event.offsetY, srect.sy) - srect.y;
    const t0 = layout.tick_from_x (srect.x), t1 = layout.tick_from_x (srect.x + srect.w);
    const k0 = layout.midinote_from_y (srect.y + srect.h), k1 = layout.midinote_from_y (srect.y);

    this.change_selection (ASSIGN, n => (n.key >= k0 && n.key <= k1 &&
					 ((t0 < n.tick && t1 >= n.tick) ||
					  (t0 >= n.tick && t0 < n.tick + n.duration))));
  }
  debounced_drag_move = Util.debounce (this.drag_move.bind (this), { wait: 17, restart: true, immediate: true });
  move_event (event) // this method is debounced
  {
    const roll = this.piano_roll, layout = roll.layout; // msrc = roll.msrc;
    if (!layout) return; // UI layout may have changed for deferred events
    if (event.target == roll.$refs.notes_canvas) {
      const tick = layout.tick_from_x (event.offsetX);
      const midinote = layout.midinote_from_y (event.offsetY);
      const idx = find_note (roll.adata.pnotes,
			     n => tick >= n.tick && tick < n.tick + n.duration && n.key == midinote);
      roll.$refs.notes_canvas.setAttribute ('data-notehover', idx >= 0);
    }
  }
  async change_selection (focusid, predicate) {
    const roll = this.piano_roll, msrc = roll.msrc;
    if (!msrc)
      return;
    const allnotes = await msrc.list_all_notes();
    if (focusid == NONE)
      for (const note of allnotes)
	note.selected = false;
    else if (focusid == ADD)
      for (const note of allnotes)
	note.selected = note.selected || predicate (note);
    else if (focusid == ASSIGN)
      for (const note of allnotes)
	note.selected = predicate (note);
    else if (focusid == SUB)
      for (const note of allnotes)
	note.selected = note.selected && !predicate (note);
    else if (!isNaN (focusid)) // numeric id
      for (const note of allnotes)
	note.selected = note.id == focusid;
    msrc.change_batch (allnotes);
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
	l.push (Util.copy_deep (n));
    }
  return l;
}
function strip_note_id (note) {
  note.id = -1;
  return note;
}

export function list_actions () {
  return actions;
}
const actions = [];

function action (icon, label, func, kbd) {
  if (icon)
    func.ic = icon;
  func.label = label;
  if (kbd)
    func.kbd = kbd;
  actions.push (func);
}
function _(arg) { return arg; }

// == Standard PianoRoll operations ==
async function action_cut_notes (event, clip) {
  const allnotes = Util.freeze_deep (await clip.list_all_notes());
  let newnotes = find_notes (allnotes, n => n.selected);
  newnotes = newnotes.map (strip_note_id);
  piano_clipboard = JSON.stringify (newnotes);
  let delnotes = find_notes (allnotes, n => n.selected);
  delnotes = delnotes.map (n => (n.duration = 0, n));
  await Data.project.group_undo (action_cut_notes.label);
  await clip.change_batch (delnotes);
  await Data.project.ungroup_undo();
}
action ("fa-scissors", _("Cut Notes"), action_cut_notes, "Ctrl+X");

async function action_copy_notes (event, clip) {
  const allnotes = Util.freeze_deep (await clip.list_all_notes());
  let newnotes = find_notes (allnotes, n => n.selected);
  newnotes = newnotes.map (strip_note_id);
  piano_clipboard = JSON.stringify (newnotes);
}
action ("fa-files-o", _("Copy Notes"), action_copy_notes, "Ctrl+C");

async function action_paste_notes (event, clip) {
  let newnotes = JSON.parse (piano_clipboard);
  newnotes = newnotes.map (strip_note_id);
  await Data.project.group_undo (action_paste_notes.label);
  await clip.change_batch (newnotes);
  await Data.project.ungroup_undo();
}
action ("fa-clipboard", _("Paste Notes"), action_paste_notes, "Ctrl+V");

async function action_delete_notes (event, clip) {
  const allnotes = Util.freeze_deep (await clip.list_all_notes());
  let newnotes = find_notes (allnotes, n => n.selected);
  for (const note of newnotes)
    note.duration = 0;
  await Data.project.group_undo (action_delete_notes.label);
  await clip.change_batch (newnotes);
  await Data.project.ungroup_undo();
}
action ("fa-times-circle", _("Delete Notes"), action_delete_notes, "Delete");
