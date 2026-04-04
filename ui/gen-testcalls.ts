// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import fs from "node:fs";
import path from "node:path";

/**
 * Extract exported function names from TypeScript source using regex.
 * For Makefile generation, this needs to work without any node_modules.
 *
 * DESIGN PRINCIPLE: Never silently skip potential test functions (no false negatives).
 * Better to throw an error than miss a test, test export code can easily be adapted.
 *
 * SUPPORTED PATTERNS (detected as tests):
 * - export function name() {} (including async, generator)
 * - export const/let/var name = ... (arrow functions, values, function expressions)
 * - export default function name() {}
 * - export { foo, bar as baz }
 *
 * SKIPPED SILENTLY (TypeScript-only, never runtime):
 * - export interface/type/class/enum
 *
 * THROWS ERROR (patterns that need explicit handling - rewrite or skip):
 * - export default () => {} (arrow function default) - rewrite as named function
 * - export namespace NS { ... }
 * - export declare function foo() {}
 * - export import Foo = require("foo")
 * - Any unrecognized export pattern
 */
function get_exported_functions (file_path: string)
{
  const source_text = fs.readFileSync (file_path, "utf8");

  // Remove all comments FIRST, but preserve newlines to keep line numbers stable
  const clean_text = source_text.replace (/\/\*[\s\S]*?\*\/|\/\/.*/g, match => match.replace (/[^\n]/g, ""))

  const exports: { name: string; local: string }[] = [];

  function add (name: string, local: string = name)
  {
    exports.push ({ name, local });
  }

  // Use regex with exec to find all 'export' lines
  const export_regex = /^(\s*)(export\b.*)$/gm;
  let match: RegExpExecArray | null;
  while ((match = export_regex.exec (clean_text)) !== null) {
    // Position of the actual "export" keyword (after leading whitespace)
    const export_pos = match.index! + match[1].length;
    const line_number = (clean_text.substring (0, export_pos).match (/\n/g) || []).length + 1;
    const export_statement = match[2];

    // Pattern 1: export [async] function [ * ] name(...)
    const func_match = export_statement.match (/^export\s+(?:async\s+)?function\s*\*?\s*([a-zA-Z0-9_$]+)/);
    if (func_match) {
      add (func_match[1]);
      continue;
    }

    // Pattern 2: export const/let/var name = ...
    const const_match = export_statement.match (/^export\s+(?:const|let|var)\s+([a-zA-Z0-9_$]+)\s*=/);
    if (const_match) {
      add (const_match[1]);
      continue;
    }

    // Pattern 3: export default function ... (only functions)
    const default_func = export_statement.match (/^export\s+default\s+(?:async\s+)?function\s*\*?\s*(?:([a-zA-Z0-9_$]+))?/);
    if (default_func) {
      add ("default", default_func[1] ?? "default");
      continue;
    }

    // Pattern 4: export { foo, bar as baz } (single-line only)
    const named_block_match = export_statement.match (/^export\s*\{([^}]*)\}/m);
    if (named_block_match && !named_block_match[1].includes ("\n")) {
      // Match each export: name or "name as alias"
      const element_regex = /([a-zA-Z0-9_$]+)(?:\s+as\s+([a-zA-Z0-9_$]+))?/g;
      let elem_match;
      while ((elem_match = element_regex.exec (named_block_match[1])) !== null) {
        add (elem_match[2] ?? elem_match[1], elem_match[1]);
      }
      continue;
    }

    // Pattern 5: export let/var name = ... (could be function expressions)
    const letvar_match = export_statement.match (/^export\s+(?:let|var)\s+([a-zA-Z0-9_$]+)\s*=/);
    if (letvar_match) {
      add (letvar_match[1]);
      continue;
    }

    // Pattern 6: TypeScript-only exports - skip silently (never runtime)
    if (/^export\s+(?:class|interface|type|enum)\s/.test (export_statement)) {
      continue;
    }

    // UNHANDLED: Throw error for any export we don't understand
    throw new Error (`Unrecognized export pattern at ${file_path}:${line_number}: "${export_statement}"`);
  }

  return exports;
}

// Get command line arguments
const args = process.argv.slice (2);

if (args.length === 0) {
  console.error ("Usage: tsx gen-testcalls.ts --list|--out <output-file> <source-file> [source-file...]");
  console.error ("  --list  List test names one per line to stdout");
  console.error ("  --out   Generate TypeScript output file");
  process.exit (1);
}

// Check for --list or --out flag
let list_mode = false;
let out_mode = false;
let out_file: string | undefined;
let source_files: string[];

if (args[0] === "--list") {
  list_mode = true;
  if (args.length < 2) {
    console.error ("Usage: tsx gen-testcalls.ts --list <source-file> [source-file...]");
    process.exit (1);
  }
  source_files = args.slice (1);
} else if (args[0] === "--out") {
  out_mode = true;
  if (args.length < 3) {
    console.error ("Usage: tsx gen-testcalls.ts --out <output-file> <source-file> [source-file...]");
    process.exit (1);
  }
  out_file = args[1];
  source_files = args.slice (2);
} else {
  console.error ("Usage: tsx gen-testcalls.ts --list|--out <output-file> <source-file> [source-file...]");
  console.error ("  --list  List test names one per line to stdout");
  console.error ("  --out   Generate TypeScript output file");
  process.exit (1);
}

// Validate source files exist
for (const file of source_files) {
  if (!fs.existsSync (file)) {
    console.error (`Source file not found: ${file}`);
    process.exit (1);
  }
}

const imports: string[] = [];
const registry_entries: string[] = [];
const test_names: string[] = [];

for (const file of source_files) {
  const base = path.basename (file, ".ts");
  const import_name = base.replace (/[^a-zA-Z0-9_]/g, "_");

  // In list mode, we don't need imports or relative paths
  if (!list_mode && out_file) {
    const relative_path = path.relative (path.dirname (out_file), file);
    imports.push (`import * as ${import_name} from "${relative_path}";`);
  }

  const funcs = get_exported_functions (file);

  for (const { name, local } of funcs) {
    const key = `${base}.${name}`;
    const ref = `${import_name}.${local}`;
    registry_entries.push (`  "${key}": ${ref},`);
    test_names.push (key);
  }
}

// Generate TypeScript output
const ts_output = `
${imports.join ("\n")}

const __testcalls_registry__ = {
${registry_entries.join ("\n")}
};

export function testcalls_find (name: string)
{
  return name in __testcalls_registry__ ? __testcalls_registry__[name] : undefined;
}

export const testcalls_names = Object.keys (__testcalls_registry__);
`;

if (list_mode) {
  // Just list test names, one per line
  test_names.forEach (name => console.log (name));
} else {
  console.log (`  WRITE    ${out_file}`);
  fs.writeFileSync (out_file, ts_output);
}
