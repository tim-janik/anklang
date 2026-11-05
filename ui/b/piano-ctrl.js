// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { createSignal } from 'solid-js';
import * as Util from '../util.js';

import { START, STOP, CANCEL, MOVE, SCROLL } from '../util.js';

/** @file piano-ctrl.js
 * Many operations on clip events involve all or many events of a clip, e.g. select-all, move.
 * Clips can potentially have many events (e.g. hundreds or thausands of notes), the time
 * needed for IPC calls to modify events increases linearly with the number of events involved
 * (change_batch() call arguments and fetching updates via list_all_notes()).
 * Using 'await' for an IPC call at every pointermove with several thausand events on a clip
 * takes too long to maintain good interactive behaviour.
 * So we use a transactional programming model for note modifications, that allows to combine
 * many note updates (e.g. selection rectangle changes) into a single IPC call, but not all
 * (e.g. note creation needs await-synchronisation for note.id creation).
 * Consequently operations are applied to a local cache of clip events and only a limited
 * number of functions may be used to modify notes.
 *
 * queue_change_selection(): modify cached selection of notes according to a predicate
 * queue_modify_notes():     modify local event cache with a callback
 *
 * The predicate or callback will be executed *after* the queue_*() call, so external
 * objects should not be referenced, copy needed objects into locals first.
 *
 * Note that IPC notifications are delivered before the reply value of a modifying call,
 * so proper notification handling with immediate reload requests will produce a consistent
 * remote cache.
 * Commits are diffing the modified events against the cached state. While these comparisons
 * take extra time, significant savings can be accomplished by reducing the IPC traffic.
 */

export const PIANO_OCTAVES = 11;
export const PIANO_KEYS = PIANO_OCTAVES * 12;
const MINDURATION = 1 / 64;
const MINBEATS = MINDURATION / 6;
const NONE = 0, SINGLE = 1, ADD = 2, SUB = 3, ASSIGN = 4;
const LEFT = Util.KeyCode.LEFT, UP = Util.KeyCode.UP, RIGHT = Util.KeyCode.RIGHT, DOWN = Util.KeyCode.DOWN;
const SHIFT = 0x1000, CTRL = 0x2000, ALT = 0x4000;

let piano_clipboard = "[]";

function quantization (piano_roll)
{
  const stepping = piano_roll.stepping ? piano_roll.stepping[0] : 1;
  return Math.min (stepping, 1);
}

function quantize (piano_roll, beat, nearest = true)
{
  const quant = quantization (piano_roll);
  const fract = beat / quant;
  return (nearest ? Math.round : Math.trunc) (fract) * quant;
}

