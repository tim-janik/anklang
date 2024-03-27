// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import * as Wrapper from './wrapper.js';

// Export supported Lit constructs
export { noChange, nothing, render, html, css, unsafeCSS } from 'lit';
export { ref } from 'lit/directives/ref';
export { live } from 'lit/directives/live';
export { repeat } from 'lit/directives/repeat';
import { LitElement, css, unsafeCSS } from 'lit';

export const docs = (...args) => undefined;

// == LitComponent ==
/** @class LitComponent
 * @description
 * An interface extending LitElement with reactive render() and updated() methods.
 * ### Props:
 * *render*
 * : A pre-bound Wrapper.reactive_wrapper() around LitElement.render().
 * *updated*
 * : A pre-bound Wrapper.reactive_wrapper() around LitElement.updated().
 * *request_update_*
 * : A pre-bound wrapper around LitElement.requestUpdate().
 */
export class LitComponent extends LitElement {
  constructor()
  {
    super();
    this.request_update_ = this.requestUpdate.bind (this);
    // Use cast to hide assignment causing instance member property shadowing instance member function (TS2425)
    (/**@type{any}*/ (this)).render = Wrapper.reactive_wrapper (this.render.bind (this), this.request_update_);
    (/**@type{any}*/ (this)).updated = Wrapper.reactive_wrapper (this.updated.bind (this), this.request_update_);
  }
  createRenderRoot()
  {
    const render_root = super.createRenderRoot();
    adopt_component_styles (render_root);
    return render_root;
  }
  get weak_this()
  {
    let wt = this[sym_weak_this];
    return wt || (this[sym_weak_this] = new WeakRef (this));
  }
}
const sym_weak_this = Symbol ('weak_this');

// == lit_update_all ==
/** Call requestUpdate() on all `LitElement`s */
export function lit_update_all (root)
{
  root = root || document.body;
  for (const el of root.querySelectorAll ('*'))
    {
      if (el instanceof LitElement)
	el.requestUpdate();
      if (el.shadowRoot)
	lit_update_all (el.shadowRoot);
    }
}

// == JsExtract ==
/** API to mark template strings for extraction and fetch extracted assets.
 * - ``` JsExtract.css`body { font-weight:bold; }` ``` \
 *   Mark CSS text inside a Javascript file for extration by jsextract.js.
 * - ``` node jsextract.js inputfile.js ``` \
 *   Extract ``` JsExtract.css`` ``` strings into `inputfile.jscss`.
 * - ``` await JsExtract.fetch_css ('/path/to/inputfile.js'); ``` \
 *   Use the browser `fetch()` API to retrieve the extracted string as `text/css`
 *   from '/path/to/inputfile.css'.
 * - ``` JsExtract.fetch_css (import.meta); ``` \
 *   Variant of the above that utilizes `import.meta.url`.
 * - ``` JsExtract.css_url (import.meta); ``` \
 *   Variant of the above that just provides the stylesheet URL.
 */
export const JsExtract = {
  // Mark CSS template string for jsextract.js
  css: () => undefined,
  // Mark SCSS template string for jsextract.js
  scss: (strings, ...values) => undefined,
  // Mark HTML template string for jsextract.js
  html: () => undefined,
  css_url,
  fetch_css,
};

/// Construct the stylesheet URL from a base URL (enforcing `.css` extension).
async function css_url (base_url)
{
  const url = base_url?.url || base_url;
  const css_url = url.replace (/\.[^.\/]+$/, '') + '.css';
  return css_url;
}

/// Fetch (extracted) asset from a base URL (enforcing `.css` extension) as "text/css"
async function fetch_css (base_url)
{
  const url = base_url?.url || base_url;
  const css_url = url.replace (/\.[^.\/]+$/, '') + '.css';
  const csstext = await fetch_text_css (css_url);
  return css`${unsafeCSS (csstext)}`;
}

async function fetch_text_css (urlstr)
{
  const url = new URL (urlstr);
  const fresponse = await fetch (url);
  if (fresponse.ok) {
    const content_type = fresponse.headers.get ("content-type");
    if (content_type && content_type.includes ("text/css")) {
      const blob = await fresponse.blob(), text = await blob.text();
      return text;
    }
    console.error ('Invalid file type for CSS import:', url + ', Content-Type:', content_type);
    return '';
  }
  console.error ('Failed to load CSS import:', url + '');
  return '';
}

/// Ensure that element has all stylesheets with [data-4litcomponent] applied to it
function adopt_component_styles (element)
{
  /* Apply stylesheets from document to shadowRoot. Notes on support and flash of unstyled content (FOUC):
   * - Loading of a <link rel=stylesheet> href does *not* block rendering in shadowRoots, leading to FOUC if uncached.
   *   https://github.com/Polymer/polymer/issues/4865#issuecomment-351799229
   * - In Chrome, even a permanently cached link href is still re-requested every few seconds for newly created shadowRoot
   *   links, leading to FOUC even if cached.
   * - Loading a stylesheet via XHR and adopting it in the shadowRoot as a constructable stylesheet avoids FOUC, but it prevents
   *   DevTools interpretation of source maps, potentially duplicates contents in the browser and does not support @import.
   *   https://github.com/WICG/construct-stylesheets/issues/119#issuecomment-588352418
   * - Using a <style type="text/css" data-4litcomponent>@import</style> element on the document will allow
   *   easy cloning into a shadowRoot, and seems to not cause any FOUC due to uncached resources or refetching.
   */
  const root_element = element.getRootNode();
  if (root_element === document) return;
  const root = /**@type{ShadowRoot}*/ (root_element);	// make ts-check happy
  // apply styles by cloning [data-4litcomponent] elements from head into the shadowRoot
  for (const ele of document.head.querySelectorAll ('[data-4litcomponent]'))
    root.appendChild (ele.cloneNode (true));
}
