/// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
"use strict";

/* This fils is loaded as CJS module from jsdoc, via jsdocrc.json */

/// Export jsdoc plugin to convert '/// comment\n' into '/** comment */\n'
exports.handlers = {
  beforeParse: (arg) => {
    // arg = { filename, source }
    arg.source = convert_slashes (arg.source);
  }
};

/// Convert triple slash comments in *src_text*
/// Turns triple slash comments into jsdoc style comments.
/// Joins comments from multiple consecutive triple slash lines.
function convert_slashes (src_text) {
  return src_text.replace (/((\n|^)\s*\/\/\/([^\n]*)\n)+/gm, lines => {
    lines = lines.replace (/\*\//g, '*​/');		// escape premature */ via Zwsp
    lines = lines.replace (/\/\/\//, '/**');		// replace *first* /// with /**
    lines = lines.replace (/(\n\s*)\/\/\//g, '$1  *');	// replace consecutive ///
    lines = lines.replace (/\n?$/, '*/\n');		// terminate with */
    return lines;
  });
}

// test with: node slashcomment.js INPUTFILE
if (process.argv[1].replace (/.*\//, '') == 'slashcomment.js') {
  const fs = require ('fs');
  const source = String (fs.readFileSync (process.argv[2]));
  console.log (convert_slashes (source));
}
