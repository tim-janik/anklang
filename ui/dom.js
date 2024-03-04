// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
'use strict';

// Check if a particular font is present (and deviates from the fallback font)
export function font_family_loaded (options = {})
{
  const defaults = { font_family: 'emoji', text: 'abc_0123' };
  options = Object.assign (defaults, options);
  const el = document.createElement ('span');
  el.style.margin = 0;
  el.style.padding = 0;
  el.style.border = 0;
  el.style.fontSize = 'initial';
  el.style.display = 'inline';
  el.style.visibility = 'hidden';
  el.style.position = 'absolute';
  el.style.top = '-9999px';
  el.style.left = '-9999px';
  el.style.fontSize = '99px';
  el.innerHTML = options.text;
  // compare with sans-serif size
  el.style.fontFamily = 'sans-serif';
  document.body.appendChild (el);
  const sans_offset_width = el.offsetWidth, sans_offset_height = el.offsetHeight;
  el.style.fontFamily = '"'+options.font_family+'"' + ', sans-serif';
  let font_offset_width = el.offsetWidth, font_offset_height = el.offsetHeight;
  document.body.removeChild (el);
  if (sans_offset_width != font_offset_width || sans_offset_height != font_offset_height)
    return true;
  // compare with monospace size
  el.style.fontFamily = 'monospace';
  document.body.appendChild (el);
  const monospace_offset_width = el.offsetWidth, monospace_offset_height = el.offsetHeight;
  el.style.fontFamily = '"'+options.font_family+'"' + ', monospace';
  font_offset_width = el.offsetWidth; font_offset_height = el.offsetHeight;
  document.body.removeChild (el);
  if (monospace_offset_width != font_offset_width || monospace_offset_height != font_offset_height)
    return true;
  return false;
}

/// Fetch URI from a DOM element, returns undefined if none is found (e.g. Number(0) is a valid URI).
export function get_uri (element)
{
  if (!element) return undefined;
  let uri = element['uri'];
  if (uri === undefined || uri === null || uri === '')
    uri = !element.getAttribute ? undefined : element.getAttribute ('uri');
  if (uri === undefined || uri === null || uri === '')
    return undefined;
  return uri;
}

/// Check if URI is not undefined.
export function valid_uri (uri)
{
  return uri !== undefined;
}

/// Check if DOM element has valid URI.
export function has_uri (element)
{
  return valid_uri (get_uri (element));
}
