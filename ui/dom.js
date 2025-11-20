// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
'use strict';

import MarkdownIt from 'markdown-it';

/** Generate `element.innerHTML` from `markdown_text` */
export function markdown_to_html (element, markdown_text) {
  // configure Markdown generator
  const config = { linkify: true };
  const md = new MarkdownIt (config);
  // add target=_blank to all links
  const orig_link_open = md.renderer.rules.link_open || function (tokens, idx, options, env, self) {
    return self.renderToken (tokens, idx, options); // default renderer
  };
  md.renderer.rules.link_open = function (tokens, idx, options, env, self) {
    const aIndex = tokens[idx].attrIndex ('target'); // attribute could be present already
    if (aIndex >= 0)
      tokens[idx].attrs[aIndex][1] = '_blank';       // override when present
    else
      tokens[idx].attrPush (['target', '_blank']);   // or add new attribute
    return orig_link_open (tokens, idx, options, env, self); // resume
  };
  // render HTML
  const html = md.render (markdown_text);
  element.classList.add ('b-markdown-it-outer');
  element.innerHTML = html;
}

/// Fetch URI from a DOM element, returns undefined if none is found (e.g. Number(0) is a valid URI).
export function get_uri (element)
{
  if (!element) return undefined;
  let uri = element['uri'];
  if (uri === undefined || uri === null || uri === '')
    uri = !element.getAttribute ? undefined : element.getAttribute ('uri');
  if (uri === undefined || uri === null || uri === '')
    return get_uri (element.parentElement);
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

/// Get `.textContent` with or without children from a DOM element.
export function text_content (element, with_children = true)
{
  if (with_children) return element.textContent;
  let s = '';
  for (let i = 0; i < element.childNodes.length; ++i)
    if (element.childNodes[i].nodeType === Node.TEXT_NODE)
      s += element.childNodes[i].textContent;
  return s;
}

/// Get RGBA values from CSS color function.
function rgba_from_css (color)
{
  if (!rgba_from_css.ctx) {
    const offscreen = new OffscreenCanvas (1, 1);
    rgba_from_css.ctx = offscreen.getContext ('2d');
    if (!rgba_from_css.ctx)
      throw new Error ('2d context for OffscreenCanvas not supported');
  }
  const ctx = rgba_from_css.ctx;
  ctx.fillStyle = color;
  ctx.fillRect (0, 0, 1, 1);
  const data = ctx.getImageData (0, 0, 1, 1).data;
  return { r: data[0], g: data[1], b: data[2], a: data[3] };
}

/// Get RGB hex value from CSS color function.
function rgbhex_from_css (color)
{
  const { r, g, b } = rgba_from_css (color);
  const hex2 = v => v.toString (16).padStart (2, '0');
  return `#${hex2 (r)}${hex2 (g)}${hex2 (b)}`;
}

/// Show a `dialog` via showModal() and close it on backdrop clicks.
export function show_modal (dialog, closefunc = null)
{
  if (dialog.open) return;
  closefunc = closefunc || (() => dialog.close());
  // close dialog on backdrop clicks, but:
  // - avoid matching text-select drags that end up on backdrop area
  // - avoid matching Enter-click event coordinates from input-submit with clientX*clientY==0
  // - avoid matching an outside popup click, after a previous pointerdown+Escape combination
  // - avoid re-popup by clicking on outside menu button and closing early on pointerdown
  let pointer_outside = false; // must reset on every dialog.showModal()
  const pointerdown = event => {
    pointer_outside = (event.buttons && event.target === dialog && // backdrop has target==dialog
		       (event.offsetX < 0 || event.offsetX >= event.target.offsetWidth ||
			event.offsetY < 0 || event.offsetY >= event.target.offsetHeight));
  };
  const escapecloses = event => {
    if (event.key === 'Escape') {
      event.preventDefault();
      closefunc();
    }
  };
  const pointerup = event => {
    if (pointer_outside && event.target === dialog && // backdrop as target is dialog
	(event.offsetX < 0 || event.offsetX >= event.target.offsetWidth ||
	 event.offsetY < 0 || event.offsetY >= event.target.offsetHeight))
      closefunc();
    else
      pointer_outside = false;
  };
  const mousedown = event => {
    // prevent focus on the dialog itself
    if (event.buttons)
      event.preventDefault();
  };
  const capture = { capture: true };
  const close = event => {
    dialog.removeEventListener ('pointerdown', pointerdown, capture);
    dialog.removeEventListener ('pointerup', pointerup);
    dialog.removeEventListener ('mousedown', mousedown);
    dialog.removeEventListener ('keydown', escapecloses);
    dialog.removeEventListener ('close', close);
  };
  dialog.addEventListener ('pointerdown', pointerdown, capture);
  dialog.addEventListener ('pointerup', pointerup);
  dialog.addEventListener ('mousedown', mousedown);
  dialog.addEventListener ('keydown', escapecloses);
  dialog.addEventListener ('close', close);
  dialog.showModal();
  return dialog;
}
