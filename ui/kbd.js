// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
'use strict';

import * as Util from './util.js';

/// Keyboard modifiers used for hotkeys.
const kbd_modifiers = Util.freeze_deep ([ 'cmd', 'command', 'super', 'meta', 'option', 'alt', 'altgraph', 'control', 'ctrl', 'shift' ]);

/// Create display name from KeyEvent.code names.
export function display_keyname (keyname)
{
  // Strip Raw prefix
  keyname = ('' + keyname).replace (/\bRaw(\w)/g, "$1");
  // Replace KeyX with 'X'
  keyname = keyname.replace (/\bKey([A-Z])/g, "$1");
  // Replace Digit7 with '7'
  keyname = keyname.replace (/\bDigit([0-9])/g, "$1");
  return keyname;
}

/// Create hotkey name from KeyboardEvent.
export function hotkey_name_from_event (event) {
  let keyparts = [];
  if (event.shiftKey)
    keyparts.push ('Shift');
  if (event.ctrlKey)
    keyparts.push ('Ctrl');
  if (event.altKey)
    keyparts.push ('Alt');
  if (event.metaKey)
    keyparts.push ('Meta');
  let charname = event.key || String.fromCharCode (event.which || event.keyCode);
  if (charname === ' ')
    charname = 'Space';
  else if (charname.search (/^[a-z]$/) === 0)
    charname = charname.toUpperCase();
  if (charname && kbd_modifiers.indexOf (charname.toLowerCase()) < 0) {
    keyparts.push (charname);
    return keyparts.join ('+');
  }
  return undefined;
}

/// Match an event's key code, considering modifiers.
export function match_key_event (event, keyname)
{
  // SEE: http://unixpapa.com/js/key.html & https://developer.mozilla.org/en-US/docs/Mozilla/Gecko/Gecko_keypress_event
  // split_hotkey (hotkey)
  const rex = new RegExp (/\s*(?<!\+)[+]\s*/); // Split 'Shift+Ctrl+Q' twice, but split 'Ctrl++' once
  const parts = keyname.split (rex);
  const rawkey = event.code; // contains physical key names for US-Layout
  const charname = event.key || String.fromCharCode (event.which || event.keyCode);
  let need_meta = 0, need_alt = 0, need_ctrl = 0, need_shift = 0;
  for (let i = 0; i < parts.length; i++)
    {
      // collect meta keys
      switch (parts[i].toLowerCase())
      {
	case 'cmd': case 'command':
	case 'super': case 'meta':	need_meta  = 1; continue;
	case 'option': case 'alt':	need_alt   = 1; continue;
	case 'control': case 'ctrl':	need_ctrl  = 1; continue;
	case 'shift':		  	need_shift = 1; continue;
      }
      // match character keys
      if (charname.toLowerCase() == parts[i].toLowerCase())
        continue;
      // match physical keys
      if (parts[i].startsWith ('Raw') &&
	  parts[i].substr (3) == rawkey)
	continue;
      // failed to match
      return false;
    }
  // ignore shift for case insensitive characters (except for navigations)
  if (charname.toLowerCase() == charname.toUpperCase() &&
      !Util.is_navigation_key_code (event.keyCode))
    need_shift = -1;
  // match meta keys
  if (need_meta   != 0 + event.metaKey ||
      need_alt    != 0 + event.altKey ||
      need_ctrl   != 0 + event.ctrlKey ||
      (need_shift != 0 + event.shiftKey && need_shift >= 0))
    return false;
  return true;
}

