#!/usr/bin/env node
// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// Tree-sitter based code smell detector for C++ and JavaScript/TypeScript

import * as fs from 'fs';
import * as path from 'path';
import { getWasmPath, type SupportedLanguage } from 'tree-sitter-wasm';
import { Parser, Language } from 'web-tree-sitter';
import type { Node } from 'web-tree-sitter';

// --- Parser cache (one per grammar) ---

const parser_cache = new Map<SupportedLanguage, Parser>();

async function get_parser (lang: SupportedLanguage): Promise<Parser>
{
  if (parser_cache.has (lang)) return parser_cache.get (lang)!;
  const p = new Parser ();
  const wasm_bytes = fs.readFileSync (getWasmPath (lang));
  p.setLanguage (await Language.load (wasm_bytes));
  parser_cache.set (lang, p);
  return p;
}

// Initialise the WASM runtime once (idempotent)
await Parser.init();

// --- Language detection ---

function get_language (filename: string): SupportedLanguage
{
  const ext = path.extname (filename).slice (1).toLowerCase ();
  switch (ext) {
    case 'c':
      return 'c';
    case 'cc': case 'cpp': case 'cxx': case 'c++':
    case 'hh': case 'hpp': case 'h++': case 'h':
      return 'cpp';
    case 'tsx':
      return 'tsx';
    case 'ts':
      return 'typescript';
    case 'jsx':
      return 'tsx'; // tsx grammar handles JS+JSX (and TS+JSX)
    case 'js':
      return 'javascript';
    default:
      return 'cpp'; // default fallback
  }
}

// --- Checks config ---

const checks: Record<string, boolean> = {
  'ban-printf':                    true,   // C++ only
  'separate-body':                 true,
  'whitespace-before-parenthesis': true,
  'ban-fixme':                     true,
  'ban-todo':                      false,
};
let diag_count = 0;
let strict_mode = false; // treat tree-sitter parse errors as fatal

// --- Diagnostics ---

const istty = process.stdout.isTTY;
function emit (filename: string, line: number, column: number,
               severity: 'error' | 'warning', message: string, source_lines: string[])
{
  diag_count++;
  const B = istty ? '\x1b[1m' : '', f = istty ? '\x1b[22m' : '';
  const R = istty ? '\x1b[31m' : '', M = istty ? '\x1b[35m' : '';
  const G = istty ? '\x1b[32m' : '', Z = istty ? '\x1b[0m' : '';
  process.stdout.write (`${B}${filename}:${line}: ${severity === 'error' ? `${R}error:${Z}` : `${M}warning:${Z}`} ${message}${f}\n`);
  const lcs = line.toString ().padStart (5);
  if (source_lines.length > line - 1) {
    process.stdout.write (`${lcs} | ${source_lines[line - 1].trimRight ()}\n`);
    process.stdout.write (`${lcs} | ${' '.repeat (Math.max (0, column))}${B}${G}^${Z}\n`);
  }
}

// --- Helpers ---

// Find a token child ('(' or ')') inside an arguments/parameter node
function find_token (node: Node, tok: '(' | ')'): Node | null
{
  for (let i = 0; i < node.childCount; i++) {
    if (node.child (i).type === tok) return node.child (i);
  }
  return null;
}

function is_empty_list (node: Node): boolean
{
  // Only two children means empty: ( )
  return node.childCount === 2 && !!find_token (node, '(') && !!find_token (node, ')');
}

// --- Checks ---

function check_ban_printf (node: Node, fn: string, lines: string[], lang: string): void
{
  if (!checks['ban-printf'] || lang !== 'cpp') return;
  if (node.type !== 'call_expression') return;
  const func = node.childForFieldName ('function');
  if (func && /\b[a-z]?printf$/.test (func.text))
    emit (fn, node.startPosition.row + 1, node.startPosition.column,
         'warning', `invalid call to ${func.text}, use printout ()`, lines);
}

