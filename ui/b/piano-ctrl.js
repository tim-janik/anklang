// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
//import * as PianoRoll from "./piano-roll.mjs";

import * as Util from '../util.js';

import { START, STOP, CANCEL, MOVE, SCROLL } from '../util.js';

/* Many operations on clip events involve all or many events of a clip, e.g. select-all, move.
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
const MINDURATION = Util.PPQN / 64;
const MINTICKS = MINDURATION / 6;
const NONE = 0, SINGLE = 1, ADD = 2, SUB = 3, ASSIGN = 4;
const LEFT = Util.KeyCode.LEFT, UP = Util.KeyCode.UP, RIGHT = Util.KeyCode.RIGHT, DOWN = Util.KeyCode.DOWN;
const SHIFT = 0x1000, CTRL = 0x2000, ALT = 0x4000;

let piano_clipboard = "[]";

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
    const roll = this.piano_roll, stepping = roll.stepping ? roll.stepping[0] : Util.PPQN;
    return Math.min (stepping, Util.PPQN);
  }
  quantize (tick, nearest = true)
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
			       note => ({ tick: this.quantize (note.tick) }));
	await queue_modify_notes (clip, quantize_notes, "Quantize");
	break; }
      case "Q".charCodeAt (0) + SHIFT: {
	const quantize_duration = (note) => {
	  let endtick = this.quantize (note.tick + note.duration);
	  if (endtick <= note.tick)
	    endtick += this.quantization();
	  return { duration: endtick - note.tick };
	};
	const quantize_notes = (clip, allnotes) =>
	  notes_filter_modify (allnotes, note => note.selected, quantize_duration);
	await queue_modify_notes (clip, quantize_notes, "Quantize Duration");
	break; }
      case CTRL + RIGHT: case SHIFT + CTRL + RIGHT: { // →→
	const tickdelta = this.tickdelta (event);
	const notes_elongate = (clip, allnotes) =>
	  notes_filter_modify (allnotes,
			       note => note.selected,
			       note => ({ duration: note.duration + tickdelta }));
	await queue_modify_notes (clip, notes_elongate, "Lengthen");
	break; }
      case CTRL + LEFT: case SHIFT + CTRL + LEFT: { // ←←
	const tickdelta = this.tickdelta (event);
	const shorten = (duration) => duration - (tickdelta < duration ? tickdelta : 0);
	const notes_elongate = (clip, allnotes) =>
	  notes_filter_modify (allnotes,
			       note => note.selected,
			       note => ({ duration: shorten (note.duration) }));
	await queue_modify_notes (clip, notes_elongate, "Shorten");
	break; }
      case RIGHT: case SHIFT + RIGHT: { // →
	const tickdelta = this.tickdelta (event);
	const notes_shift = (clip, allnotes) =>
	  notes_filter_modify (allnotes,
			       note => note.selected,
			       note => ({ tick: note.tick + tickdelta }));
	await queue_modify_notes (clip, notes_shift, "Shift Right");
	break; }
      case LEFT: case SHIFT + LEFT: { // ←
	const tickdelta = this.tickdelta (event);
	let bounced = false;
	const note_mods = note => {
	  const newtick = note.tick - tickdelta;
	  bounced |= newtick < 0;
	  return { tick: newtick };
	};
	const notes_shift = (clip, allnotes) => {
	  const notes = notes_filter_modify (allnotes, note => note.selected, note_mods);
	  return bounced ? false : notes;
	};
	await queue_modify_notes (clip, notes_shift, "Shift Left");
	break; }
      case UP: { // ↑
	let bounced = false;
	const note_mods = note => {
	  const newkey = note.key + 1;
	  bounced |= newkey >= 128;
	  return { key: newkey };
	};
	const notes_shift = (clip, allnotes) =>
	  notes_filter_modify (allnotes, note => note.selected, note_mods, () => !bounced);
	await queue_modify_notes (clip, notes_shift, "Shift Up");
	break; }
      case DOWN: { // ↓
	let bounced = false;
	const note_mods = note => {
	  const newkey = note.key - 1;
	  bounced |= newkey < 0;
	  return { key: newkey };
	};
	const notes_shift = (clip, allnotes) =>
	  notes_filter_modify (allnotes, note => note.selected, note_mods, () => !bounced);
	await queue_modify_notes (clip, notes_shift, "Shift Down");
	break; }
      case ALT + RIGHT:
      case ALT + LEFT: {
	const goright = event.keyCode == RIGHT;
	const sortscore = (n) => n.tick * 1000 + 128 - n.key;
	const advance_selected = (clip, allnotes) => { // SINGLE signature
	  const xnotes = [...allnotes];
	  xnotes.sort ((a, b) => sortscore (a) - sortscore (b));
	  let first = null, last = null, seenselected = false;
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
	const tick = layout.tick_from_x (event.offsetX);
	debug ("tdrag:", MODE, tick, event);
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
    const t0 = layout.tick_from_x (srect.x), t1 = layout.tick_from_x (srect.x + srect.w);
    const k0 = layout.midinote_from_y (srect.y + srect.h), k1 = layout.midinote_from_y (srect.y);

    queue_change_selection (piano_roll, ASSIGN, n => (n.key >= k0 && n.key <= k1 &&
						      ((t0 < n.tick && t1 >= n.tick) ||
						       (t0 >= n.tick && t0 < n.tick + n.duration))));
  }
  debounced_drag_move = Util.debounce (this.drag_move.bind (this), { wait: 17, restart: true, immediate: true });
  move_event (event) // this method is debounced
  {
    const piano_roll = this.piano_roll, layout = piano_roll.layout;
    if (!layout) return; // UI layout may have changed for deferred events
    if (event.target == piano_roll.notes_canvas) {
      const tick = layout.tick_from_x (event.offsetX);
      const midinote = layout.midinote_from_y (event.offsetY);
      const idx = find_note (piano_roll.note_cache.notes,
			     n => tick >= n.tick && tick < n.tick + n.duration && n.key == midinote);
      piano_roll.notes_canvas.setAttribute ('data-notehover', idx >= 0);
    }
  }

  drag_select (event, MODE)
  {
    const piano_roll = this.piano_roll, layout = piano_roll.layout, clip = piano_roll.clip, srect = piano_roll.srect;
    const horizontal = piano_roll.pianotool == 'H';
    if (!clip || event.button >= 2) // contextmenu
      return false;

    // TODO: click on unselected note needs to focus1 and start move
    // TODO: click on selected note needs to start block move

    switch (MODE) {
      case START:
	srect.sx = layout.xscroll(); // scroll offset for bx
	srect.sy = layout.yscroll(); // scroll offset for by
	srect.bx = event.offsetX;
	srect.by = event.offsetY;
	srect.lx = srect.bx; // last x
	srect.ly = srect.by; // last y
	// implicit via case MOVE: queue_change_selection (piano_roll, NONE);
	// fall through
      case SCROLL:
      case MOVE: {
	if (MODE == SCROLL) { // adjust scroll offsets
	  const sx = layout.xscroll(), sy = layout.yscroll();
	  srect.bx += srect.sx - sx;
	  srect.by -= srect.sy - sy;
	  srect.sx = sx;
	  srect.sy = sy;
	} else if (MODE == MOVE) { // adjust last coords
	  srect.lx = event.offsetX;
	  srect.ly = event.offsetY;
	}
	if (horizontal) {
	  srect.sy = 0;
	  srect.by = 0;
	  srect.ly = layout.cssheight;
	}
	// adjust selection
	srect.x = Math.min (srect.lx, srect.bx);
	srect.y = Math.min (srect.ly, srect.by);
	srect.w = Math.max (srect.lx, srect.bx) - srect.x;
	srect.h = Math.max (srect.ly, srect.by) - srect.y;
	piano_roll.srect = srect;
	const t0 = layout.tick_from_x (srect.x), t1 = layout.tick_from_x (srect.x + srect.w);
	const k0 = layout.midinote_from_y (srect.y + srect.h);
	const k1 = layout.midinote_from_y (srect.y);
	let selectmode = ASSIGN;
	if (event.shiftKey && !event.ctrlKey)
	  selectmode = ADD;
	if (!event.shiftKey && event.ctrlKey)
	  selectmode = SUB;
	queue_change_selection (piano_roll, selectmode,
				n => (n.key >= k0 && n.key <= k1 &&
				      ((t0 < n.tick && t1 >= n.tick) ||
				       (t0 >= n.tick && t0 < n.tick + n.duration))));
	break; }
      case STOP:
	srect.w = 0;
	srect.h = 0;
	piano_roll.srect = srect;
	break;
    }
    event.preventDefault();
    event.stopPropagation();
    return false;
  }

  drag_paint (event, MODE)
  {
    const piano_roll = this.piano_roll, clip = piano_roll.clip, layout = piano_roll.layout;
    const event_tick = layout.tick_from_x (event.offsetX);
    const event_key = layout.midinote_from_y (event.offsetY);
    if (!clip)
      return false;

    // stop+prevent *before* await
    event.preventDefault();
    event.stopPropagation();
    switch (MODE) {
      case START:
	queue_change_selection (piano_roll, NONE);
	// fall through
      case SCROLL:
      case MOVE: {
	const paint_note = (clip, allnotes) => {
	  const note_idx = find_note (allnotes, n => event_tick >= n.tick && event_tick < n.tick + n.duration && event_key == n.key);
	  if (note_idx < 0) {
	    const newnote = { channel: 0, key: event_key, velocity: 1, fine_tune: 0,
			      tick: this.quantize (event_tick, false),
			      duration: Util.PPQN / 4,
			      selected: true };
	    return [ newnote ];
	  }
	  return false;
	};
	queue_modify_notes (clip, paint_note, "Paint Note");
	break; }
      case STOP:
	break;
    }
    return false;
  }

  drag_erase (pointerevent, MODE)
  {
    const piano_roll = this.piano_roll, clip = piano_roll.clip, layout = piano_roll.layout;
    if (!clip)
      return false;
    // stop+prevent *before* await
    pointerevent.preventDefault();
    pointerevent.stopPropagation();

    /* To allow consecutive erasure, (offsetX,offsetY) from two successive events need to be conected.
     * Tick resolution is much higher than X pixels, so use pixels horizontally.
     * Key resolution is lower than Y pixels, so use keys vertically.
     * Event X/Y coordinates are floats (and are fractional in Chrome HIDPI settings), to work
     * with integer line rastering, we scale X up/down around rastering.
     */

    switch (MODE) {
      case START:
	this.erase = { ids: new Set(), x: pointerevent.offsetX, y: pointerevent.offsetY };
	// fall through
      case MOVE: {
	const erase = this.erase; // keep around after change_selection() queued the request
	const positions = [];
	let x0 = erase.x, k0 = layout.midinote_from_y (erase.y);
	for (const event of Util.coalesced_events (pointerevent)) {
	  erase.x = event.offsetX;
	  erase.y = event.offsetY;
	  const x1 = erase.x, k1 = layout.midinote_from_y (erase.y);
	  for (const pos of Util.raster_line (x0*2, k0, x1*2, k1)) {
	    const t = layout.tick_from_x (pos[0]/2), k = pos[1]; // pos[0] is x -> tick, pos[1] is key
	    if (positions.length == 0 ||
		positions[positions.length-1].tick != t ||
		positions[positions.length-1].key != k)
	      positions.push ({ tick: t, key: k });
	  }
	  x0 = x1;
	  k0 = k1;
	}
	const erasable_note = (note) => {
	  if (erase.ids.has (note.id))
	    return true;
	  for (const pos of positions)
	    if (pos.tick >= note.tick && pos.tick < note.tick + note.duration && pos.key == note.key) {
	      erase.ids.add (note.id);
	      return true;
	    }
	  return false;
	};
	queue_change_selection (piano_roll, ASSIGN, erasable_note);
	break; }
      case STOP: {
	const erase = this.erase; // keep around after modify_notes() queued the request
	this.erase = null;
	const erase_notes = (clip, allnotes) => {
	  const deletions = [];
	  for (const note of allnotes)
	    if (erase.ids.has (note.id))
	      deletions.push (Object.assign ({}, note, { duration: 0 }));
	  return deletions;
	};
	queue_modify_notes (clip, erase_notes, "Erase Notes");
      } // fall through
      case CANCEL:
	queue_change_selection (piano_roll, NONE);
	this.erase = null;
	break;
    }
    return false;
  }
}