/// Symbolic names for key codes
export const KeyCode = {
  BACKSPACE: 8, TAB: 9, LINEFEED: 10, ENTER: 13, RETURN: 13, CAPITAL: 20, CAPSLOCK: 20, ESC: 27, ESCAPE: 27, SPACE: 32,
  PAGEUP: 33, PAGEDOWN: 34, END: 35, HOME: 36, LEFT: 37, UP: 38, RIGHT: 39, DOWN: 40, PRINTSCREEN: 44,
  INSERT: 45, DELETE: 46, SELECT: 93,
  F1: 112, F2: 113, F3: 114, F4: 115, F5: 116, F6: 117, F7: 118, F8: 119, F9: 120, F10: 121, F11: 122, F12: 123,
  F13: 124, F14: 125, F15: 126, F16: 127, F17: 128, F18: 129, F19: 130, F20: 131, F21: 132, F22: 133, F23: 134, F24: 135,
  BROWSERBACK: 166, BROWSERFORWARD: 167, PLUS: 187/*?*/, MINUS: 189/*?*/, PAUSE: 230, ALTGR: 255,
  VOLUMEMUTE: 173, VOLUMEDOWN: 174, VOLUMEUP: 175, MEDIANEXTTRACK: 176, MEDIAPREVIOUSTRACK: 177, MEDIASTOP: 178, MEDIAPLAYPAUSE: 179,
};

/// Check if a key code is used of rnavigaiton (and non alphanumeric).
export function is_navigation_key_code (keycode)
{
  const navigation_keys = [
    KeyCode.UP, KeyCode.RIGHT, KeyCode.DOWN, KeyCode.LEFT,
    KeyCode.PAGE_UP, KeyCode.PAGE_DOWN, KeyCode.HOME, KeyCode.END,
    KeyCode.ESCAPE, KeyCode.TAB, KeyCode.SELECT /*contextmenu*/,
    KeyCode.BROWSERBACK, KeyCode.BROWSERFORWARD,
    KeyCode.BACKSPACE, KeyCode.DELETE,
    KeyCode.SPACE, KeyCode.ENTER /*13*/,
  ];
  return Util.in_array (keycode, navigation_keys);
}

/// Get the currently focussed Element, also inspecting open shadowRoot hierarchies.
export function activeElement()
{
  let e = document.activeElement;
  while (e && e.shadowRoot && e.shadowRoot.activeElement)
    e = e.shadowRoot.activeElement;
  return e;
}

function collect_focusables (element, elist, dlist, is_focusable)
{
  const w = document.createTreeWalker (element, NodeFilter.SHOW_ELEMENT);
  let e;
  while ( (e = w.nextNode()) )
    {
      if (e instanceof HTMLDialogElement && e.matches('[open]:modal'))
	dlist.push (e); // open modal dialog
      if (is_focusable (e))
	elist.push (e);
      if (e.shadowRoot)
	collect_focusables (e.shadowRoot, elist, dlist, is_focusable);
    }
}

/// List elements that can take focus including shadow DOMs and are descendants of `element` or the document
export function list_focusables (element)
{
  element = element || document.body;
  const candidates = [
    'a[href]',
    'audio[controls]',
    'button',
    'input:not([type="radio"]):not([type="hidden"])',
    'select',
    'textarea',
    'video[controls]',
    '[contenteditable]:not([contenteditable="false"])',
    '[tabindex]',
  ];
  const excludes = ':not([disabled]):not([tabindex="-1"])';
  const candidate_selector = candidates.map (e => e + excludes).join (', ');
  const is_focusable = e => {
    // skip invisible elements
    if (e.offsetWidth <= 0 && e.offsetHeight <= 0 &&            // 0x0 elements are still focusable
	Util.inside_display_none (e))                           // but not within display:none
      return false;
    // skip not focusable elements
    if (!e.matches (candidate_selector))
      return false;
    return true;                                                // node should be focusable
  };
  const l = [], d = [];
  // find focusable elements, recursively piercing into shadow DOMs
  collect_focusables (element, l, d, is_focusable);
  if (!d.length)
    return l;   // no modals, list is done
  // handle modal dialogs
  const inner = d[d.length - 1]; // this better be the innermost modal dialog
  if (Util.has_ancestor (element, inner))
    return l;   // search was confined to this dialog anyway
  // in case inner is part of a shadowRoot, find 'descendant' assigned elements
  let droots = [ inner ];
  for (const slot of inner.querySelectorAll ('slot'))
    droots.push (...slot.assignedNodes ({ flatten:true }));
  // constrain results to modal dialog
  const is_droot_descendant = e => {
    for (const r of droots)
      if (Util.has_ancestor (e, r))
	return true;
    return false;
  };
  return l.filter (is_droot_descendant);
}

