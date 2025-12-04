#!/usr/bin/env node
//This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import 'fs';

const compile_commands_json = process.argv[2];

// const assert = console.assert;
import assert from 'node:assert';

const fs = await import ("fs");
const root = process.cwd() + "/";

const ccj_bin = fs.readFileSync (compile_commands_json);
const objects = JSON.parse (ccj_bin.toString());

const headers = [ fs.globSync ('modules/**/*.h'),
		  fs.globSync ('modules/**/*.tcc'),
		  fs.globSync ('modules/**/*.hpp') ].flat();
const ccfiles = [ fs.globSync ('modules/**/*.c'),
		  fs.globSync ('modules/**/*.cpp') ].flat();

/** Parse a single compilation object from the compile_commands.json file.
 *
 * Extract input file, include paths, preprocessor definitions and flags.
 * Normalize paths to the project root.
 * - o - Single object from the compile_commands.json array
 * - root - The absolute path to the project's root directory
 * - returns processed compilation info
 */
function parse_object (o, root)
{
  // console.log (`root=${root}`, o);
  assert (o.directory && o.directory.startsWith (root));
  assert (o.output && o.output.startsWith (root));
  assert (o.file && o.file.startsWith (root));
  const dat = {
    input: o.file.substr (root.length),
    warnings: [],
    flags: [],
    includes: [],
    defines: [],
  };
  for (const arg of o.arguments) {
    if (arg.startsWith ('-W'))
      dat.warnings.push (arg);
    if (arg.startsWith ('-f'))
      dat.flags.push (arg);
    if (arg.startsWith ('-D'))
      dat.defines.push (arg.substr (2));
    if (arg.startsWith ('-I')) {
      let ifile = arg.substr (2);
      if (ifile.startsWith (root))
	ifile = ifile.substr (root.length);
      dat.includes.push (ifile);
    }
  }
  return dat;
}

/** Categorize preprocessor definitions based on where they are used.
 *
 * Grep through the headers and cc files to categorize macros by
 * use into header, ccbody, and unused groups.
 */
function find_defines (defines, headers, ccfiles)
{
  let hhtexts = '', cctexts = '';
  for (const cc of ccfiles)
    if (!cc.startsWith ("/")) {
      cctexts = cctexts + '\n' + fs.readFileSync (cc).toString();
    }
  for (const hh of headers)
    if (!hh.startsWith ("/")) {
      hhtexts = hhtexts + '\n' + fs.readFileSync (hh).toString();
    }
  const hd = [], bd = [], ud = [];
  for (const def of defines)  {
    const rx = RegExp ('\\b' + def.replace (/=.*/, '') + '\\b', '');
    if (hhtexts.match (rx))
      hd.push (def);
    else if (cctexts.match (rx))
      bd.push (def);
    else
      ud.push (def);
  }
  hhtexts = '';
  return [hd, bd, ud];
}

const includes = new Set();
const defines = new Set();

const sources = [];
for (const o of objects) {
  const dat = parse_object (o, root);
  if (dat.input.includes ("/examples/"))
    continue;
  sources.push (dat.input);
  // console.log (dat.warnings.join (' '));
  // console.log (dat.flags.join (' '));
  for (const inc of dat.includes)
    includes.add (inc);
  for (const inc of dat.defines.sort())
    defines.add (inc);
}
sources.sort();

/* Strip path prefixes we don't use */
const pstrip = path => {
  if (path.search (/^modules\/juce\/modules\/juce_/) >= 0)
    return path.replace (/^modules\/juce\/modules\//, '');
  else
    return path.replace (/^modules\//, '');
};

console.log ('# Source files needed for juce and tracktion_engine');
console.log ('JUCE_SOURCES = $(strip \\');
for (const src of sources)
  if (src.search (/^modules\/juce\/modules\/juce_/) >= 0)
    console.log (' ', pstrip (src), '\\');
console.log (')');
console.log ('TRACKTION_SOURCES = $(strip \\');
for (const src of sources)
  if (src.search (/^modules\/juce\/modules\/juce_/) < 0)
    console.log (' ', pstrip (src), '\\');
console.log (')');

console.log ('');
const extinc = includes.keys().filter (str => str.startsWith ('/')).toArray();
let intinc = includes.keys().toArray().filter (str => str.startsWith ('modules')).sort();
intinc = intinc.filter (str => str != 'modules');		// -> external/tracktion_engine/
intinc = intinc.filter (str => str != 'modules/juce/modules');	// -> external/juce/
console.log ('TRACKTION_INTERNAL_INCLUDES', '= \\\n' + intinc.map (k => '  -I' + pstrip (k)).join (' \\\n'));
console.log ('');
console.log ('TRACKTION_EXTERNAL_INCLUDES', '= \\\n' + extinc.map (k => '  -I' + k).join (' \\\n'));

let [hd, bd, ud] = find_defines (defines, headers, ccfiles);

hd = hd.filter (str => !str.includes ('EXAMPLE')).sort();

console.log ('');
console.log ('# Macros used by header files');
console.log ('TRACKTION_HEADER_DEFINES', '= \\\n' + hd.map (k => '  -D '+k).join (' \\\n'));
console.log ('');
console.log ('# Macros used by C and C++ source files');
console.log ('TRACKTION_CCBODY_DEFINES', '= \\\n' + bd.map (k => '  -D '+k).join (' \\\n'));
console.log ('');
console.log ('TRACKTION_UNUSED_DEFINES', '= \\\n' + ud.map (k => '  -D '+k).join (' \\\n'));
console.log ('');
