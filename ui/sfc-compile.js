// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
"use strict";

const fs = require ('fs');
const path = require ('path');
const sass = require ('sass');

/// Count newlines
function count_newlines (str) {
  let l = 0;
  for (let i = 0; i < str.length; i++)
    if (str[i] == '\n')
      l++;
  return l;
}

/// List tag names matching `^</tag>` in `string`.
function find_tags (string) {
  const re = /^<\/[a-z][a-z0-9-]*>/mgi;
  const tags = new Set();
  let arr;
  while ((arr = re.exec (string)))
    tags.add (arr[0].substr (2, arr[0].length - 3));
  return Array.from (tags);
}

/// Extract and process sections of an SFC file.
function process_file (filename, config) {
  // determine output file
  let ofile = path.join (config.odir, path.basename (filename.replace(/\.(vue|sfc)$/, '')));
  // read input file
  const string = String (fs.readFileSync (filename));
  // determine supported tags
  const taglist = find_tags (string);
  // recognize XML comment
  let pat = '(?:<!--.*?-->)';
  // recognize tags
  for (const tag of taglist)
    {
      // <script/> is always greedy
      const inner = tag === 'script' ? '.*' : '.*?';
      pat += String.raw`|^(?:<${tag}(?:\s+[^<>]*)?>${inner}^<\/${tag}>)`;
    }
  // split at tags
  const re = RegExp ('(' + pat + ')', 'mgs');
  const parts = string.split (re); // [ '', '<!-- -->', '\n\n', '<style></style>', '\n\n', '<template></template>', '\n\n', '<script></script>', '\n' ]
  // convert tags
  let newlines = 0;
  for (let i = 1; i < parts.length; i += 2)
    {
      newlines += count_newlines (parts[i - 1]);
      const part_newlines = count_newlines (parts[i]);
      if (parts[i].startsWith ('<!--')) // comment
	parts[i] = '  /*' + parts[i].substr (4, parts[i].length - 7) + '*/';
      else if (parts[i].match (/<\/script>$/i)) // in <script/>
	{
	  parts[i] = parts[i].substr (0, parts[i].length - 9); // strip end tag
	  const bits = parts[i].split (/^([^>]+>)/); // split start tag: ['', '<script \n lang=...>', 'rest...']
	  bits[1] = bits[1].replace (/./g, ''); // delete all non-newlines
	  parts[i] = bits.join ('');
	}
      else // <any/>
        {
	  const bits = parts[i].split (/(^[^>]+>|<\/[^>]+>$)/); // split tags: [ '', '<any\n>', 'contents...', '</any>', '' ]
	  console.assert (bits.length == 5);
	  const tag = bits[3].substr (2, bits[3].length - 3);
	  bits[1] = bits[1].replace (/./g, ''); // delete all non-newlines
	  if (parts[i].match (/<\/style>$/i))
	    {
	      bits[3] = '\n';
	      write_style (filename, ofile, config, '\n'.repeat (newlines) + bits.join (''));
	    }
	  else
	    {
	      bits[2] = bits[2].replace (/([`$\\])/g, '\\$1');
	    }
	  bits[1] += `function sfc_${tag} (T, m) { return T\``; // start function
	  bits[3] = '`; }'; // end function
	  parts[i] = bits.join ('');
	}
      newlines += part_newlines;
    }
  // return string.replace (re, 'function sfc_docs (tl_docs, m) { return tl_docs`$3`; }');
  ofile += '.js';
  fs.writeFileSync (ofile, parts.join (''));
  return ofile;
}

/// Process and write style information
function write_style (filename, ofile, config, stylestring) {
  const result = sass.renderSync ({
    data: stylestring,
    file: filename,
    includePaths: config.includes,
    outputStyle: 'expanded',
    sourceMap: config.debug,
    omitSourceMapUrl: false,
    sourceMapEmbed: config.debug,
    functions: require ("./chromatic-sass2"),
    outFile: ofile + '.css',
  });
  //  function (err, result) {
  // if (err)       console.error (err);
  fs.writeFileSync (ofile + '.css', result.css.toString());
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
