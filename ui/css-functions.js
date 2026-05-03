// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
import { parse } from "postcss-values-parser";

function to_delta (node)
{
  const raw = node.toString().trim();
  // Percent → convert to absolute delta
  if (raw.endsWith ("%"))
    return parseFloat (raw) * 0.01;
  // If numeric, treat as absolute delta
  const num = parseFloat (raw);
  if (!isNaN (num))
    return num;
  // Fallback → passthrough (produces invalid CSS, but warn and let browser handle it)
  console.warn (`css-functions: non-numeric amount "${raw}" in ${node.name}()`);
  return raw;
}

// All CSS functions must have test cases in ui/tests/css-tests.css
const fns = {
  lighten: (color, amt = 0.05) =>
    {
      const delta = to_delta (amt);
      // toFixed(15) + parseFloat: prevent floating-point drift (e.g. 0.1+0.2 → 0.30000000000000004)
      return `oklch(from ${color} calc(l + ${parseFloat (delta.toFixed (15))} ) c h)`;
    },
  darken: (color, amt = 0.05) =>
    {
      const delta = to_delta (amt);
      return `oklch(from ${color} calc(l - ${parseFloat (delta.toFixed (15))} ) c h)`;
    },
  desaturate: (color, amt = 0.05) =>
    {
      const factor = 1 - to_delta (amt);
      return `oklch(from ${color} l calc(c * ${parseFloat (factor.toFixed (15))} ) h)`;
    },
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