export class PianoCtrl {
  constructor (piano_roll)
  {
    this.piano_roll = piano_roll;
    this.erase = null;
  }
  dom_update()
  {
  }
  quantization ()
  {
    return quantization (this.piano_roll);
  }
  quantize (beat, nearest = true)
  {
    return quantize (this.piano_roll, beat, nearest);
  }
  beatdelta (event)
  {
    if (!event.shiftKey)
      return this.quantization();
    const roll = this.piano_roll, layout = roll.layout;
    const pixelstep = 2;
    let dist = Math.ceil (pixelstep / (layout.beatscale * MINBEATS));
    dist = Math.max (MINBEATS, dist * MINBEATS);
    return dist;
  }
  keydown (event)
  {
    const roll = this.piano_roll, clip = roll.clip;
    if (!clip) return;
    const options = { stop_event: false };
    const promise = this.async_keydown (event, options);
    if (options.stop_event)
      {
	event.preventDefault();
	event.stopPropagation();
      }
    return promise;
  }
  async async_keydown (event, options)
  {
    const roll = this.piano_roll, clip = roll.clip;
    // most switch cases need event.preventDefault()
    options.stop_event = true; // must be assigned *before* await
    // key handling
    const key_ctrl_alt_shift = event.keyCode + (event.shiftKey && SHIFT) + (event.ctrlKey && CTRL) + (event.altKey && ALT);
    switch (key_ctrl_alt_shift) {
      default:
	// undo order for event.preventDefault() *before* await or async return
	options.stop_event = false;
	return; // returns promise, but options.stop_event was modified before that
      case "A".charCodeAt (0) + CTRL:
	await queue_change_selection (roll, ASSIGN, n => 1);
	break;
      case "A".charCodeAt (0) + CTRL + SHIFT:
	await queue_change_selection (roll, ASSIGN, n => 0);
	break;
      case "I".charCodeAt (0) + CTRL:
	await queue_change_selection (roll, ASSIGN, n => !n.selected);
	break;
      case Util.KeyCode.BACKSPACE: // ⌫
	await queue_modify_notes (clip, delete_note_batch, "Delete");
	break;
      case "Q".charCodeAt (0): {
	const quantize_notes = (clip, allnotes) =>
	  notes_filter_modify (allnotes,
			       note => note.selected,
			       note => ({ start_beat: this.quantize (note.start_beat) }));
	await queue_modify_notes (clip, quantize_notes, "Quantize");
	break; }
      case "Q".charCodeAt (0) + SHIFT: {
	const quantize_duration = (note) => {
	  let endbeat = this.quantize (note.start_beat + note.length_beats);
	  if (endbeat <= note.start_beat)
	    endbeat += this.quantization();
	  return { length_beats: endbeat - note.start_beat };
	};
	const quantize_notes = (clip, allnotes) =>
	  notes_filter_modify (allnotes, note => note.selected, quantize_duration);
	await queue_modify_notes (clip, quantize_notes, "Quantize Duration");
	break; }
      case CTRL + RIGHT: case SHIFT + CTRL + RIGHT: { // →→
	const beatdelta = this.beatdelta (event);
	const notes_elongate = (clip, allnotes) =>
	  notes_filter_modify (allnotes,
			       note => note.selected,
			       note => ({ length_beats: note.length_beats + beatdelta }));
	await queue_modify_notes (clip, notes_elongate, "Lengthen");
	break; }
      case CTRL + LEFT: case SHIFT + CTRL + LEFT: { // ←←
	const beatdelta = this.beatdelta (event);
	const shorten = (duration) => duration - (beatdelta < duration ? beatdelta : 0);
	const notes_elongate = (clip, allnotes) =>
	  notes_filter_modify (allnotes,
			       note => note.selected,
			       note => ({ length_beats: shorten (note.length_beats) }));
	await queue_modify_notes (clip, notes_elongate, "Shorten");
	break; }
      case RIGHT: case SHIFT + RIGHT: { // →
	const beatdelta = this.beatdelta (event);
	const notes_shift = (clip, allnotes) =>
	  notes_filter_modify (allnotes,
			       note => note.selected,
			       note => ({ start_beat: note.start_beat + beatdelta }));
	await queue_modify_notes (clip, notes_shift, "Shift Right");
	break; }
      case LEFT: case SHIFT + LEFT: { // ←
	const beatdelta = this.beatdelta (event);
	let bounced = false;
	const note_mods = note => {
	  const newbeat = note.start_beat - beatdelta;
	  bounced |= newbeat < 0;
	  return { start_beat: newbeat };
	};
	const notes_shift = (clip, allnotes) => {
	  const notes = notes_filter_modify (allnotes, note => note.selected, note_mods);
	  return bounced ? false : notes;
	};
	await queue_modify_notes (clip, notes_shift, "Shift Left");
	break; }
      case UP: case SHIFT + UP: { // ↑
	const semitones = (key_ctrl_alt_shift & SHIFT) ? 12 : 1;
	let bounced = false;
	const note_mods = note => {
	  const newkey = note.note_number + semitones;
	  bounced |= newkey >= 128;
	  return { note_number: newkey };
	};
	const notes_shift = (clip, allnotes) =>
	  notes_filter_modify (allnotes, note => note.selected, note_mods, () => !bounced);
	await queue_modify_notes (clip, notes_shift, "Shift Up");
	break; }
      case DOWN: case SHIFT + DOWN: { // ↓
	const semitones = (key_ctrl_alt_shift & SHIFT) ? 12 : 1;
	let bounced = false;
	const note_mods = note => {
	  const newkey = note.note_number - semitones;
	  bounced |= newkey < 0;
	  return { note_number: newkey };
	};
	const notes_shift = (clip, allnotes) =>
	  notes_filter_modify (allnotes, note => note.selected, note_mods, () => !bounced);
	await queue_modify_notes (clip, notes_shift, "Shift Down");
	break; }
      case ALT + RIGHT:
      case ALT + LEFT: {
	const goright = event.keyCode == RIGHT;
	const advance_selected = (clip, allnotes) => { // SINGLE signature
	  const xnotes = [...allnotes];
	  xnotes.sort ((a, b) => a.start_beat - b.start_beat || b.note_number - a.note_number);
	  let first = null, last = null, seenselected = 0;
	  for (const note of xnotes) {
	    if (note.selected) {
	      seenselected++;
	      last = null;
	    } else if (!seenselected)
	      first = note;
	    else if (seenselected && !last)
	      last = note;
	  }
	  if (goright && xnotes.length) {
	    if (!last)
	      last = xnotes[xnotes.length-1];
	    return last.id;
	  } else if (!goright && xnotes.length) {
	    if (!first)
	      first = xnotes[0];
	    return first.id;
	  }
	  return null; // none possible
	};
	await queue_change_selection (roll, SINGLE, advance_selected);
	break; }
    }
  }