/** Install a FocusGuard to allow only a restricted set of elements to get focus. */
class FocusGuard {
  defaults() { return {
    updown_focus: true,
    updown_cycling: false,
    focus_root_list: [],
    last_focus: undefined,
  }; }
  constructor () {
    Object.assign (this, this.defaults());
    window.addEventListener ('focusin', this.focusin_handler.bind (this), true);
    window.addEventListener ('keydown', this.keydown_handler.bind (this));
    const fe = activeElement();
    if (fe && fe != document.body)
      this.last_focus = fe;
    // Related: https://developer.mozilla.org/en-US/docs/Web/Accessibility/Keyboard-navigable_JavaScript_widgets
  }
  push_focus_root (element, escapecb) {
    const fe = activeElement();
    const current_focus = fe && fe != document.body ? fe : undefined;
    this.focus_root_list.unshift ([ element, current_focus, escapecb ]);
    if (current_focus)
      this.focus_changed (current_focus, false);
  }
  remove_focus_root (element) {
    if (this.last_focus && !this.last_focus.parentElement)
      this.last_focus = undefined;	// cleanup to allow GC
    for (let i = 0; i < this.focus_root_list.length; i++)
      if (this.focus_root_list[i][0] === element)
	{
	  const saved_focus = this.focus_root_list[i][1];
	  this.focus_root_list.splice (i, 1);
	  if (saved_focus)
	    saved_focus.focus(); // try restoring focus
	  return true;
	}
    return false;
  }
  focusin_handler (event) {
    return this.focus_changed (event.target);
  }
  focus_changed (target, refocus = true) {
    const fe = activeElement();
    if (this.focus_root_list.length == 0 || !fe || fe == document.body)
      return false; // not interfering
    const focuslist = list_focusables (this.focus_root_list[0][0]);
    const idx = focuslist.indexOf (target);
    if (idx >= 0 || // element found in our detected focus order
	Util.has_ancestor (fe, this.focus_root_list[0][0])) // possibly customElement
      this.last_focus = fe;
    else // invalid element gaining focus
      {
	fe.blur();
	if (refocus && focuslist.length)
	  {
	    const lastidx = focuslist.indexOf (this.last_focus);
	    let newidx = 0;
	    if (lastidx >= 0 && lastidx < focuslist.length / 2)
	      newidx = focuslist.length - 1;
	    focuslist[newidx].focus();
	  }
	return true;
      }
    return false; // not interfering
  }
  keydown_handler (event) {
    const fe = activeElement();
    if (!fe || fe == document.body) // no focus
      return keydown_move_focus (event);
    else
      return false; // not interfering
  }
}
const the_focus_guard = new FocusGuard();

/** Constrain focus to `element` and its descendants */
export function push_focus_root (element, escapecb) {
  the_focus_guard.push_focus_root (element, escapecb);
}

/** Remove an `element` previously installed via push_focus_root() */
export function remove_focus_root (element) {
  the_focus_guard.remove_focus_root (element);
}

/** Compute global midpoint position of element, e.g. for focus movement */
function element_midpoint (element) {
  const r = element.getBoundingClientRect();
  return { y: 0.5 * (r.top + r.bottom),
	   x: 0.5 * (r.left + r.right) };
}

/** Move focus on UP/DOWN/HOME/END `keydown` events */
export function keydown_move_focus (event) {
  // constrain focus movements within data-subfocus=1 container
  const fe = activeElement();
  const subfocus = Util.closest (fe, '[data-subfocus]') || document.body;
  const left_right = subfocus.getAttribute ('data-subfocus') === "*";
  let dir;
  if (event.keyCode == KeyCode.HOME)
    dir = 'HOME';
  else if (event.keyCode == KeyCode.END)
    dir = 'END';
  else if (event.keyCode == KeyCode.UP)
    dir = -1;
  else if (event.keyCode == KeyCode.DOWN)
    dir = +1;
  else if (left_right && event.keyCode == KeyCode.LEFT)
    dir = 'LEFT';
  else if (left_right && event.keyCode == KeyCode.RIGHT)
    dir = 'RIGHT';
  else
    return false;
  return move_focus (dir, subfocus);
}

