// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

// Little helpers for LitElement components

export * from '../lit.js';
import { LitElement, html, css, unsafeCSS } from '../lit.js';

export const docs = (...args) => undefined;

// == PostCSS ==
const postcssjs = require ('postcss');		// browserified
import postcss_config from './postcss.esm.js';	// require()s browserified plugins
const postcss_processor = postcssjs (postcss_config.plugins);

/// Process CSS via PostCSS, uses async plugins.
export async function postcss_process (css_string, fromname = '<style string>') {
  const options = Object.assign ({ from: fromname }, postcss_config);
  let result;
  try {
    result = (await postcss_processor.process (css_string, options)).css;
  } catch (ex) {
    console.warn ('PostCSS input:', fromname + '\n', css_string);
    console.error ('PostCSS error:', ex);
    result = '';
  }
  if (__DEV__) {
    const errs = require ('csstree-validator').validate (result, "input.postcss");
    if (errs.length) {
      console.error ('postcss``:' + errs[0].line + ': ' + errs[0].name + ': ' + errs[0].message + ': ' + errs[0].css + '\n', errs);
      console.info (result);
    }
  }
  return result;
}

const imports_done = memorize_imports ([ 'theme.scss', 'mixins.scss', 'shadow.scss' ]);

export async function postcss (...args) {
  const css_string = args.join ('');
  await imports_done;
  const result = await postcss_process (css_string, "literal-css``");
  return css`${unsafeCSS (result)}`;
}

async function memorize_imports (imports) {
  const fetchoptions = {};
  const fetchlist = imports.map (async filename =>
    postcss_config.add_import (filename, await (await fetch (filename, fetchoptions)).text()));
  await Promise.all (fetchlist);
}