  drag_event (event, MODE) {
    const piano_roll = this.piano_roll, layout = piano_roll.layout, clip = piano_roll.clip;
    if (!clip || event.button >= 2) // contextmenu
      return;
    if (event.target == piano_roll.notes_canvas && MODE == Util.START)
      piano_roll.notes_canvas.setAttribute ('data-notehover', false);
    if (event.target == piano_roll.time_canvas)
      {
	const beat = layout.beat_from_x (event.offsetX);
	debug ("tdrag:", MODE, beat, event);
      }
    else if (event.target == piano_roll.piano_canvas)
      debug ("pdrag:", MODE, piano_roll.pianotool, event);
    else if (event.target == piano_roll.notes_canvas && piano_roll.pianotool == 'S')
      {
	if (MODE == Util.START)
	  {
	    event.preventDefault();
	    const srect = piano_roll.srect;
	    srect.sx = event.offsetX;
	    srect.sy = event.offsetY;
	    piano_roll.srect = srect;
	    queue_change_selection (piano_roll, NONE);
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
	    const srect = piano_roll.srect;
	    srect.w = 0;
	    srect.h = 0;
	    piano_roll.srect = srect;
	  }
      }
    else
      debug ("odrag:", MODE, piano_roll.pianotool, event);
  }
  drag_move (event) {
    const piano_roll = this.piano_roll, layout = piano_roll.layout;
    const srect = piano_roll.srect;
    srect.x = Math.min (event.offsetX, srect.sx);
    srect.y = Math.min (event.offsetY, srect.sy);
    srect.w = Math.max (event.offsetX, srect.sx) - srect.x;
    srect.h = Math.max (event.offsetY, srect.sy) - srect.y;
    piano_roll.srect = srect;
    const t0 = layout.beat_from_x (srect.x), t1 = layout.beat_from_x (srect.x + srect.w);
    const k0 = layout.midinote_from_y (srect.y + srect.h), k1 = layout.midinote_from_y (srect.y);

    queue_change_selection (piano_roll, ASSIGN, n => (n.note_number >= k0 && n.note_number <= k1 &&
						      ((t0 < n.start_beat && t1 >= n.start_beat) ||
						       (t0 >= n.start_beat && t0 < n.start_beat + n.length_beats))));
  }
  debounced_drag_move = Util.debounce (this.drag_move.bind (this), { wait: 17, restart: true, immediate: true });
}

/// Add/register piano-roll canvas tool
function ntool (toolmode, drag_event, cursor = 'default', predicate)
{
  const tool = { toolmode, drag_event, cursor, predicate };
  ntool_list.push (tool);
}
const ntool_list = [];

/// ### Select Tool
/// ![Crosshair](cursors/cross.svg)
/// The Select Tool allows selection of single notes or groups of notes. Modifier keys can be used to modify the selection behavior.
///
/// ### Horizontal Select
function notes_canvas_drag_select (event, MODE)
{
  Util.prevent_event (event);
  this.update_coords (event, MODE); // piano_layout_update_coords
  const srect = this.piano_roll.srect, horizontal = this.piano_roll.pianotool == 'H';

  // TODO: click on unselected note needs to focus1 and start move
  // TODO: click on selected note needs to start block move

  switch (MODE) {
    case START:
      // implicit via case MOVE: queue_change_selection (this.piano_roll, NONE);
      // fall through
    case SCROLL:
    case MOVE: {
      const sx = this.piano_roll.layout.xscroll(), sy = this.piano_roll.layout.yscroll();
      const bx = this.begin.x + this.begin.sx - sx;
      const by = this.begin.y - this.begin.sy + sy;
      // adjust selection
      srect.x = Math.min (this.x, bx);
      srect.y = Math.min (this.y, by);
      srect.w = Math.max (this.x, bx) - srect.x;
      srect.h = Math.max (this.y, by) - srect.y;
      if (horizontal) {
	srect.y = 0;
	srect.h = this.piano_roll.layout.cssheight;
      }
      this.piano_roll.srect = srect; // forces repaint
      const t0 = this.piano_roll.layout.beat_from_x (srect.x), t1 = this.piano_roll.layout.beat_from_x (srect.x + srect.w);
      const k0 = this.piano_roll.layout.midinote_from_y (srect.y + srect.h);
      const k1 = this.piano_roll.layout.midinote_from_y (srect.y);
      let selectmode = ASSIGN;
      if (event.shiftKey && !event.ctrlKey)
	selectmode = ADD;
      if (!event.shiftKey && event.ctrlKey)
	selectmode = SUB;
      queue_change_selection (this.piano_roll, selectmode,
			      n => (n.note_number >= k0 && n.note_number <= k1 &&
				    ((t0 < n.start_beat && t1 >= n.start_beat) ||
				     (t0 >= n.start_beat && t0 < n.start_beat + n.length_beats))));
      break; }
    case STOP: {
      srect.w = 0;
      srect.h = 0;
      this.piano_roll.srect = srect; // forces repaint
    } // fall through
    case CANCEL:
      break;
  }
  return false;
}
ntool ('S', notes_canvas_drag_select, 'var(--svg-cursor-cross)');
ntool ('H', notes_canvas_drag_select, 'var(--svg-cursor-cross)');

/// ### Paint Tool
/// ![Pen](cursors/pen.svg)
/// With the note Paint Tool, notes can be placed everywhere in the grid by clicking mouse button 1 and possibly keeping it held during drags.
function notes_canvas_drag_paint (event, MODE)
{
  Util.prevent_event (event);
  this.update_coords (event, MODE); // piano_layout_update_coords
  switch (MODE) {
    case START:
      queue_change_selection (this.piano_roll, NONE);
      // fall through
    case SCROLL:
    case MOVE: {
      const event_beat = this.piano_roll.layout.beat_from_x (this.x);
      const event_key = this.piano_roll.layout.midinote_from_y (this.y);
      const paint_note = (clip, allnotes) => {
	const note_idx = find_note (allnotes, n => event_beat >= n.start_beat && event_beat < n.start_beat + n.length_beats && event_key == n.note_number);
	if (note_idx < 0) {
	  const newnote = { note_number: event_key, velocity: 127, colour: 0, mute: false,
			    start_beat: quantize (this.piano_roll, event_beat, false),
			    length_beats: this.piano_roll.last_note_length,
			    selected: true };
	  return [ newnote ];
	}
	return false;
      };
      queue_modify_notes (this.piano_roll.clip, paint_note, "Paint Note");
      break; }
    case STOP:
    case CANCEL:
      break;
  }
  return false;
}
ntool ('P', notes_canvas_drag_paint, 'var(--svg-cursor-pen)');