// Unified newline-before-brace check for function bodies (C++ + JS/TS)
function check_separate_body (node: Node, fn: string, lines: string[], lang: string): void
{
  if (!checks['separate-body']) return;

  // NOTE: 'arrow_function' excluded — too noisy for now, will re-enable later
  const func_types = lang === 'cpp'
    ? ['function_definition']
    : ['function_declaration', 'method_definition'];
  if (!func_types.includes (node.type)) return;

  // C++: skip lambdas
  if (lang === 'cpp' && node.type === 'function_definition') {
    let p = node.parent;
    while (p) { if (p.type === 'lambda_expression') return; p = p.parent; }
    // Skip macros — real functions have a return type, macros don't
    if (!node.childForFieldName ('type')) return;
  }

  const body = node.childForFieldName ('body');
  if (!body || body.startPosition.row === body.endPosition.row) return;

  const brace_col = lines[body.startPosition.row]?.indexOf ('{');
  if (brace_col < 0) return;

  const params = node.childForFieldName ('parameters');
  if (!params) return;

  const close_paren = find_token (params, ')');
  if (!close_paren) return;

  if (close_paren.startPosition.row === body.startPosition.row)
    emit (fn, body.startPosition.row + 1, brace_col,
         'warning', 'missing newline before function body', lines);
}


// Helper: check that the '(' token has a preceding whitespace character
function check_parenthesis_whitespace (list_node: Node, fn: string, lines: string[]): void
{
  const paren = find_token (list_node, '(');
  if (!paren || paren.startPosition.column === 0) return;
  const ch = lines[paren.startPosition.row]?.[paren.startPosition.column - 1];
  if (ch && /[\w]/.test (ch))
    emit (fn, paren.startPosition.row + 1, paren.startPosition.column - 1,
         'warning', 'missing whitespace before parenthesis', lines);
}

// Unified whitespace-before-parenthesis check (C++ + JS/TS)
// Relaxed for: empty lists, gettext _(), #define macros, C++ casts
function check_whitespace_before_paren (node: Node, fn: string, lines: string[], lang: string, in_error_zone: boolean): void
{
  if (!checks['whitespace-before-parenthesis']) return;
  // Skip nodes inside tree-sitter ERROR regions (broken macro parsing etc.)
  if (in_error_zone) return;

  // call_expression - function calls: foo (x) vs foo(x)
  if (node.type === 'call_expression') {
    const func = node.childForFieldName ('function');
    const args = node.childForFieldName ('arguments');
    if (!func || !args) return;

    // EXCEPTION: gettext _() translation markup - space would break it
    if (func.text.length == 1) return; // e.g. _("Translate Me")
    // EXCEPTION: C++ casts static_cast<T>(x), dynamic_cast<T>(x) etc.
    // tree-sitter parses these as call_expression with template_function callee
    if (func.type === 'template_function') return;
    // RELAXED: empty argument lists are OK without preceding space: relaxed ()
    if (is_empty_list (args)) return;

    check_parenthesis_whitespace (args, fn, lines);
    return;
  }

  // Function declarators / definitions - function name (params)
  // NOTE: #define FOO(a,b,c) is a preproc_function_def - not checked here,
  // NOTE: macro_invocation so far never appeared in tree-sitter-cpp output
  const decl_types = lang === 'cpp'
		   ? ['function_declarator']
		   : ['function_declaration'];
  if (!decl_types.includes (node.type)) return;

  const params = node.childForFieldName ('parameters');
  if (!params) return;
  // RELAXED: empty parameter lists are OK: function foo ()
  if (is_empty_list (params)) return;
  check_parenthesis_whitespace (params, fn, lines);
}

function check_ban_fixme (node: Node, fn: string, lines: string[]): void
{
  if (!checks['ban-fixme'] || node.type !== 'comment') return;
  if (/\bFI[X]ME\b/i.test (node.text))
    emit (fn, node.startPosition.row + 1, 0, 'warning', 'comment indicates unfinished code', lines);
}

function check_ban_todo (node: Node, fn: string, lines: string[]): void
{
  if (!checks['ban-todo'] || node.type !== 'comment') return;
  if (/\bTO[D]O\b/i.test (node.text))
    emit (fn, node.startPosition.row + 1, 0, 'error', 'comment indicates open issues', lines);
}

