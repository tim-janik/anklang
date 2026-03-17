// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import fs from "node:fs";
import path from "node:path";
import ts from "typescript";

function get_exported_functions (file_path: string)
{
  const source_text = fs.readFileSync (file_path, "utf8");
  const source = ts.createSourceFile (
    file_path,
    source_text,
    ts.ScriptTarget.Latest,
    true
  );

  const exports: { name: string; local: string }[] = [];

  function add (name: string, local: string = name)
  {
    exports.push ({ name, local });
  }

  ts.forEachChild (source, node => {
    // export function foo() {}
    if (
      ts.isFunctionDeclaration (node) &&
	node.name &&
	node.modifiers?.some (m => m.kind === ts.SyntaxKind.ExportKeyword)
    ) {
      add (node.name.text);
    }

    // export const foo = () => {}
    if (
      ts.isVariableStatement (node) &&
	node.modifiers?.some (m => m.kind === ts.SyntaxKind.ExportKeyword)
    ) {
      for (const decl of node.declarationList.declarations) {
        if (ts.isIdentifier (decl.name)) {
          // Only include if initializer is a function or arrow function
          if (
            decl.initializer &&
              (ts.isFunctionExpression (decl.initializer) ||
               ts.isArrowFunction (decl.initializer))
          ) {
            add (decl.name.text);
          }
        }
      }
    }

    // export default function foo() {}
    if (
      ts.isFunctionDeclaration (node) &&
	node.modifiers?.some (m => m.kind === ts.SyntaxKind.DefaultKeyword)
    ) {
      const name = node.name ? node.name.text : "default";
      add ("default", name);
    }

    // export default () => {}
    if (
      ts.isExportAssignment (node) &&
	(ts.isArrowFunction (node.expression) ||
	 ts.isFunctionExpression (node.expression))
    ) {
      add ("default", "default");
    }

    // export { foo, bar as baz }
    if (ts.isExportDeclaration (node) && node.exportClause) {
      if (ts.isNamedExports (node.exportClause)) {
        for (const spec of node.exportClause.elements) {
          const exported = spec.name.text;
          const local = spec.propertyName?.text ?? exported;
          add (exported, local);
        }
      }
    }
  });

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