/// ### Move Tool
/// ![Pen](cursors/move.svg)
/// With the note Move Tool, selected notes can be moved clicking mouse button 1
/// and keeping it held during drags. A copy will be made instead of moving
/// the selected notes if the ctrl key is pressed during drag.
function notes_canvas_drag_move (event, MODE)
{
  Util.prevent_event (event);
  this.update_coords (event, MODE); // piano_layout_update_coords
  const get_selection_range = (allnotes) => {
    let selected_notes = allnotes.filter (note => note.selected);
    return {
      min_beat : Math.min (...selected_notes.map (note => note.start_beat)),
      min_key  : Math.min (...selected_notes.map (note => note.note_number)),
      max_key  : Math.max (...selected_notes.map (note => note.note_number))
    };
  };
  const select_before_move = (clip, allnotes) => {
    /* TODO: this.note_id is updated during hover, so it can happen that it is
     * not up-to-date which can make the drag operation as a whole fail */
    const note_idx = find_note (allnotes, n => n.id === this.note_id);
    this.move_ok = (note_idx >= 0);
    if (!this.move_ok || allnotes[note_idx].selected)
      return false;
    // select (only) current note if it wasn't selected before move
    return notes_filter_modify (allnotes,
                                note => true,
                                note => ({ selected : (note.id == this.note_id) }));
  };
  const start_move_notes = (clip, allnotes) => {
    const note_idx = find_note (allnotes, n => n.id === this.note_id);
    if (!this.move_ok || note_idx < 0)
      return false;

    this.original_note_ids = {};
    let srange = get_selection_range (allnotes);
    this.move_note_relative_beat = allnotes[note_idx].start_beat - srange.min_beat;
    this.move_note_relative_key = this.event_key - srange.min_key;
    this.move_note_grid_offset = allnotes[note_idx].start_beat - quantize (this.piano_roll, allnotes[note_idx].start_beat, false);
    this.move_note_cursor_offset = this.event_beat - allnotes[note_idx].start_beat;

    const newnotes = [];
    for (const note of allnotes)
      {
        if (note.selected)
          {
            // unselect note
            const newnote = Object.assign ({}, note);
            newnote.selected = false;
            newnotes.push (newnote);
            this.original_note_ids[note.id] = true;
            // create a new note with the same attributes, but selected
            const newnote2 = Object.assign ({}, note);
            newnote2.id = -1;
            newnotes.push (newnote2);
          }
      }
    return newnotes;
  };
  const move_notes = (clip, allnotes) => {
    if (!this.move_ok)
      return false;

    let start_beat = this.event_beat - this.move_note_cursor_offset;
    let snap_beat = 0;
    const snap_to = (beat) => {
      let Q = quantization (this.piano_roll);
      for (const t of [ beat - Q, beat, beat + Q ])
        {
          if (Math.abs (t - start_beat) < Math.abs (snap_beat - start_beat))
            snap_beat = t;
        }
    };
    if (!event.shiftKey)
      {
        // TODO: snapping should be configurable via menu, constraint: one of both needs to be on
        const snap_to_grid = true;
        const snap_to_grid_offset = true;
        if (snap_to_grid)
          snap_to (quantize (this.piano_roll, start_beat));
        if (snap_to_grid_offset)
          snap_to (quantize (this.piano_roll, start_beat - this.move_note_grid_offset) + this.move_note_grid_offset);
      }
    else
      {
        snap_beat = start_beat;
      }
    let srange = get_selection_range (allnotes);
    let delta_key = this.event_key - srange.min_key - this.move_note_relative_key;
    let delta_beat = snap_beat - srange.min_beat - this.move_note_relative_beat;
    // constrain output beat/key range
    if (srange.min_beat + delta_beat < 0)
      delta_beat = -srange.min_beat;
    if (srange.min_key + delta_key < 0)
      delta_key = -srange.min_key;
    if (srange.max_key + delta_key > 127)
      delta_key = 127 - srange.max_key;
    // move selected notes
    return notes_filter_modify (allnotes,
                                note => note.selected,
                                note => ({ start_beat: note.start_beat + delta_beat, note_number: note.note_number + delta_key }));
  };
  const end_move_notes = (clip, allnotes) => {
    if (!this.move_ok)
      return false;

    let killnotes = [];
    // if ctrl key was pressed: copy notes, otherwise move notes
    if (!event.ctrlKey)
      {
        // TODO: use different mouse cursors for copy and move
        killnotes = notes_filter_modify (allnotes,
                                         note => this.original_note_ids[note.id],
                                         note => ({ length_beats: 0 }));
      }
    // cleanup state
    this.original_note_ids = undefined;
    return killnotes;
  };
  // TODO: START/SCROLL/MOVE/STOP should be in one undo group
  switch (MODE) {
    case START:
      this.event_beat = this.piano_roll.layout.beat_from_x (this.x);
      this.event_key = this.piano_roll.layout.midinote_from_y (this.y);
      queue_modify_notes (this.piano_roll.clip, select_before_move, "Select Note Before Move");
      queue_modify_notes (this.piano_roll.clip, start_move_notes, "Start Move Notes");
      break;
    case SCROLL:
    case MOVE:
      this.event_beat = this.piano_roll.layout.beat_from_x (this.x);
      this.event_key = this.piano_roll.layout.midinote_from_y (this.y);
      queue_modify_notes (this.piano_roll.clip, move_notes, "Move Notes");
      break;
    case STOP:
      queue_modify_notes (this.piano_roll.clip, end_move_notes, "End Move Notes");
      break;
    case CANCEL:
      // TODO: reset state
      queue_modify_notes (this.piano_roll.clip, end_move_notes, "End Move Notes");
      break;
  }
  return false;
}
ntool ('P', notes_canvas_drag_move, 'var(--svg-cursor-move)', note_hover_head);
ntool ('S', notes_canvas_drag_move, 'var(--svg-cursor-move)', note_hover_body);
ntool ('H', notes_canvas_drag_move, 'var(--svg-cursor-move)', note_hover_body);

