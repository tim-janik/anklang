// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
import { parse } from "postcss-values-parser";

const fns = {
};

function evaluate_function (node)
{
  const name = node.name;
  if (!fns[name])
    return node.toString(); // unknown → keep as-is
  // Extract arguments (css-tree similar)
  const args =
    node.nodes
	.filter (n => n.type !== "operator") // ignore commas
	.map (n => n.type === "func" ? evaluate_function (n) : n.toString());
  return fns[name] (...args);
}

export default () =>
  ({
    postcssPlugin: "css-functions",
    Declaration (decl)
    {
      // Skip values with Vite-transformed assets
      if (decl.value.includes ("__VITE_ASSET__") ||     // Vite build-time placeholder
	  /\bdata:/.test (decl.value) ||                // Vite dev-mode inlining
	  decl.value.includes ("/@fs/") ||              // Vite dev-server URL prefix
	  /var\([^)]*\/[^)]*\)/.test (decl.value))      // Ignore TW var(--color-dim-800/50)
	return;
      // console.log ("css-functions:", decl.source.input.from, decl.source.start.line, decl.prop, JSON.stringify (decl.value.substring (0, 80)));
      const ast = parse (decl.value);
      let changed = false;
      ast.walk (node =>
	{
	  // https://github.com/shellscape/postcss-values-parser/blob/master/docs/Func.md
	  if (node.type === "func") {
            const result = evaluate_function (node);
            if (result !== node.toString()) {
              changed = true;
              node.value = result;
              node.type = "word";
              delete node.name;
              delete node.nodes;
            }
	  }
      });
      if (changed) // Avoid rewriting (and normalizing) unrelated CSS
	decl.value = ast.toString();
    }
  });
export const postcss = true;