/** Move focus to prev or next focus widget */
export function move_focus (dir = 0, subfocus = null) {
  const home = dir == 'HOME', end = dir == 'END';
  const up = dir == -1, down = dir == +1;
  const left = dir == 'LEFT', right = dir == 'RIGHT';
  const updown_focus = the_focus_guard.updown_focus;
  const updown_cycling = the_focus_guard.updown_cycling;
  const last_focus = the_focus_guard.last_focus;
  if (!(home || up || down || end || left || right))
    return false; // nothing to move
  const root = subfocus || the_focus_guard.focus_root_list[0]?.[0] || document.body;
  if (!root || !updown_focus ||
      // is_nav_input (fe) || (fe.tagName == "INPUT" && !is_button_input (fe)) ||
      !(up || down || home || end || left || right))
    return false; // not interfering
  const focuslist = list_focusables (root);
  if (!focuslist)
    return false; // not interfering
  const fe = activeElement();
  let idx = focuslist.indexOf (fe);
  if (idx < 0 && (up || down))
    { // re-focus last element if possible
      idx = focuslist.indexOf (last_focus);
      if (idx >= 0)
	{
	  focuslist[idx].focus();
	  return true;
	}
    }
  let next; // position to move new focus to
  if (idx < 0)
    next = (down || home) ? 0 : focuslist.length - 1;
  else if (home || end)
    next = home ? 0 : focuslist.length - 1;
  else if (left || right)
    {
      const idx_pos = element_midpoint (focuslist[idx]);
      let dist = { x: 9e99, y: 9e99 };
      for (let i = 0; i < focuslist.length; i++)
	if (i != idx)
	  {
	    const pos = element_midpoint (focuslist[i]);
	    const d = { x: pos.x - idx_pos.x, y: pos.y - idx_pos.y };
	    if ((right && d.x > 0) || (left && d.x < 0))
	      {
		d.x = Math.abs (d.x);
		d.y = Math.abs (d.y);
		if (d.x < dist.x || (d.x == dist.x && d.y < dist.y))
		  {
		    dist = d;
		    next = i;
		  }
	      }
	  }
    }
  else // up || down
    {
      next = idx + (up ? -1 : +1);
      if (updown_cycling)
	{
	  if (next < 0)
	    next += focuslist.length;
	  else if (next >= focuslist.length)
	    next -= focuslist.length;
	}
      else if (next < 0 || next >= focuslist.length)
	next = undefined;
    }
  if (next >= 0 && next < focuslist.length)
    {
      focuslist[next].focus();
      return true;
    }
  return false;
}

/** Forget the last focus element inside `element` */
export function forget_focus (element) {
  if (!the_focus_guard.last_focus)
    return;
  let l = the_focus_guard.last_focus;
  while (l)
    {
      if (l == element)
	{
	  the_focus_guard.last_focus = undefined;
	  return;
	}
      l = l.parentNode;
    }
}

const hotkey_list = [];
const filter_keycodes = {};