/// #### Resizing Notes
/// ![H-Resize](cursors/hresize.svg)
/// When the Paint Tool is selected, the right edge of a note can be draged to make notes shorter or longer in duration.
function notes_canvas_drag_resize (event, MODE)
{ // this = piano_layout_coords()
  Util.prevent_event (event);
  this.update_coords (event, MODE); // piano_layout_update_coords
  const resize_notes = (clip, allnotes) => {
    const note_idx = find_note (allnotes, n => n.id === this.note_id);
    if (note_idx < 0)
      return false;

    // store delta between note durations to preserve them during resize
    if (this.note_duration_delta[this.note_id] === undefined)
      {
        this.note_duration_delta[this.note_id] = 0;

        for (const note of allnotes)
          if (note.selected)
            this.note_duration_delta[note.id] = note.length_beats - allnotes[note_idx].length_beats;
      }

    // compute new duration of dragged note (may be negative)
    let new_duration = this.event_beat - allnotes[note_idx].start_beat;
    if (!event.shiftKey)
      new_duration = quantize (this.piano_roll, new_duration, true);

    // if note that is being resized was not selected before:
    //  -> select it now and unselect other notes
    let select_note = !allnotes[note_idx].selected;

    const newnotes = [];
    for (const note of allnotes)
      {
        const newnote = Object.assign ({}, note);
        let changed = false;

        if (select_note)
          {
            newnote.selected = (note.id === this.note_id);
            changed = true;
          }

        if (newnote.selected)
          {
            newnote.length_beats = Math.max (1 / 16, new_duration + this.note_duration_delta[note.id]);
            if (note.id === this.note_id)
              this.last_note_length = newnote.length_beats;
            changed = true;
          }
        if (changed)
          newnotes.push (newnote);
      }
    return newnotes;
  };
  switch (MODE) {
    case START:
      this.last_note_length = this.piano_roll.last_note_length;
      this.note_duration_delta = {};
      break;
    case SCROLL:
    case MOVE:
      this.event_beat = this.piano_roll.layout.beat_from_x (this.x);
      queue_modify_notes (this.piano_roll.clip, resize_notes, "Resize Notes");
      break;
    case STOP:
      this.piano_roll.last_note_length = this.last_note_length;
      this.note_duration_delta = undefined;
      break;
    case CANCEL:
      // TODO: reset note size
      this.note_duration_delta = undefined;
      break;
  }
  return false;
}
ntool ('P', notes_canvas_drag_resize, 'var(--svg-cursor-hresize)', note_hover_tail);

/// ### Erase Tool
/// ![Eraser](cursors/eraser.svg)
/// The Erase Tool allows deletion of all notes selected during a mouse button 1 drag. The deletion can be aborted by the Escape key.
function notes_canvas_drag_erase (event, MODE)
{
  Util.prevent_event (event);

  /* To allow consecutive erasure, (offsetX,offsetY) from two successive events need to be conected.
   * Beat resolution is much higher than X pixels, so use pixels horizontally.
   * Key resolution is lower than Y pixels, so use keys vertically.
   * Event X/Y coordinates are floats (and are fractional in Chrome HIDPI settings), to work
   * with integer line rastering, we scale X up/down around rastering.
   */

  switch (MODE) {
    case START:
      this.update_coords (event, MODE); // piano_layout_update_coords
      this.erase = { ids: new Set(), x: this.x, y: this.y };
      // fall through
    case SCROLL:
    case MOVE: {
      const erase = this.erase; // keep around after change_selection() queued the request
      const positions = [];
      let x0 = erase.x, k0 = this.piano_roll.layout.midinote_from_y (erase.y);
      for (const cevent of Util.coalesced_events (event)) {
	this.update_coords (cevent, MODE); // piano_layout_update_coords
	erase.x = this.x;
	erase.y = this.y;
	const x1 = erase.x, k1 = this.piano_roll.layout.midinote_from_y (erase.y);
	for (const pos of Util.raster_line (x0*2, k0, x1*2, k1)) {
	  const t = this.piano_roll.layout.beat_from_x (pos[0]/2), k = pos[1]; // pos[0] is x -> beat, pos[1] is key
	  if (positions.length == 0 ||
	      positions[positions.length-1].start_beat != t ||
	      positions[positions.length-1].note_number != k)
	    positions.push ({ start_beat: t, note_number: k });
	}
	x0 = x1;
	k0 = k1;
      }
      const erasable_note = (note) => {
	if (erase.ids.has (note.id))
	  return true;
	for (const pos of positions)
	  if (pos.note_number == note.note_number && pos.start_beat >= note.start_beat && pos.start_beat < note.start_beat + note.length_beats) {
	    erase.ids.add (note.id);
	    return true;
	  }
	return false;
      };
      queue_change_selection (this.piano_roll, ASSIGN, erasable_note);
      break; }
    case STOP: {
      const erase = this.erase; // keep around after modify_notes() queued the request
      this.erase = null;
      const erase_notes = (clip, allnotes) => {
	const deletions = [];
	for (const note of allnotes)
	  if (erase.ids.has (note.id))
	    deletions.push (Object.assign ({}, note, { length_beats: 0 }));
	return deletions;
      };
      queue_modify_notes (this.piano_roll.clip, erase_notes, "Erase Notes");
    } // fall through
    case CANCEL:
      this.erase = null;
      queue_change_selection (this.piano_roll, NONE);
      break;
  }
  return false;
}
ntool ('E', notes_canvas_drag_erase, 'var(--svg-cursor-eraser)');

