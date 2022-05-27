// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
'use strict';

/* PostCSS driver for the web browser and CLI.
 * This is an ESM module that uses require() to load PostCSS CJS modules from
 * node_modules/ or via a browserified window.require implementation.
 */
const __BROWSER__ = !globalThis.process?.versions?.node, __NODE__ = !__BROWSER__;
const __filename = !__NODE__ ? 'postcss.js' : import.meta.url.replace (/^.*?:\/\/\//, '/');
const __MAIN__ = __NODE__ && process.argv[1] == __filename;
const require = globalThis.require || (await import ("module")).createRequire (__filename);

// == imports ==
const FS = __NODE__ && require ('fs');
const chromajs = require ('chroma-js');
const css_color_converter = require ('css-color-converter');
const PostCss = require ('postcss');
const postcss_scss = require ('postcss-scss');
import * as Colors from './colors.js';

// == Plugins ==
const postcss_plugins = [
  require ('postcss-discard-comments') ({ remove: comment => true }),
  require ('postcss-advanced-variables') ({ disable: '@content, @each, @for', // @if, @else, @mixin, @include, @import
					    importResolve: __NODE__ ? load_import : find_import }),
  require ('postcss-color-mod-function') ({ unresolved: 'throw' }),
  require ('postcss-color-hwb'),
  require ('postcss-lab-function'),
  require ('postcss-functions') (css_functions()),
  require ('postcss-nested'),
  require ('postcss-discard-duplicates'),
];
const nodeonly_plugins = __NODE__ && [
  // require ('postcss-preset-env') ({ stage: 3, browsers: 'Firefox >= 90 and chrome >= 90', autoprefixer: false }),
  // require ('cssnano') ({ preset: ['default', { normalizeWhitespace: false }] }),
  require ('perfectionist-dfd') ({ format: 'expanded', indentChar: '\t', indentSize: 1, trimLeadingZero: false }),
];

// == Options ==
export const postcss_options = {
  syntax: postcss_scss,
  map: false,
};

// == Processing ==
async function postcss_process (css_string, fromname = '<style string>') {
  const postcss = PostCss (postcss_plugins);
  const poptions = Object.assign ({ from: fromname }, postcss_options);
  let result;
  try {
    result = await postcss.process (css_string, poptions);
  } catch (ex) {
    console.warn ('PostCSS input:', fromname + ':\n', css_string);
    console.error ('PostCSS error:', ex);
    result = { content: '' };
  }
  return result.content;
}

// == CSS Functions ==
const clamp = (v,l,u) => v < l ? l : v > u ? u : v;

/// Yield [ R, G, B, A ] from `color`.
function color2rgba (color) {
  try {
    const c = css_color_converter.fromString (color);
    const rgba = c.toRgbaArray();
    return rgba; // [ R, G, B, A ]
  } catch (ex) {
    throw new Error ("invalid color: " + color + "\n" + ex);
  }
}

/// Yield `value` as number in `[min..max]`, converts percentages.
function tofloat (value, fallback = NaN, min = -Number.MAX_VALUE, max = Number.MAX_VALUE) {
  if (typeof value === 'string')
    {
      const isperc = value.indexOf ('%') >= 0;
      value = parseFloat (value);
      if (isperc)
	value = min + (max - min) * (0.01 * value);
    }
  value = clamp (value, min, max);
  return isNaN (value) ? fallback : value;
}

const FLOAT_REGEXP = /^([+-]?(?:(?:[1-9][0-9]*|0)(?:\.[0-9]*)?(?:[eE][+-]?[0-9]+)?|\.[0-9]+(?:[eE][+-]?[0-9]+)?))/;

// Provide CSS functions that are used after variable expansion
function css_functions() {
  const color = css_color_converter;
  const functions = {
    info: function (...args) {
      console.info ("Postcss:info:", ...args);
      return ''; // args.join (',');
    },
    lgrey (perc) {
      const f = tofloat (perc, 0, 0, 1);
      const rgb = [ 255*f, 255*f, 255*f ]; // FIXME: need Lab
      return color.fromRgb (rgb).toHexString();
    },
    lcolor (col, perc) {
      const f = tofloat (perc, 0, 0, 1);
      const [r,g,b,a] = color.fromString (col).toRgbaArray();
      const rgba = [ r*f, g*f, b*f, a ]; // FIXME: need Lab
      return color.fromRgba (rgba).toHexString();
    },
    fade (col, perc) { // LESS fade, sets absolute alpha
      const a = tofloat (perc, 1, 0, 1);
      const [r,g,b] = color.fromString (col).toRgbaArray();
      const rgba = [ r, g, b, a ];
      return color.fromRgba (rgba).toHexString();
    },
    lighter (col, perc) {
      const f = 1 + tofloat (perc, 0.1, 0, 1);
      const [r,g,b,a] = color.fromString (col).toRgbaArray();
      const rgba = [ r*f, g*f, b*f, a ]; // FIXME: need Lab
      return color.fromRgba (rgba).toHexString();
    },
    darker (col, perc) {
      const f = 1 - tofloat (perc, 0.1, 0, 1);
      const [r,g,b,a] = color.fromString (col).toRgbaArray();
      const rgba = [ r*f, g*f, b*f, a ]; // FIXME: need Lab
      return color.fromRgba (rgba).toHexString();
    },
    asfactor (val) {
      const f = tofloat (val);
      return '' + f;
    },
    div (dividend, divisor) {
      let [ _z, dz = 0, zrest = '' ] = dividend.trim().split (FLOAT_REGEXP);
      let [ _n, dn = 0, nrest = '' ] = divisor.trim().split (FLOAT_REGEXP);
      const val = dz / dn;
      zrest = zrest.trim();
      nrest = nrest.trim();
      const unit = !nrest ? zrest : zrest + '/' + nrest;
      return val + unit;
    },
    pow (number, exponent) {
      let [ _n, num = 0, _r = '' ] = number.trim().split (FLOAT_REGEXP);
      let [ _e, exp = 0, _s = '' ] = exponent.trim().split (FLOAT_REGEXP);
      return '' + Math.pow (num, exp);
    },
    mix (col1, col2, frac) {
      const rgba1 = color.fromString (col1).toRgbaArray();
      const rgba2 = color.fromString (col2).toRgbaArray();
      const f = frac === undefined ? 0.5 : clamp (parseFloat (frac), 0, 1);
      const rgba = rgba1.map ((v,i) => v * (1-f) + rgba2[i] * f);
      return color.fromRgb (rgba).toHexString();
    },
    'chromatic-color-luminance': function (color, luminance = '', mode = '') {
      const rgba = color2rgba (color); // [ R, G, B, A ]
      if (luminance)
	{
	  if (mode)
	    return 'red'; // rgb2sass (chromajs (rgb).luminance (luminance, mode)._rgb);
	  else
	    return 'blue'; // rgb2sass (chromajs (rgb).luminance (luminance)._rgb);
	}
      else
	return String (chromajs (rgba).luminance());
    },
    'contrast-lighten': function (color, targetratio = 1, pivot = 'SAME') {
      color = chromajs (color2rgba (color));
      targetratio = tofloat (targetratio, 1, 1, 21);
      pivot = pivot === 'SAME' ? color : chromajs (color2rgba (pivot));
      let lnext = color.luminance();
      let cnew = color;
      let currentratio = chromajs.contrast (pivot, cnew);
      let i = 0;
      while (currentratio < targetratio && lnext < 1 && i++ < 99) { // FIXME
	const delta = Math.max (0.000379, (targetratio - currentratio) / 21 / 2);
	lnext = lnext + delta;
	cnew = color.luminance (Math.max (0.0001, lnext));
	currentratio = chromajs.contrast (pivot, cnew);
      }
      return String (cnew);
    },
    /// Darken `$color` until a target contrast ratio is met.
    'contrast-darken': function (color, targetratio = 1, pivot = 'SAME') {
      color = chromajs (color2rgba (color));
      targetratio = tofloat (targetratio, 1, 1, 21);
      pivot = pivot === 'SAME' ? color : chromajs (color2rgba (pivot));
      let lnext = color.luminance();
      let cnew = color;
      let currentratio = chromajs.contrast (pivot, cnew);
      while (currentratio < targetratio && lnext > 0.00015) {
	const delta = clamp ((targetratio - currentratio) / 21 / 2, 0.00045, lnext * 0.7);
	lnext = lnext - delta;
	cnew = color.luminance (Math.max (0.0001, lnext));
	currentratio = chromajs.contrast (pivot, cnew);
      }
      return String (cnew);
    },
    /// Set `channel` of `color` to `value`.
    'chromatic-color-set': function (color, channel, value) {
      color = chromajs (color2rgba (color));
      if (typeof channel === 'string')
	color = color.set (channel, value);
      return String (color);
    },
    zmod: Colors.zmod,
    zhsl: Colors.zhsl,
    zHsl: Colors.zHsl,
    zhsv: Colors.zhsv,
    zHsv: Colors.zHsv,
  };
  functions.darken = functions.darker;
  functions.lighten = functions.lighter;
  return { functions };
}

/// Load import files in nodejs to implement `@import`.
function load_import (id, cwd, opts) {
  for (let path of [ './', cwd ]) {
    const file = path + '/' + id;
    if (FS.existsSync (file)) {
      const input = FS.readFileSync (file);
      return { file, contents: input || '' };
    }
  }
  console.error ("Failed to find CSS import:", id, 'in', cwd);
  throw new Error ("Failed to find CSS import: " + id);
}

/// Provide canned CSS files to use with `@import`.
function find_import (id, cwd, opts) {
  const csstext = css_import_list[id];
  if (csstext === undefined)
    console.error ("Failed to find CSS import:", id, 'in', cwd);
  return new Promise (r => r ({ file: id, contents: csstext || '' }));
}
const css_import_list = {};

/// Provide `filename` as `@import "filename";` source with contents `csstext`.
function add_import (filename, csstext) {
  css_import_list[filename] = '' + csstext;
}

// == self-test (nodejs) ==
const test_rules = {
  '$padpx: 11px; vars { padding: $padpx; }':		'vars{ padding:11px; }',
  '$ivar: 22px; interpolation { margin: #{$ivar}; }':	'interpolation{ margin:22px; }',
  '@mixin foo() { f: "Mf"; } mfoo { @include foo(); }':	'mfoo{ f:"Mf"; }',
  '@media screen { color: var(--text-color) }':		'@media screen{ color:var(--text-color) }',
  'a { color: color-mod(#123456 a(15.5%)); }':		'a{ color:rgba(18, 52, 86, 0.155); }',
  'b { hwbcolor: hwb(90deg, 13%, 70%); }':		'b{ hwbcolor:rgb(55, 77, 33); }',
  'c { colormod: color-mod(#345 lightness(50%)); }':	'c{ colormod:hsl(210, 25%, 50%); }', /* == color-mod(red l(50%)); */
  'l { color: lighten(#abc, 17%); }':			'l{ color:#c6daee; }',
  'd { color: darken(#abc, 50%); }':			'd{ color:#555d66; }',
  'n { &:hover { nested: 1; } }':			'n:hover{ nested:1; }',
  's { color: color-mod(hsla(125, 50%, 50%, .4) saturation(+ 10%) w(- 20%)); }': 's{ color:rgba(0, 204, 17, 0.4); }',
  'h { lchcolor: lch(62% 54 63); }':			'h{ lchcolor:rgb(205, 132, 63);', // match only first part to allow display-p3 extension
  't { --cd-1: contrast-darken(#fff,20.8); }':		'--cd-1:#010101;',
  't { --cd-2: contrast-darken(#fff,1.007); }':		'--cd-2:#fefefe;',
  't { --cd-3: contrast-darken(#abcdef,1.33); }':	'--cd-3:#94b2d0;',
  't { --cl-1: contrast-lighten(#987,2.7); }':		'--cl-1:#e8e4e0;',
  't { --cl-2: contrast-lighten(#000,1.006); }':	'--cl-2:#010101;',
  't { --cl-3: contrast-lighten(#000,1.0075); }':	'--cl-3:#020202;',
  't { --cl-4: contrast-lighten(#000,20.8); }':		'--cl-4:#fefefe;',
  't { --cd-5: div(5,2) div(5s,2) div(5km,2h); }':	'--cd-5:2.5 2.5s 2.5km/h;',
  'tn { font-variant-numeric:tabular-nums; }':		'tn{ font-variant-numeric:tabular-nums; }', // preprocessors must NOT reset font-feature-settings here
  // add_import ('imp.css', '$importval: "imported";');
  // '@import "imp.css"; i { --importval: $importval; }': 'i { --importval: "imported"; }',
};
async function test_css (verbose) {
  test_rules[`--text-color: ${'#ABCDEF'};`] = '--text-color:#ABCDEF;';
  const input = Object.keys (test_rules).join ('\n');
  console.log ('  CHECK   ', __filename);
  const result = await postcss_process (input, 'test_css()');
  if (verbose >= 2)
    console.log (__filename + ': result:', '\n' + result);
  let last = -1, errors = 0;
  for (const e of Object.entries (test_rules))
    {
      const matchpos = result.indexOf (e[1]);
      const cssmatch = matchpos > last;
      console.assert (cssmatch, "CSS test unmatched: " + e[1]);
      errors += !cssmatch;
      if (cssmatch && verbose >= 1)
	console.log ("√", e[1]);
      last = matchpos;
    }
  if (errors) {
    console.log (__filename + ': result:', '\n' + result);
    throw new Error (`${__filename}: self test errors: ` + errors);
  }
}

// == Exports ==
export const options = postcss_options;
export const plugins = postcss_plugins;
export { postcss_process };
export { add_import };
export { test_css };

// == main ==
// Usage: postcss.js <inputfile.css>
// Usage: postcss.js --test [1|0]
if (__MAIN__) {
  async function main (argv) {
    // run unit tests
    if (argv[0] === '--test')
      return test_css (argv[1] | 0);
    // parse options
    const filename = argv[0];
    if (!filename)
      throw new Error (`${__filename}: missing input filename`);
    // configure CLI processor
    postcss_options.map = true;
    postcss_plugins.push (...nodeonly_plugins);
    // process and printout
    const result = await postcss_process (FS.readFileSync (filename), filename);
    process.stdout.write (result);
  }
  process.exit (await main (process.argv.splice (2)));
}