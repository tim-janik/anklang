// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
import { parse } from "postcss-values-parser";

const fns = {
};

function evaluate_function (node)
{
  const name = node.name;
  if (!fns[name])
    return node.toString(); // unknown → keep as-is
  // Extract arguments (css-tree-compatible)
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
      if (decl.value.includes ("__VITE_ASSET__") ||
	  /\bdata:/.test (decl.value))
	return;
      const ast = parse (decl.value);
      // Collect (old, new) pairs then do string replacement on decl.value
      ast.walk (node =>
	{
	  // https://github.com/shellscape/postcss-values-parser/blob/master/docs/Func.md
	  if (node.type === "func") {
	    node.value = evaluate_function (node);
	    node.type = "word";
	    delete node.name;
	    delete node.nodes;
	  }
      });
      decl.value = ast.toString();
    }
  });
export const postcss = true;