/// Detect note if hovering over its body
function note_hover_body (coords, beat, key, notes)
{
  const note_idx = find_note (notes, n => key == n.note_number && beat >= n.start_beat && beat < n.start_beat + n.length_beats);
  return note_idx >= 0 ? { note_id: notes[note_idx].id } : null;
}

/// Detect note if hovering over its tail
function note_hover_tail (coords, beat, key, notes)
{
  const note_idx = find_note (notes, n => key == n.note_number && beat >= n.start_beat + n.length_beats / 2 && beat < n.start_beat + n.length_beats);
  return note_idx >= 0 ? { note_id: notes[note_idx].id } : null;
}

/// Detect note if hovering over its head
function note_hover_head (coords, beat, key, notes)
{
  if (note_hover_tail (coords, beat, key, notes))
    return null;
  return note_hover_body (coords, beat, key, notes);
}

/// Get drag tool and cursor from hover position
export function notes_canvas_tool_from_hover (piano_roll, pointerevent)
{
  const layout = piano_roll.layout;
  if (!layout)
    return { drag_start: (p) => ({ drag_event: (e,m) => debug ("DRAG no layout", m, e.offsetX, e.offsetY) }), cursor: 'default' };
  // coords for tool predicate
  const coords = target_coords (pointerevent, piano_roll.notes_canvas);
  const event_beat = layout.beat_from_x (coords.x);
  const event_key = layout.midinote_from_y (coords.y);
  // need an early decision if event is on a resizable note, so use last cache without await
  const notes = list_notes (piano_roll.clip);
  const selected_notes = notes.filter (note => note.selected);
  // startup tool handler
  function drag_start (piano_roll)
  {
    const ctool_this = piano_layout_coords (piano_roll.layout, piano_roll.notes_canvas);
    ctool_this.piano_roll = piano_roll;
    if (this)
      Object.assign (ctool_this, this);
    return ctool_this;
  }
  // rate tools and find best tool
  let best_tool = null;
  let best_tool_prio = -1;
  for (let tool of ntool_list)
    if (tool.toolmode === piano_roll.pianotool)
      {
        let tool_prio = !!tool.drag_event + 2 * !!tool.cursor + 4 * !!tool.predicate;
        tool = Object.assign ({ drag_start }, tool);
        if (tool.predicate) // must match predicate
          {
            let result = tool.predicate (coords, event_beat, event_key, selected_notes);
            if (result)
              {
                // prefer predicate matches on selected notes
                tool_prio += 8;
              }
            else
              {
                result = tool.predicate (coords, event_beat, event_key, notes);
                if (!result)
                  continue;
              }
            tool = Object.assign (tool, result);
          }
        if (tool_prio > best_tool_prio)
          {
            best_tool = tool;
            best_tool_prio = tool_prio;
          }
      }
  if (best_tool)
    return best_tool; // found matching tool
  // debug
  return { drag_start, drag_event: (e,m) => debug ("DRAG unhandled", m, e.offsetX, e.offsetY), cursor: 'default' };
}

/// Translate event offsetX,offsetY into taret element
function target_coords (event, target)
{
  if (!target || event.target == target)
    return { x: event.offsetX, y: event.offsetY };
  const org = event.target.getBoundingClientRect();
  const tar = target.getBoundingClientRect();
  return { x: event.offsetX + org.left - tar.left,
	   y: event.offsetY + org.top - tar.top };
}

// Track scroll position and target relative piano layout coordinates
function piano_layout_coords (layout, target)
{
  return { x: 0, y: 0, sx: 0, sy: 0, begin: null, end: null, layout, target, update_coords: piano_layout_update_coords };
}

