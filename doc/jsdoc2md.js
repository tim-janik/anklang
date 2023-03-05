// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
"use strict";

const fs = require ('fs');
const jsdoc = require ('jsdoc-api');

function usage (full = false) {
  const prog = process.argv[1].replace (/.*\//, '');
  console.log ("Usage:", prog, "[OPTIONS] [jsdoc-data.json...]");
  if (!full) return;
  //            12345678911234567892123456789312345678941234567895123456789612345678971234567898
  console.log ("Generate API documentation in Markdown format from *.js.");
  console.log ("  -h, --help        Display command line help");
  console.log ("  -d <DEPTH>        Set Markdown section level");
  console.log ("  -e <EXPORTNAME>   Use EXPORTNAME as API prefix");
}

// Config and arguments
function parse_args (config, args, start = 2) {
  for (let i = start; i < args.length; i++)
    {
      if (args[i] == '-h' || args[i] == '--help')
	{
	  usage (true);
	  process.exit (0);
	}
      else if (args[i] == '--debug')
	config.debug = true;
      else if (args[i] == '-d' && i + 1 < args.length)
	config.depth = args[++i] | 0;
      else if (args[i] == '-e' && i + 1 < args.length)
	config.exports = args[++i];
      else
	config.files.push (args[i]);
    }
}
const arg_config = {
  debug: false,
  files: [],
  exports: '',
  // h1, h2, h3
  depth: 2,
};

// build dict for classes
function add_class (classname, classdesc = '', exports = false) {
  let cl = global_classes[classname];
  if (!cl)
    global_classes[classname] = cl = { methods: [], classdesc: '', name: classname, exports: false };
  if (classdesc)
    cl.classdesc = classdesc;
  if (exports)
    cl.exports = true;
  return cl;
}
let global_classes;

// collect class methods
function add_method (doclet) {
  const name = doclet.name, params = doclet.meta.code.paramnames;
  const description = doclet.description;
  const cl = add_class (doclet.memberof);
  const m = { name, params, description,
	      'static': doclet.scope == 'static',
	      ctor: name == doclet.memberof,
  };
  cl.methods.push (m);
  return m;
}

// collect global functions
function add_function (name, params, description = '', exports = false) {
  let fun = global_functions[name];
  if (!fun)
    global_functions[name] = fun = { name, params: [], description: '', exports: false, 'static': false, ctor: false };
  if (params && params.length)
    fun.params = params;
  if (description)
    fun.description = description;
  if (exports)
    fun.exports = true;
  return fun;
}
let global_functions;

// collect global variables
function add_var (name, description = '', exports = false) {
  let gvar = global_vars[name];
  if (!gvar)
    global_vars[name] = gvar = { name, description: '', exports: false, };
  if (description)
    gvar.description = description;
  if (exports)
    gvar.exports = true;
  return gvar;
}
let global_vars;
let global_overview;

/// Strip left side / head
function lstrip (s) { return s.replace (/^\s+/, ''); }
/// Strip right side / tail
function rstrip (s) { return s.replace (/\s+$/, ''); }
/// Strip left and right side or head and tail
function strip (s) { return s.replace (/^\s+|\s+$/g, ''); }
/// Strip empty newlines
function strip_whitelines (s) { return s.replace (/\n\s*\n/g, '\n'); }
/// Indent all lines
function indent_lines (s, prefix) { return s.replace (/(^|\n)/g, '$1' + prefix); }
/// Make indented description paragraph with lead character
function dpara (s, prefix = '', c1 = '') {
  s = description_markdown (s);
  s = strip (s);
  // s = strip_whitelines (s); // empty lines are needed as block seperators
  s = indent_lines (s, prefix);
  if (c1)
    s = s.replace (/^./, c1);
  return s;
}

/// Generate function markdown
function gen_section_head (cfg, what) {
  const exports = cfg.exports ? cfg.exports + ' ' : '';
  let s = '\n';
  s += cfg.h2 + exports + what + '\n';
  s += '\n';
  return s;
}

/// Generate variable markdown
function gen_global_var (cfg, gvar) {
  const exports = cfg.exports && gvar.exports ? '<small>`' + cfg.exports + '.`</small>' : '';
  let s = exports + '**`' + gvar.name + '`**' + '\n';
  if (gvar.description)
    s += dpara (gvar.description, '    ', ':') + '\n';
  s += '\n';
  return s;
}

/// Substitute chars to yield a valid HTML anchor
function make_anchor (input) {
  let string = input.replace (re_charn, '_');
  if (string.length && string[0].search (re_char1) == 0)
    string = '_' + string;
  return string;
}
const char1 = '_:A-Za-z\xC0-\xD6\xD8-\xF6\xF8-\u02FF\u0370-\u037D\u037F-\u1FFF\u200C-\u200D\u2070-\u218F\u2C00-\u2FEF\u3001-\uD7FF\uF900-\uFDCF\uFDF0-\uFFFD';
const charn = '0-9\xB7\u0300-\u036F\u203F-\u2040.-';
const re_char1 = new RegExp ('[^' + char1 + ']', 'gu');
const re_charn = new RegExp ('[^' + char1 + charn + ']+', 'gu');

/// Process description to add auto-links, etc
function description_markdown (description) {
  let d = description;
  // support anchor links
  d = d.replace (/(^|\s)#([a-z_A-Z][a-zA-Z0-9_]+)\b/, '$1[$2](#$2)');
  // done
  return d;
}

/// Generate function markdown
function gen_function (cfg, fun, exports = '') {
  const exports_name = exports + (exports && '.') + fun.name;
  let prefix = '', postfix = '', anchor = make_anchor (exports_name);
  const pexports = exports ? '<small>`' + exports + '`</small>.' : '';
  if (fun['static'])
    postfix += '   `[static]`';
  if (fun.ctor)
    prefix += '*new* ';
  let params = fun.params.join (', ');
  params = params ? ' `(`*' + params + '*`)`' : '`()`';
  let s = '';
  if (anchor)
    s += '<a id="' + anchor + '" data-4search="' + cfg.filename + ':' + exports_name + ';func"></a> ';
  s += prefix + pexports + '**`' + fun.name + '`**' + params + postfix + '\n';
  if (fun.description)
    s += dpara (fun.description, '    ', ':') + '\n';
  s += '\n';
  return s;
}

/// Generate function markdown
function gen_global_function (cfg, fun) {
  const exports = cfg.exports && fun.exports ? cfg.exports : '';
  return gen_function (cfg, fun, exports);
}

/// Generate class markdown
function gen_global_class (cfg, cl) {
  const hprefix = '', anchor = make_anchor (cl.name); // cfg.exports && cl.exports ? cfg.exports + '.' : '';
  const exports = cfg.exports && cl.exports ? cfg.exports : '';
  const pexports = exports ? '<small>`' + exports + '`</small>.' : '';
  let s = '';
  s += '\n' + cfg.h2 + hprefix + cl.name + ' class\n';
  s += '*class* ' + pexports + '**' + cl.name + '**';
  if (anchor)
    s += '<a id="' + anchor + '" data-4search="' + cfg.filename + ':' + cl.name + ';class"></a> ';
  s += '\n' + dpara (cl.classdesc || ' … \n', '    ', ':') + '\n';
  s += '\n';
  for (const mt of cl.methods) {
    let t = gen_function (cfg, mt);
    s += rstrip (indent_lines (t, '    ')) + '\n\n';
  }
  return s;
}

// Generate markdown from jsdoc AST
function generate_md (cfg, ast) {
  const classdone = {};
  let s = '';
  // build dicts from doclets
  for (const doclet of ast) {
    if ('string' !== typeof doclet.meta?.code?.name) continue;
    // https://github.com/jsdoc/jsdoc/blob/master/packages/jsdoc/lib/jsdoc/schema.js
    const exports = doclet.meta.code.name && doclet.meta.code.name.search ('exports.') == 0;
    // collect classes
    if (doclet.scope == 'global' && doclet.kind == 'class' && doclet.classdesc && doclet.meta.code.type == "ClassDeclaration")
      add_class (doclet.name, doclet.classdesc, exports);
    // collect methods
    if (doclet.memberof && doclet.meta.code.paramnames && doclet.description &&
	(doclet.scope == 'instance' || doclet.scope == 'static'))
      add_method (doclet);
    // collect functions
    if (doclet.scope == 'global' && doclet.meta.code.paramnames && doclet.description)
      add_function (doclet.name, doclet.meta.code.paramnames, doclet.description, exports);
    // collect variables
    if (doclet.scope == 'global' && doclet.kind == 'constant' && doclet.description)
      add_var (doclet.name, doclet.description, exports);
    // collect overview
    if (doclet.scope == 'global' && doclet.kind == 'file' && doclet.description)
      global_overview = doclet.description;
  }
  // produce overview
  if (global_overview)
    s += global_overview + '\n';
  // generate classes
  for (let [name, cl] of Object.entries (global_classes))
    s += gen_global_class (cfg, cl);
  // generate variables
  if (Object.getOwnPropertyNames (global_vars).length > 0)
    s += gen_section_head (cfg, 'Constants');
  for (let [name, gvar] of Object.entries (global_vars))
    s += gen_global_var (cfg, gvar);
  // generate functions
  if (Object.getOwnPropertyNames (global_functions).length > 0)
    s += gen_section_head (cfg, 'Functions');
  for (let [name, fun] of Object.entries (global_functions))
    s += gen_global_function (cfg, fun);
  return s;
}

// Parse arguments and process files
parse_args (arg_config, process.argv);
arg_config.h1 = '#'.repeat (arg_config.depth | 0) + ' ';
arg_config.h2 = '#' + arg_config.h1;
arg_config.h3 = '#' + arg_config.h2;
// Generate docs from json files
for (let filename of arg_config.files)
  {
    global_functions = {};
    global_classes = {};
    global_vars = {};
    global_overview = '';
    const configure = 'doc/jsdocrc.json';
    const jsdocast = jsdoc.explainSync ({ configure, files: [ filename ] })
    const cfg = Object.assign ({ filename }, arg_config);
    console.log (generate_md (cfg, jsdocast));
  }
