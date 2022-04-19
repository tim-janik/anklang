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
  const rex = new RegExp (/\s*[+]\s*/); // Split 'Shift+Ctrl+Alt+Meta+SPACE'
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