// Update piano_layout_coords()
function piano_layout_update_coords (event, MODE)
{
  const coords = this, layout = this.layout;
  // adjust coords according to MODE
  switch (MODE) {
    case START:	{	// begin, x, y
      Object.assign (coords, target_coords (event, coords.target)); // x,y
      const sx = layout.xscroll();
      const sy = layout.yscroll();
      coords.begin = { x: coords.x, y: coords.y, sx, sy };
      break; }
    case SCROLL: {		// adjust scroll offsets only
      // coords.sx = layout.xscroll();
      // coords.sy = layout.yscroll();
      break; }
    case MOVE:		// x,y
      Object.assign (coords, target_coords (event, coords.target)); // x,y
      break;
    case STOP:
      coords.end = { x: coords.x, y: coords.y, sx: coords.sx, sy: coords.sy };
      break;
  }
}

function find_note (notes, predicate)
{
  return notes.findIndex (predicate);
}

// UI selection flags are kept outside Tracktion note data.
const selections = new WeakMap();

function selection_signal (clip)
{
  if (!selections.has (clip))
    selections.set (clip, createSignal (new Set()));
  return selections.get (clip);
}

export function list_notes (clip)
{
  if (!clip) return [];
  const selected = selection_signal (clip)[0]();
  return (clip.all_notes || []).map (note => ({ ...note, selected: selected.has (note.id) }));
}

// == queue_change_selection ==
// Asynchronously modify selected `clip` notes and adjust piano_roll focus.
async function queue_change_selection (piano_roll, selectmode, predicate) {
  const clip = piano_roll.clip;
  if (!clip) return;
  return queue_modify_notes (clip, modify_selection);

  // Modify selection in allnotes according to selectmode and predicate
  async function modify_selection (clip, allnotes)
  {
    const newnotes = [];
    let changes = false;
    const toggle_select = (note, flag) => {
      changes = changes || note.selected != flag;
      newnotes.push (note.selected == flag ? note : Object.assign ({}, note, { selected: flag }));
    };
    // apply selection modifications
    if (selectmode == NONE)
      for (const note of allnotes)
	toggle_select (note, false);
    else if (selectmode == SUB)
      for (const note of allnotes)
	toggle_select (note, note.selected && !predicate (note));
    else if (selectmode == ADD)
      for (const note of allnotes)
	toggle_select (note, note.selected || predicate (note));
    else if (selectmode == ASSIGN)
      for (const note of allnotes)
	toggle_select (note, !!predicate (note));
    else if (selectmode == SINGLE) {
      const numericid = predicate (clip, allnotes);
      for (const note of allnotes)
	toggle_select (note, note.id == numericid);
    }
    return changes ? Object.freeze (newnotes) : false;
  }
}

// == queue_modify_notes ==
async function queue_modify_notes (clip, modifier, undogroup = "")
{
  if (!queue_modify_notes.requests_)
    queue_modify_notes.requests_ = [];

  // enqueue request
  queue_modify_notes.requests_.push ({ clip, modifier, undogroup });
  // process request queue
  if (!queue_modify_notes.promise)
    queue_modify_notes.promise = modify_notes_handler();
  return queue_modify_notes.promise;

  // process modification requests, only commit needed changes
  async function modify_notes_handler()
  {
    // memory of last batch modifications
    const last = {
      clip: null, allnotes: null, notes: null
    };
    // process requests in FIFO order
    while (queue_modify_notes.requests_.length) {
      const { clip, modifier, undogroup } = queue_modify_notes.requests_.shift();
      if (last.clip != clip)
	await commit_last (last); // resets last.clip
      // fetch most recent set of notes
      if (clip != last.clip) {
        clip.all_notes;                 // force read-out and auto-updates
        last.clip = clip;
        await clip.$props.$promise;     // finish all_notes fetching
        last.allnotes = list_notes (last.clip);
	last.notes = null;
      }
      const allnotes = last.notes || last.allnotes;
      // carry out mods
      const changednotes = await modifier (clip, allnotes);
      if (Array.isArray (changednotes))
	{
	  if (Object.isFrozen (changednotes))   // selection change, frozen array replaces allnotes
	    last.notes = changednotes;
	  else if (changednotes.length)         // Array with modification list, needs commit
	    last.notes = last.notes ? last.notes.concat (changednotes) : changednotes;
	}
      else if (changednotes)
	{
	  console.assert (Array.isArray (changednotes));        // expect Array or null-ish
	  last.clip = null;                                     // force update polling
	}
      // debug ("modify_notes_handler:", last.notes ? last.notes.length : 0, ( undogroup || !Object.isFrozen (changednotes) ||  queue_modify_notes.requests_.length == 0 ));
      // force early or final commit
      if (undogroup || !Object.isFrozen (changednotes) ||
	  queue_modify_notes.requests_.length == 0) // ensure final commit
	await commit_last (last, undogroup);
      // must recheck requests_.length *after* await commit_last
    }
    // all requests handled, only now reset promise guard
    queue_modify_notes.promise = null;
  }

  // commit actual changes
  async function commit_last (last, undogroup = "")
  {
    if (!last.clip || !Array.isArray (last.notes))
      return;
    const { clip, allnotes, notes } = last;
    last.clip = null;
    const originals = new Map (allnotes.map (note => [note.id, note]));
    const pending = new Map (originals);
    const inserted = [];
    // Deletion takes precedence over other changes to the same handle.
    for (const note of notes)
      if (note.id > 0) {
        if (pending.get (note.id)?.length_beats !== 0)
          pending.set (note.id, note);
      } else if (note.length_beats !== 0)
        inserted.push (note);
    // Keep older handles first, then new notes in insertion order.
    const ordered = [...pending.values()].sort ((a, b) => a.id - b.id).concat (inserted);
    const positions = new Map();
    // Collapse notes at the same beat and pitch, preserving selected drag copies.
    for (let i = 0; i < ordered.length; i++) {
      const note = ordered[i];
      if (note.length_beats === 0)
        continue;
      const key = [ note.start_beat, note.note_number, !!note.selected ].join (':');
      const previous = positions.get (key);
      if (previous !== undefined)
        ordered[previous] = { ...ordered[previous], length_beats: 0 };
      positions.set (key, i);
    }
    const selected = new Set();
    const modified = [];
    // Separate modified notes from UI selection changes.
    for (const note of ordered) {
      if (note.id <= 0 && note.length_beats === 0)
        continue;
      if (note.id > 0 && note.selected && note.length_beats !== 0)
        selected.add (note.id);
      const original = originals.get (note.id);
      if (!original || !note_equals (original, note))
        modified.push (note);
    }
    // Commit modifications and map selected insertions to their session handles.
    if (modified.length) {
      const changed = await clip.change_batch (modified.map (({ selected, ...note }) => note), undogroup);
      for (let i = 0; i < changed.length; i++)
        if (modified[i].selected && changed[i].id > 0 && changed[i].length_beats !== 0)
          selected.add (changed[i].id);
    }
    selection_signal (clip)[1] (selected);
  }

  // fast comparison of notes
  function note_equals (a, b) {
    if (a           === b)		return true;
    if (a.id        !== b.id)		return false;
    if (a.note_number !== b.note_number) return false;
    if (a.start_beat !== b.start_beat) return false;
    if (a.length_beats !== b.length_beats) return false;
    if (a.velocity  !== b.velocity)	return false;
    if (a.colour !== b.colour) return false;
    if (a.mute !== b.mute) return false;
    return true;
  }
}

