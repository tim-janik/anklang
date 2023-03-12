// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

// Little helpers for LitElement components

export * from '../lit.js';
import { LitElement, html, css, unsafeCSS } from '../lit.js';

export const docs = (...args) => undefined;

// == LitComponent ==
/// A LitElement with reactive render() and updated() methods.
export class LitComponent extends LitElement {
  constructor()
  {
    super();
    const request_update = this.requestUpdate.bind (this);
    this.render = Util.reactive_wrapper (this.render.bind (this), request_update);
    this.updated = Util.reactive_wrapper (this.updated.bind (this), request_update);
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
export const JsExtract = {
  css: () => undefined,
  scss: () => undefined,
  html: () => undefined,
  fetch_css,
};
async function fetch_css (import_meta_url)
{
  const url = import_meta_url?.url || import_meta_url;
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
