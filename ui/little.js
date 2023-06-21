// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import * as Wrapper from './wrapper.js';

// Export supported Lit constructs
export { noChange, render, html, css, unsafeCSS } from 'lit';
export { ref } from 'lit/directives/ref';
export { live } from 'lit/directives/live';
export { repeat } from 'lit/directives/repeat';
import { LitElement, css, unsafeCSS } from 'lit';

export const docs = (...args) => undefined;

// == LitComponent ==
/// A LitElement with reactive render() and updated() methods.
export class LitComponent extends LitElement {
  constructor()
  {
    super();
    const request_update = this.requestUpdate.bind (this);
    // Use cast to hide assignment causing instance member property shadowing instance member function (TS2425)
    (/**@type{any}*/ (this)).render = Wrapper.reactive_wrapper (this.render.bind (this), request_update);
    (/**@type{any}*/ (this)).updated = Wrapper.reactive_wrapper (this.updated.bind (this), request_update);
  }
  createRenderRoot()
  {
    const render_root = super.createRenderRoot();
    if (render_root) {
      const link = document.createElement ("link");
      link.setAttribute ("rel", "stylesheet");
      link.setAttribute ("href", "shadow.css");
      render_root.appendChild (link);
    }
    return render_root;
  }
}

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
 */
export const JsExtract = {
  // Mark CSS template string for jsextract.js
  css: () => undefined,
  // Mark SCSS template string for jsextract.js
  scss: (strings, ...values) => undefined,
  // Mark HTML template string for jsextract.js
  html: () => undefined,
  fetch_css,
};

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
