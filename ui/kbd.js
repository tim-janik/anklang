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
