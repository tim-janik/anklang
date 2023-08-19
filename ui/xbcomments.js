// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
"use strict";

const fs = require ('fs');
const path = require ('path');

/// Strip whitespace from start and end of string.
function lrstrip (str)
{
  return str.replaceAll (/^[\s\t\f]+|[\s\t\f]+$/g, '');
}

/// Remove C-comment style `\n\s\*\s`
function fix_indent (txt)
{
  txt = lrstrip (txt);
  if (txt.substr (0, 3) === '///')
    {
      txt = ('\n' + txt).replace (/\n\s*\/\/\/[ \t]?/g, '\n');
    }
  else if (txt.substr (0, 2) === '/*')
    {
      txt = txt.replace (/^\/\*+\s*/, '');
      txt = txt.replace (/\s*\*+\/$/, '');
      const count_newlines = (txt.match (/\n/g) || []).length;
      // remove \s*\*+\s if *all* lines use it
      if (count_newlines === (txt.match (/\n\s*\*+\s/g) || []).length)
	txt = txt.replace (/\n\s*\*+\s/g, '\n');
      // remove \s*\*+ if *all* lines use it
      else if (count_newlines === (txt.match (/\n\s*\*+/g) || []).length)
	txt = txt.replace (/\n\s*\*+/g, '\n');
    }
  txt = lrstrip (txt);
  return txt + '\n';
}

/// Look for a markdown documentation block
function extract_md (txt)
{
  if (txt.search (/^\s*#+\s+[-_.~!*'"();:@&=+$,\/?%#A-z0-9]/) >= 0)
    return txt;
  return '';
}

// Patterns to match block comments at line start
// slash star comment:		^ \s* \/\*\* .*? \*\/
// triple slash comment:	^ ( \s* \/\/\/ [^\n]* \n )+
const block_comments = /(^\s*\/\*\*.*?\*\/)|(^(?:\s*\/\/\/[^\n]*\n)+)/mgis;

/// Extract and process block comments of a file
function process_file (filename, config) {
  // determine output file
  let ofile = path.join (config.odir, path.basename (filename.replace(/\.(vue|sfc|ts|js)$/, '')));
  // read input file
  const string = String (fs.readFileSync (filename));
  // identify comment blocks
  let blocks = [], m, re = block_comments;
  while (( m = re.exec (string) ))
    {
      let comment = m[0];
      comment = fix_indent (comment);
      // console.log ('{{{', comment ,'}}}');
      const md = extract_md (comment);
      blocks.push (md);
    }
  // cleanup
  blocks = blocks.filter (t => !!t);
  // output
  ofile += '.md';
  const output = blocks.join ('\n\n');
  if (lrstrip (output))
    fs.writeFileSync (ofile, output);
  return ofile;
}

// Config and arguments
function parse_args (config, args, start = 2) {
  for (let i = start; i < args.length; i++)
    {
      if (args[i] == '-O' && i + 1 < args.length)
	config.odir = args[++i];
      else if (args[i] == '--debug')
	config.debug = true;
      else if (args[i] == '-I' && i + 1 < args.length)
	config.includes.push (args[++i]);
      else
	config.files.push (args[i]);
    }
}
const arg_config = {
  odir: './',
  includes: [],
  files: [],
  debug: false,
};

// Parse arguments and process files
parse_args (arg_config, process.argv);
arg_config.odir = path.resolve (arg_config.odir);
for (const filename of arg_config.files)
  process_file (filename, arg_config);
