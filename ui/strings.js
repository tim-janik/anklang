// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check
'use strict';

/// Convert bytes in `s` into 2-digit hex representation.
export function hex (s, prefix = '')
{
  let h = '';
  for (let i = 0; i < s.length; i++)
    h += prefix + s.charCodeAt (i).toString (16);
  return h;
}

/// Map a UTF-16 encoded filename with PUA byte encodings to visible characters.
export function displayfs (utf16pua)
{
  // UTF-16: Map unicode code points 0xEF80..0xEFFF to 0x80..0xFF
  return utf16pua.replace (ef80efff_rx, uc => String.fromCharCode (uc.charCodeAt (0) - 0xef00));
}
// Match UTF-16 encoding of 0xEF80..0xEFFF
const ef80efff_rx = new RegExp (/[\uef80-\uefff]/g);

/// Strip the directory parts of a `path`.
export function basename (path)
{
  return path.replace (/.*\//, '');
}

/// Strip the directory parts of displayfs (`path`).
export function displaybasename (path)
{
  return displayfs (path.replace (/.*\//, ''));
}

/// Strip basename and return only directory parts of a `path`.
export function dirname (path)
{
  return path.replace (/[^\/]*$/, '');
}

/// Strip basename and return only directory parts of a displayfs (`path`).
export function displaydirname (path)
{
  return displayfs (path.replace (/[^\/]*$/, ''));
}

function string_tests()
{
  assert (displaydirname ('/foo/bar') == '/foo/');
  assert (displaybasename ('/foo/bar') == 'bar');
  let s, d;
  s = "\uef80"; d = displayfs (s); assert (d == '\u0080');
  s = "\uefbf"; d = displayfs (s); assert (d == '\u00bf');
  s = "\uefc0"; d = displayfs (s); assert (d == '\u00c0');
  s = "\uefff"; d = displayfs (s); assert (d == '\u00ff');
  if (1) { // UTF-8
    // Map an UTF-8 encoded filename with PUA byte encodings to visible characters.
    function displayfs (utf8pua)
    {
      // Match UTF-8 encoding of 0xEF80..0xEFFF, i.e. \xee\xbe\x80 .. \xee\xbf\xbf
      const eebebf_rx = new RegExp (/\xee[\xbe\xbf][\x80-\xbf]/g);
      // UTF-8: Map UTF-8 encoding of 0xEF80..0xEFFF to 0x80..0xFF, i.e. \xee\xbe\x80 -> \xc2\x80 .. \xee\xbf\xbf -> \xc3\xbf
      return utf8pua.replace (eebebf_rx, match => match[1] == '\xbe' ? '\xc2' + match[2] : '\xc3' + match[2]);
    }
    s = "\xee\xbe\x80"; d = displayfs (s); assert (d == '\xc2\x80');
    s = "\xee\xbe\xbf"; d = displayfs (s); assert (d == '\xc2\xbf');
    s = "\xee\xbf\x80"; d = displayfs (s); assert (d == '\xc3\x80');
    s = "\xee\xbf\xbf"; d = displayfs (s); assert (d == '\xc3\xbf');
  }
}
string_tests();