/* --- Macro stripping ---
 * Since tree-sitter has no preprocessor, we strip macros out, similar to doxygen.
 * All fixes use whitespace-preserving replacements (char → space, newlines kept)
 * so line/column offsets stay valid.
 *
 * 1. Multi-line #define bodies - continuation lines contain bare expressions.
 *    Fix: blank out the #define body + usage lines (chars → spaces).
 *
 * 2. Macro identifiers in type position (e.g. CONSTEXPR int foo).
 *    Fix: --macros FILE lists names to strip (space-preserving replacement).
 */

interface MacroEntry { name: string; func_like: boolean; }
let macro_list: MacroEntry[] = []; // populated by --macros flag

// Blank out multi-line #define blocks which tree-sitter fails to parse.
// Returns [stripped_content, detected_macros] so callers can inline-replace usages.
function strip_multiline_defines (content: string): [string, MacroEntry[]]
{
  const lines = content.split ('\n');
  const blank = new Set<number>(); // line indices to blank out
  const macros: MacroEntry[] = [];

  // Blank only multi-line #define (body ends with \\); tree-sitter can't parse continuations.
  for (let i = 0; i < lines.length; i++) {
    if (/^\s*#\s*define\s+(\w+)\s*(.*)\\\s*$/.test (lines[i])) {
      blank.add (i);
      let j = i + 1;
      while (j < lines.length && /\\\s*$/.test (lines[j])) { blank.add (j); j++; }
      if (j < lines.length) blank.add (j); // last continuation, no trailing \\
    }
  }

  // Replace marked lines with spaces (preserve length; newlines stay via split/join)
  for (let i = 0; i < lines.length; i++)
    if (blank.has (i))
      lines[i] = ' '.repeat (lines[i].length);
  return [lines.join ('\n'), macros];
}

// Load --macros file: one name per line, '#' = comment, blank lines ignored.
// Lines ending with '()' are function-like (strip name + parens + args),
// otherwise object-like (strip the word only).
function load_macros_file (filepath: string): MacroEntry[]
{
  let text: string;
  try { text = fs.readFileSync (filepath, 'utf-8'); }
  catch (err) { console.error (`error: cannot read macros file '${filepath}': ${err instanceof Error ? err.message : err}`); process.exit (1); }
  const entries: MacroEntry[] = [];
  for (const raw of text.split ('\n')) {
    const line = raw.trim ();
    if (!line || line.startsWith ('#')) continue;
    const func_like = line.endsWith ('()');
    const name = func_like ? line.slice (0, -2) : line;
    entries.push ({ name, func_like });
  }
  return entries;
}

// Build a space-preserving regex replacer for the macro list.
// Object-like: \bNAME\b → spaces
// Function-like: \bNAME\s*\([^)]*\) → spaces (single-level parens)
function build_macro_replacer (macros: MacroEntry[]): ((text: string) => string) | null
{
  if (macros.length === 0) return null;

  // Sort longest name first so partial matches don't win (e.g. NS_CONST before NS)
  const sorted = [...macros].sort ((a, b) => b.name.length - a.name.length);

  // Build alternation groups: function-like and object-like separately
  const func_like = sorted.filter (m => m.func_like).map (m => m.name.replace (/[/\\.^*+?${}()|[\]]/g, '\\$&'));
  const obj_like = sorted.filter (m => !m.func_like).map (m => m.name.replace (/[/\\.^*+?${}()|[\]]/g, '\\$&'));

  const parts: string[] = [];
  if (func_like.length > 0) parts.push ('\\b(' + func_like.join ('|') + ')\\s*\\([^)]*\\)');
  if (obj_like.length > 0) parts.push ('\\b(' + obj_like.join ('|') + ')\\b');

  const pattern = new RegExp (parts.join ('|'), 'g');
  return (text: string) => text.replace (pattern, match => ' '.repeat (match.length));
}

// --- Main ---

// Walk the AST, yielding (node, in_error_zone) tuples.
// in_error_zone is true when an ancestor or self is an ERROR node.
function* walk (node: Node, in_error_zone: boolean): Generator<[Node, boolean]> {
  if (node.type === 'ERROR') in_error_zone = true;
  yield [node, in_error_zone];
  for (let i = 0; i < node.childCount; i++)
    yield* walk (node.child (i), in_error_zone);
}

