// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
"use strict";

import * as fs from 'fs';
import * as path from 'path';

// Count newlines
function count_newlines (str) {
  let l = 0;
  for (let i = 0; i < str.length; i++)
    if (str[i] == '\n')
      l++;
  return l;
}

const re_jscss = /^\s*JsExtract\s*\.\s*s?css\s*`(.*?)`;/mgs;

// Extract strings from JsExtract markers.
function process_file (filename, config) {
  // determine output file
  const jscss_filename = filename.replace (/\.(vue|sfc|mjs|cjs|js)$/, '') + '.jscss';
  let ofile = jscss_filename;
  if (arg_config.odir)
    ofile = path.join (config.odir, path.basename (jscss_filename));
  // read input file
  const string = String (fs.readFileSync (filename));
  // split CSS blocks
  const parts = string.split (re_jscss);
  // convert CSS blocks
  let jscss_string = `/* ${filename} */ @charset "UTF-8";`;
  for (let i = 1; i < parts.length; i += 2)
    {
      const prefix_newlines = count_newlines (parts[i - 1]);
      jscss_string += '\n'.repeat (prefix_newlines);
      jscss_string += parts[i];
    }
  jscss_string += '\n';
  fs.writeFileSync (ofile, jscss_string);
  return ofile;
}

// Config and arguments
function parse_args (config, args, start = 2) {
  for (let i = start; i < args.length; i++)
    {
      if (args[i] == '-O' && i + 1 < args.length)
	config.odir = args[++i];
      else
	config.files.push (args[i]);
    }
}
const arg_config = {
  odir: '',
  files: [],
};

// Parse arguments and process files
parse_args (arg_config, process.argv);
if (arg_config.odir)
  arg_config.odir = path.resolve (arg_config.odir);
for (const filename of arg_config.files)
  process_file (filename, arg_config);

/** @file
 *
 * # NAME
 * `jsextract.js` - Script to extract snippets marked with JsExtract
 *
 * # SYNOPSIS
 * node **jsextract.js** [*OPTIONS*] [*file.js*...]
 *
 * # DESCRIPTION
 *
 * The **jsextract.js** processes its input files by looking for markers
 * such as ``` JsExtract.css`...` ``` and extracting the template string
 * contents into a corresponding `*.jscss` file. This can be preprocessed
 * or copied into a corresponding `*.css` file, to be loaded via
 * `JsExtract.fetch_css()`.
 *
 * # OPTIONS
 *
 * **-O** <*dir*>
 * :   Specify the directory for output files.
 */
