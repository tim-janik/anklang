// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

// Little helpers for LitElement components

export * from '../lit.js';
import { LitElement, html, css, unsafeCSS } from '../lit.js';

export const docs = (...args) => undefined;

// == PostCSS ==
import * as PostCss from './postcss.js';	// require()s browserified plugins
const csstree_validator = __DEV__ && await import ('./csstree-validator.esm.js');

/// Process CSS via PostCSS, uses async plugins.
export async function postcss_process (css_string, fromname = '<style string>', validate = true) {
  const result = await PostCss.postcss_process (css_string, fromname);
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

const imports_done = memorize_imports ([ 'theme.scss', 'mixins.scss', 'shadow.scss', 'cursors/cursors.css' ]);

export async function postcss (...args) {
  const css_string = args.join ('');
  await imports_done;
  const result = await postcss_process (css_string, "literal-css``");
  return css`${unsafeCSS (result)}`;
}

async function memorize_imports (imports) {
  const fetchoptions = {};
  const fetchlist = imports.map (async filename => {
    const cssfile = await fetch (filename, fetchoptions);
    const csstext = await cssfile.text();
    // debug ("memorize_imports:", filename, cssfile, csstext);
    PostCss.add_import (filename, csstext);
  });
  await Promise.all (fetchlist);
}