function find_note (allnotes, predicate)
{
  for (let i = 0; i < allnotes.length; ++i)
    {
      const n = allnotes[i];
      if (predicate (n))
	return i;
    }
  return -1;
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
	last.clip = clip;
	last.allnotes = await Shell.note_cache_notes (clip);
	last.notes = null;
      }
      const allnotes = last.notes || last.allnotes;
      // carry out mods
      const changednotes = await modifier (clip, allnotes);
      if (Array.isArray (changednotes) && changednotes.length)
	last.notes = changednotes;      // enqueue changes (frozen array replaces allnotes)
      else if (changednotes)            // side effects
	last.clip = null;		// force update polling
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
    let modified = [];
    // Separate modified notes
    for (let i = 0, j = 0; i < notes.length; i++) {
      if (j < allnotes.length && notes[i].id == allnotes[j].id) {
	const anote = allnotes[j++];
	if (note_equals (anote, notes[i]))
	  continue;
      }
      modified.push (notes[i]);
    }
    // commit modifications and selection changes
    if (modified.length)
      await clip.change_batch (modified, undogroup);
  }

  // fast comparison of notes
  function note_equals (a, b) {
    if (a           === b)		return true;
    if (a.id        !== b.id)		return false;
    if (a.channel   !== b.channel)	return false;
    if (a.key       !== b.key)		return false;
    if (a.selected  !== b.selected)	return false;
    if (a.tick      !== b.tick)		return false;
    if (a.duration  !== b.duration)	return false;
    if (a.velocity  !== b.velocity)	return false;
    if (a.fine_tune !== b.fine_tune)	return false;
    return true;
  }
}

