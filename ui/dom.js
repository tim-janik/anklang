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
  let closed = false; // idempotency guard — prevent double closefunc() (onCleanup + native close event)
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
      if (!closed) { closed = true; closefunc(); }
    }
  };
  const pointerup = event => {
    if (pointer_outside && event.target === dialog && // backdrop as target is dialog
	(event.offsetX < 0 || event.offsetX >= event.target.offsetWidth ||
	 event.offsetY < 0 || event.offsetY >= event.target.offsetHeight))
      if (!closed) { closed = true; closefunc(); }
    else
      pointer_outside = false;
  };
  const mousedown = event => {
    // prevent focus on the dialog backdrop itself (not its children)
    if (event.buttons && event.target === dialog)
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

/// Collect text from an element, recursing into shadow DOM.
/// TODO: remove shadow DOM recursion after Lit→SolidJS migration completes
function ui_get_text (el)
{
  let text = el.textContent;
  if (el.shadowRoot) {
    for (const child of el.shadowRoot.children)
      text += ' ' + ui_get_text (child);
  }
  return text.trim ();
}

/// Build a CSS selector from tag name and attribute filters.
/// Non-text filters become [attr="value"] selectors.
/// Text filter is applied in JS (no CSS :contains()).
function ui_build_selector (tag, filters)
{
  const { text, ...attrs } = filters;
  const selector = tag + Object.entries (attrs)
    .map (([k, v]) => `[${k}="${String (v).replace (/"/g, '\\"')}"]`)
    .join ('');
  return { selector, text };
}

/// Find the first element matching tag + filters.
/// Dom.ui_find('button', { uri: 'about' })       — menu item
/// Dom.ui_find('b-trackview', { text: 'Drums' }) — track by displayed name
/// Dom.ui_find('button', { text: /close/i })     — button by text (regex)
export function ui_find (tag, filters = {})
{
  const { selector, text } = ui_build_selector (tag, filters);
  const candidates = Array.from (document.querySelectorAll (selector));
  if (!text) return candidates[0] || null;
  const re = text instanceof RegExp ? text : new RegExp (text, 'i');
  for (const el of candidates) {
    if (re.test (ui_get_text (el))) return el;
  }
  return null;
}

/// List all elements matching tag + filters.
export function ui_list (tag, filters = {})
{
  const { selector, text } = ui_build_selector (tag, filters);
  const candidates = Array.from (document.querySelectorAll (selector));
  if (!text) return candidates;
  const re = text instanceof RegExp ? text : new RegExp (text, 'i');
  return candidates.filter (el => re.test (ui_get_text (el)));
}

/// Poll for an element until found or timeout.
export async function ui_wait_for (tag, filters = {}, timeout_ms = 5000)
{
  const start = Date.now ();
  while (Date.now () - start < timeout_ms) {
    const el = ui_find (tag, filters);
    if (el) return el;
    await new Promise (r => setTimeout (r, 100));
  }
  return null;
}

/// Find + click convenience.
export async function ui_click (tag, filters = {})
{
  const el = ui_find (tag, filters);
  if (el) el.click ();
  return el;
}

/// Find + click + wait convenience.
export async function ui_click_wait (tag, filters = {}, wait_ms = 50)
{
  const el = ui_click (tag, filters);
  if (wait_ms > 0)
    await ui_wait (wait_ms);
  return el;
}

/// Wait for the next `requestAnimationFrame` callback to fire.
export async function ui_next_frame ()
{
  await new Promise (r => requestAnimationFrame (() => r ()));
}

/// Wait for `ms` milliseconds.
export async function ui_wait (ms)
{
  await new Promise (r => setTimeout (r, ms));
}