function hotkey_handler (event) {
  const kdebug = () => undefined; // kdebug = debug;
  const focus_root = the_focus_guard?.focus_root_list?.[0];
  const fe = activeElement();
  kdebug ("hotkey_handler:", "focus:", fe, event);
  if (!fe)
    return false;
  // avoid composition events, https://developer.cdn.mozilla.net/en-US/docs/Web/API/Element/keydown_event
  if (event.isComposing || event.keyCode === 229)
    {
      kdebug ("hotkey_handler: ignore-composition: " + event.code + ' (' + fe.tagName + ')');
      return false;
    }
  // allow filtering via keyCode
  if (filter_keycodes[event.keyCode])
    {
      const filtered = filter_keycodes[event.keyCode] (event);
      kdebug ("hotkey_handler: filtering:", event.keyCode, !!filtered);
      if (filtered)
	return true;
    }
  // allow ESCAPE callback for focus_root
  if (focus_root && focus_root[2] && Util.match_key_event (event, 'Escape'))
    {
      const escapecb = focus_root[2];
      if (false === escapecb (event))
	event.preventDefault();
      return true;
    }
  // give precedence to navigatable element with focus
  if (is_nav_input (fe) ||
      (fe.tagName == "INPUT" && !is_button_input (fe)))
    {
      kdebug ("hotkey_handler: ignore-nav: " + event.code + ' (' + fe.tagName + ')');
      return false;
    }
  // activate focus via Enter
  if (Util.match_key_event (event, 'Enter') && fe != document.body)
    {
      kdebug ("hotkey_handler: button-activation: " + ' (' + fe.tagName + ')');
      // allow `onclick` activation via `Enter` on <button/> and <a href/> with event.isTrusted
      const click_pending = fe.nodeName == 'BUTTON' || (fe.nodeName == 'A' && fe.href);
      Util.keyboard_click (fe, event, !click_pending);
      if (!click_pending)
	event.preventDefault();
      return true;
    }
  // restrict global hotkeys during modal dialogs
  const modal_element = !!(document._b_modal_shields?.[0]?.root || focus_root);
  // activate global hotkeys
  for (const keymap of global_keymaps)
    for (const entry of keymap)
      if (entry instanceof KeymapEntry)
	{
	  if (match_key_event (event, entry.key) &&
	      (!modal_element || Util.has_ancestor (entry.element, modal_element)))
	    {
	      if (entry.handler && entry.handler (event) != false)
		return true; // handled
	    }
	}
  // activate global hotkeys
  const array = hotkey_list;
  for (let i = 0; i < array.length; i++)
    if (match_key_event (event, array[i][0]))
      {
	const subtree_element = array[i][2];
	if (modal_element && !Util.has_ancestor (subtree_element, modal_element))
	  continue;
	const callback = array[i][1];
	event.preventDefault();
	kdebug ("hotkey_handler: hotkey-callback: '" + array[i][0] + "'", callback.name, "PREVENTED:", event.defaultPrevented);
	callback.call (null, event);
	return true;
      }
  // activate elements with data-hotkey=""
  const hotkey_elements = document.querySelectorAll ('[data-hotkey]:not([disabled])');
  for (const el of hotkey_elements)
    if (match_key_event (event, el.getAttribute ('data-hotkey')))
      {
	if (modal_element && !Util.has_ancestor (el, modal_element))
	  continue;
	event.preventDefault();
	kdebug ("hotkey_handler: keyboard-click2: '" + el.getAttribute ('data-hotkey') + "'", el);
	Util.keyboard_click (el, event);
	return true;
      }
  kdebug ('hotkey_handler: unused-key: ' + event.code + ' ' + event.which + ' ' + event.charCode + ' (' + fe.tagName + ')');
  return false;
}
window.addEventListener ('keydown', hotkey_handler, { capture: true });

/// Add a global hotkey handler.
export function add_hotkey (hotkey, callback, subtree_element = undefined) {
  hotkey_list.push ([ hotkey, callback, subtree_element ]);
}

/// Remove a global hotkey handler.
export function remove_hotkey (hotkey, callback) {
  const array = hotkey_list;
  for (let i = 0; i < array.length; i++)
    if (hotkey === array[i][0] && callback === array[i][1])
      {
	array.splice (i, 1);
	return true;
      }
  return false;
}

export class KeymapEntry {
  constructor (key = '', handler = null, element = null)
  {
    this.key = key;
    this.handler = handler;
    this.element = element;
  }
}

/// Add a global keymap.
export function add_key_filter (keycode, callback)
{
  filter_keycodes[keycode] = callback;
}

/// Remove a global keymap.
export function remove_key_filter (keycode)
{
  delete filter_keycodes[keycode];
}

const global_keymaps = [];

/// Add a global keymap.
export function add_keymap (keymap)
{
  Util.array_remove (global_keymaps, keymap);
  global_keymaps.push (keymap);
}

/// Remove a global keymap.
export function remove_keymap (keymap)
{
  Util.array_remove (global_keymaps, keymap);
}

