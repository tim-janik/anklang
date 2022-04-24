// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

// Little helpers for LitElement components

export * from '../lit.js';
import { LitElement, html, css, unsafeCSS } from '../lit.js';

export const docs = (...args) => undefined;

// == PostCSS ==
const postcssjs = require ('postcss');		// browserified
import postcss_config from './postcss.esm.js';	// require()s browserified plugins
const postcss_processor = postcssjs (postcss_config);

/// Process CSS via PostCSS, uses async plugins.
export async function postcss_process (css_string, fromname = '<style string>') {
  const options = Object.assign ({ from: fromname }, postcss_config);
  try {
    return await postcss_processor.process (css_string, options);
  } catch (ex) {
    console.warn ('CSS error in:', fromname + '\n', css_string);
    console.error ('PostCSS error:', ex);
    return '';
  }
}

export async function postcss (...args) {
  const css_string = args.join ('');
  const result = await postcss_process (css_string, "css`` literal");
  return css`${unsafeCSS (result)}`;
}

//export const css_import = PC.css_import;
/* import all_cssfiles from '/all-cssfiles.js';
for (const filename of Object.keys (all_cssfiles))
  ; // css_import (filename, all_cssfiles[filename]);
*/
