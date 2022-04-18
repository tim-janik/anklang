// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
'use strict';

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