// Filter `allnotes` with predicate() and apply object values from patchfunc().
function notes_filter_modify (allnotes, predicate, patchfunc, discard = null)
{
  const notes = [];
  for (const note of allnotes)
    if (predicate (note))
      notes.push (Object.assign ({}, note, patchfunc (note)));
  return !discard || discard (notes) ? notes : null;
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
function _(arg) { return arg; }

async function action_cut_notes (action, piano_roll, clip, event)
{
  const cut_notes = (clip, allnotes) => {
    let newnotes = deepcopy_notes (allnotes, n => n.selected);
    newnotes = newnotes.map (strip_note_id);
    piano_clipboard = JSON.stringify (newnotes);
    newnotes.length = 0;
    for (let note of allnotes)
      if (note.selected)
	newnotes.push ({ id: note.id, duration: 0 });
    return newnotes;
  };
  await queue_modify_notes (clip, cut_notes, action.label);
}
action ("fa-scissors", _("Cut Notes"), action_cut_notes, "Ctrl+X");

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
action ("fa-files-o", _("Copy Notes"), action_copy_notes, "Ctrl+C");

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
action ("fa-clipboard", _("Paste Notes"), action_paste_notes, "Ctrl+V");

function delete_note_batch (clip, allnotes)
{
  const delnotes = [];
  for (const note of allnotes)
    if (note.selected)
      delnotes.push ({ id: note.id, duration: 0 });
  return delnotes;
}
async function action_delete_notes (action, piano_roll, clip, event)
{
  await queue_modify_notes (clip, delete_note_batch, action.label);
}
action ("fa-times-circle", _("Delete Notes"), action_delete_notes, "Delete");

// register menu action
function action (icon, label, func, kbd) {
  (action.list = action.list || []).push ({ ic: icon, label, kbd, func });
}

/// List menu actions for PianoRoll.
export const list_actions = () => action.list;
