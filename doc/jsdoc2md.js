// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
"use strict";

const fs = require ('fs');

// Config and arguments
function parse_args (config, args, start = 2) {
  for (let i = start; i < args.length; i++)
    {
      if (args[i] == '--debug')
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
const global_classes = {};

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
const global_functions = {};

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
  s = strip_whitelines (strip (s));
  s = indent_lines (s, prefix);
  if (c1)
    s = s.replace (/^./, c1);
  return s;
}

/// Generate function markdown
function gen_function_head (cfg) {
  const xprefix = cfg.exports ? cfg.exports + ' ' : '';
  let s = '\n';
  s += cfg.h2 + xprefix + 'Functions\n';
  s += '\n';
  return s;
}

/// Generate function markdown
function gen_function (cfg, fun, exports = '') {
  let prefix = '', postfix = '';
  if (fun['static'])
    postfix += '   `[static]`';
  if (fun.ctor)
    prefix += '*new* ';
  let params = fun.params.join (', ');
  params = params ? ' `(`*' + params + '*`)`' : '`()`';
  let s = prefix + exports + '**`' + fun.name + '`**' + params + postfix + '\n';
  if (fun.description)
    s += dpara (fun.description, '    ', ':') + '\n';
  s += '\n';
  return s;
}

/// Generate function markdown
function gen_global_function (cfg, fun) {
  const xprefix = cfg.exports && fun.exports ? '<small>`' + cfg.exports + '.`</small>' : '';
  return gen_function (cfg, fun, xprefix);
}

/// Generate class markdown
function gen_global_class (cfg, cl) {
  const hprefix = ''; // cfg.exports && cl.exports ? cfg.exports + '.' : '';
  const xprefix = cfg.exports && cl.exports ? '<small>`' + cfg.exports + '.`</small>' : '';
  let s = '';
  s += '\n' + cfg.h2 + hprefix + cl.name + ' class\n';
  s += '*class* ' + xprefix + '**' + cl.name + '**\n';
  s += dpara (cl.classdesc, '    ', ':') + '\n';
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
    if (!doclet.meta) continue;
    const exports = doclet.meta.code.name.search ('exports.') == 0;
    // collect classes
    if (doclet.scope == 'global' && doclet.kind == 'class' && doclet.meta.code.type == "ClassDeclaration")
      add_class (doclet.name, doclet.classdesc, exports);
    // collect methods
    if (doclet.memberof && doclet.meta.code.paramnames && doclet.description &&
	(doclet.scope == 'instance' || doclet.scope == 'static'))
      add_method (doclet);
    // collect functions
    if (doclet.scope == 'global' && doclet.meta.code.paramnames && doclet.description)
      add_function (doclet.name, doclet.meta.code.paramnames, doclet.description, exports);
  }
  // generate classes
  for (let [name, cl] of Object.entries (global_classes))
    s += gen_global_class (cfg, cl);
  // generate functions
  if (Object.getOwnPropertyNames (global_functions).length > 0)
    s += gen_function_head (cfg);
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
for (let filename of arg_config.files) {
  const string = String (fs.readFileSync (filename));
  const jsdocast = JSON.parse (string);
  console.log (generate_md (arg_config, jsdocast));
}