/** Check if `element` is button-like input */
export function is_button_input (element) {
  const click_elements = [
    "BUTTON",
    "SUMMARY",
  ];
  if (Util.in_array (element.tagName, click_elements))
    return true;
  const button_input_types = [
    'button',
    'checkbox',
    'color',
    'file',
    // 'hidden',
    'image',
    'radio',
    'reset',
    'submit',
  ];
  if (element.tagName == "INPUT" && Util.in_array (element.type, button_input_types))
    return true;
  return false;
}

/** Check if `element` has inner input navigation */
export function is_nav_input (element) {
  const nav_elements = [
    "AUDIO",
    "SELECT",
    "TEXTAREA",
    "VIDEO",
    "EMBED",
    "IFRAME",
    "OBJECT",
    "APPLET",
  ];
  if (Util.in_array (element.tagName, nav_elements))
    return true;
  const nav_input_types = [
    'date',
    'datetime-local',
    'email',
    'month',
    'number',
    'password',
    'range',
    'search',
    'tel',
    'text',
    'time',
    'url',
    'week',
  ];
  if (element.tagName == "INPUT" && Util.in_array (element.type, nav_input_types))
    return true;
  return false;
}


let shortcut_map = new Map();
const MS = '\x1F'; // Menu Separator

/// Lookup shortcut for `label` in `mapname`, default to `shortcut`
export function shortcut_lookup (mapname, label, shortcut) {
  const path = mapname + MS + label;
  const assigned = shortcut_map.get (path);
  return assigned !== undefined ? assigned : shortcut;
}

const shortcut_keydown_hotkey = { prev: undefined, next: undefined };

function shortcut_keydown (div, event) {
  const shortcut = Util.hotkey_name_from_event (event);
  if (shortcut === 'Enter' || shortcut === 'Return')
    return;
  event.preventDefault();
  event.stopPropagation();
  if (shortcut === 'Backspace' || shortcut === 'Delete')
    shortcut_keydown_hotkey.next = '';
  else
    shortcut_keydown_hotkey.next = shortcut || shortcut_keydown_hotkey.prev;
  div.innerText = Util.display_keyname (shortcut_keydown_hotkey.next || '');
}

/// Display shortcut editing dialog
export async function shortcut_dialog (mapname, label, shortcut) {
  shortcut_keydown_hotkey.prev = shortcut;
  shortcut_keydown_hotkey.next = shortcut_lookup (mapname, label, shortcut) || shortcut_keydown_hotkey.prev;
  const title = "Assign Shortcut";
  const html = "Enter Shortcut for:\n" +
	       "<b>" + Util.escape_html (label) + "</b>\n";
  let shortcut_keydown_ = undefined;
  const div_handler = (div, dialog) => {
    div.innerText = Util.display_keyname (shortcut_keydown_hotkey.next || '');
    if (!shortcut_keydown_) {
      shortcut_keydown_ = shortcut_keydown.bind (null, div);
      window.addEventListener ('keydown', shortcut_keydown_, { capture: true });
    }
  };
  const destroy = () => {
    window.removeEventListener ('keydown', shortcut_keydown_, { capture: true });
  };
  let v = App.async_modal_dialog ({ title, html, div_handler, destroy,
				    class: "b-shortcut-dialog",
				    buttons: [ 'Cancel', { label: "OK", autofocus: true } ],
				    emblem: "KEYBOARD" });
  v = await v;
  // TODO: pick 'OK' on Enter
  const assigned = v === 1 ? shortcut_keydown_hotkey.next : undefined;
  shortcut_keydown_hotkey.prev = undefined;
  shortcut_keydown_hotkey.next = undefined;
  if (assigned === undefined)
    return false;
  const path = mapname + MS + label;
  shortcut_map.set (path, assigned); // TODO: delete defaults
  if (Ase.server)
    Ase.server.set_data ('shortcuts.json', JSON.stringify ([...shortcut_map], null, 2));
  return true;
}

import * as Ase from '../aseapi.js';
(async () => {
  await Ase.server;
  const mapjson = await Ase.server.get_data ('shortcuts.json');
  if (mapjson)
    shortcut_map = new Map (JSON.parse (mapjson));
}) ();