async function analyze_file (filename: string): Promise<void>
{
  let content = fs.readFileSync (filename, 'utf-8');
  const lang = get_language (filename);
  const parser = await get_parser (lang);

  // Preprocess for tree-sitter compatibility (C++ only).
  // All replacements are whitespace-preserving - line/column offsets unchanged.
  if (lang === 'cpp') {
    const [stripped, detected_macros] = strip_multiline_defines (content);
    content = stripped;
    // Merge --macros file entries with auto-detected #define macros.
    const all_macros = [...macro_list, ...detected_macros];
    const replacer = build_macro_replacer (all_macros);
    if (replacer) content = replacer (content);
  }

  const lines = content.split ('\n');
  const root = parser.parse (content).rootNode;

  // --strict: refuse to lint files with tree-sitter parse errors
  // tree-sitter has no preprocessor and struggles with: #define inside blocks,
  // member-function-pointers, operator co_await (), macros with == in args, etc.
  if (strict_mode) {
    const errors = root.descendantsOfType ('ERROR');
    if (errors.length > 0) {
      for (const err of errors) {
        emit (filename, err.startPosition.row + 1, err.startPosition.column,
              'error', `tree-sitter parse error`, lines);
      }
      return;
    }
  }

  for (const [node, in_error_zone] of walk (root, false)) {
    check_ban_printf (node, filename, lines, lang);
    check_separate_body (node, filename, lines, lang);
    check_whitespace_before_paren (node, filename, lines, lang, in_error_zone);
    check_ban_fixme (node, filename, lines);
    check_ban_todo (node, filename, lines);
  }
}

// --- CLI ---

function print_help (): void
{
  console.log (`Usage: ${path.basename (process.argv[0])} [Options] file.cc ...`);
  console.log ('Check source files for code smells using tree-sitter AST analysis.');
  console.log ('Supports C (.c) C++ (.cc .hh .cpp .hpp .h) and JS/TS (.js .ts .jsx .tsx).');
  console.log ('  --all / --none        Enable/disable all checks');
  console.log ('  --list                Show check configuration');
  console.log ('  --errors              Exit non-zero if any diagnostics found');
  console.log ('  --strict              Treat tree-sitter parse errors as fatal (default: skip)');
  console.log ('  --macros FILE         Strip listed macros before parsing (see help)');
  console.log ('  --<check>=1/0         Toggle individual check');
}

async function main (): Promise<number>
{
  let exit_on_diag = false;
  const files: string[] = [];
  const args = process.argv.slice (2);
  for (let i = 0; i < args.length; i++) {
    const arg = args[i];
    if (arg === '-h' || arg === '--help') {
      print_help ();
      return 0;
    }

    // Handle --name=value form
    const eq = arg.indexOf ('=');
    if (arg.startsWith ('--') && eq >= 3) {
      const name = arg.slice (2, eq);
      const val = arg.slice (eq + 1);
      if (name in checks) { checks[name] = val !== '0'; continue; }
      if (name === 'errors') { exit_on_diag = true; continue; }
      if (name === 'macros') { macro_list = load_macros_file (val); continue; }
      if (name === 'strict') { strict_mode = val !== '0'; continue; }
      console.error (`warning: unknown option '${arg}'`);
      continue;
    }

    // Handle bare --flag and --flag VALUE forms
    if (arg === '--errors') { exit_on_diag = true; }
    else if (arg === '--strict') { strict_mode = true; }
    else if (arg === '--macros' && i + 1 < args.length) { i++; macro_list = load_macros_file (args[i]); }
    else if (arg === '--all') { for (const k in checks) checks[k] = true; }
    else if (arg === '--none') { for (const k in checks) checks[k] = false; }
    else if (arg === '--list') { for (const k in checks) console.log (`  --${k}=${Number (checks[k])}`); }
    else if (arg.startsWith ('--')) { console.error (`warning: unknown option '${arg}'`); }
    else { files.push (arg); }
  }
  if (files.length === 0) {
    print_help ();
    return 0;
  }
  for (const f of files)
    await analyze_file (f);
  return exit_on_diag && diag_count > 0 ? 1 : 0;
}

main ().then (exit_status => process.exit (exit_status));