// Filter `allnotes` with predicate() and apply object values from patchfunc().
function notes_filter_modify (allnotes, predicate, patchfunc, confirm = null)
{
  const notes = [];
  for (const note of allnotes)
    if (predicate (note))
      notes.push (Object.assign ({}, note, patchfunc (note)));
  return !confirm || confirm (notes) ? notes : null;
}

// Set note.id to -1.
function strip_note_id (note) {
  note.id = -1;
  return note;
}

// Make a deep copy of `allnotes` for which `predicate` matches.
function deepcopy_notes (allnotes, predicate) {
  const l = [];
  for (let i = 0; i < allnotes.length; ++i)
    {
      const n = allnotes[i];
      if (predicate (n))
	l.push (Util.copy_deep (n));
    }
  return l;
}

// == PianoRoll menu actions ==
function _ (arg) { return arg; }

async function action_cut_notes (action, piano_roll, clip, event)
{
  const cut_notes = (clip, allnotes) => {
    let newnotes = deepcopy_notes (allnotes, n => n.selected);
    newnotes = newnotes.map (strip_note_id);
    piano_clipboard = JSON.stringify (newnotes);
    newnotes.length = 0;
    for (let note of allnotes)
      if (note.selected)
	newnotes.push ({ id: note.id, length_beats: 0 });
    return newnotes;
  };
  await queue_modify_notes (clip, cut_notes, action.label);
}
action ("fa-scissors", _("Cut Notes"), action_cut_notes, "Ctrl+X", 'cut-notes');

async function action_copy_notes (action, piano_roll, clip, event)
{
  const copy_notes = (clip, allnotes) => {
    let newnotes = deepcopy_notes (allnotes, n => n.selected);
    newnotes = newnotes.map (strip_note_id);
    piano_clipboard = JSON.stringify (newnotes);
    return false;
  };
  await queue_modify_notes (clip, copy_notes, action.label);
}
action ("fa-files_o", _("Copy Notes"), action_copy_notes, "Ctrl+C", 'copy-notes');

async function action_paste_notes (action, piano_roll, clip, event)
{
  const paste_notes = (clip, allnotes) => {
    let newnotes = JSON.parse (piano_clipboard);
    newnotes = newnotes.map (strip_note_id);
    const unselected = notes_filter_modify (allnotes,
					    note => note.selected,
					    note => ({ selected: false }));
    return newnotes.concat (unselected);
  };
  await queue_modify_notes (clip, paste_notes, action.label);
}
action ("fa-clipboard", _("Paste Notes"), action_paste_notes, "Ctrl+V", 'paste-notes');

function delete_note_batch (clip, allnotes)
{
  const delnotes = [];
  for (const note of allnotes)
    if (note.selected)
      delnotes.push ({ id: note.id, length_beats: 0 });
  return delnotes;
}
async function action_delete_notes (action, piano_roll, clip, event)
{
  await queue_modify_notes (clip, delete_note_batch, action.label);
}
action ("fa-times_circle", _("Delete Notes"), action_delete_notes, "Delete", 'delete-notes');

// register menu action
function action (icon, label, func, kbd, uri)
{
  (action.list = action.list || []).push ({ ic: icon, label, kbd, func, uri });
}

/// List menu actions for PianoRoll.
export const list_actions = () => action.list;
