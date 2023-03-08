// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

// Little helpers for LitElement components

export * from '../lit.js';
import { LitElement, html, css, unsafeCSS } from '../lit.js';

export const docs = (...args) => undefined;

// Provide StyleSheet for all LitComponents
const shadowdom_stylesheet = new CSSStyleSheet();
(async () => {
  const cssfile = 'shadow.css';
  shadowdom_stylesheet[cssfile] = cssfile;
  const csstext = await postcss_process ("@import url('" + cssfile + "');");
  shadowdom_stylesheet.replaceSync (csstext);
}) ();

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
    render_root.adoptedStyleSheets.unshift (shadowdom_stylesheet);
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

// == PostCSS ==
import * as PostCss from './postcss.js';	// require()s browserified plugins
const csstree_validator = __DEV__ && await import ('./csstree-validator.esm.js');

/// Process CSS via PostCSS, uses async plugins.
export async function postcss_process (css_string, fromname = '<style string>', validate = true) {
  const result = await PostCss.postcss_process (css_string, fromname, { import_all: true });
  if (__DEV__ && validate) {
    const errs = csstree_validator.validate (result, "input.postcss");
    if (errs.length) {
      console.warn ('PostCSS output:', fromname + ':\n', css_string);
      console.error (fromname + ':' + errs[0].line + ': ' + errs[0].name + ': ' + errs[0].message + ': ' + errs[0].css + '\n', errs);
      console.info (result);
    }
  }
  return result;
}

export async function postcss (...args) {
  const css_string = args.join ('');
  const result = await postcss_process (css_string, "literal-css``");
  return css`${unsafeCSS (result)}`;
}
