// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
import type { Plugin, ResolvedConfig, ModuleNode, ViteDevServer } from 'vite';
import { normalizePath, createFilter } from 'vite';
import * as babel from '@babel/core';
import * as t from '@babel/types';

const PLUGIN_NAME = 'vite-plugin-extra-css';

const debug = (...args) => undefined; // console.log (`${PLUGIN_NAME}:`, ...args);

function make_css_filename (fname) {
  const filepath = normalizePath (fname);
  return filepath + '.extra.css';
}

/**
 * Plugin Options
 */
interface ExtraCssOptions {
  tagName?: string | string[];	// tagged template function to extract CSS from
  include?: string | RegExp | (string | RegExp)[];	// include files for extraction
  exclude?: string | RegExp | (string | RegExp)[];	// exclude files from extraction
  cssPrefix?: string;		// prefix extracted CSS (e.g. for tailwind)
}

/**
 * Plugin Function
 */
export default function extraCssPlugin (options: ExtraCssOptions = {}): Plugin
{
  // Option defaults
  const tagNames = Array.isArray (options.tagName) ? options.tagName : [options.tagName || 'Extra_css'];
  const filter = createFilter (
    options.include || /\.[jt]sx?$/,
    options.exclude || /node_modules/
  );
  const css_prefix = options.cssPrefix === undefined ? '' : options.cssPrefix; // '@reference "tailwindcss";\n'
  // Map to store virtual CSS modules
  const cssModulesMap = new Map<string, string>();
  // Plugin definition
  return {
    name: PLUGIN_NAME,
    // configResolved (resolvedConfig) { config = resolvedConfig; },

    /** Resolve virtual CSS modules */
    resolveId (id)
    {
      const plain_id = id.replace (/\?.*/, '');
      if (!cssModulesMap.has (plain_id))
	return null;
      debug (`resolveId: ${id}`);
      return id;
    },

    /** Load virtual CSS modules */
    load (id)
    {
      const plain_id = id.replace (/\?.*/, '');
      let result = null;
      if (cssModulesMap.has (plain_id)) {
	result = cssModulesMap.get (plain_id);
	debug (`load: ${id}:\n${result}`);
      }
      return result;
    },

    /** Extract CSS from JS via AST */
    transform (code, id)
    {
      const contains_tag_name = haystack => {
	for (let i = 0; i < tagNames.length; i++)
	  if (haystack.includes (tagNames[i]))
	    return true;
	return false;
      };
      // Skip non matching files
      if (!filter (id) || !contains_tag_name (code))
        return null; // no need for AST parsing in these cases
      debug (`transform: ${id}`);

      let extractedCss = '';
      const filepath = normalizePath (id);
      const fileName = filepath.replace (/.*\//, ''); // avoid leaking build paths
      // For extraction from AST, use Babel as parser
      const ast = babel.parse (code, {
        sourceType: 'module',
        filename: filepath,
      });
      // Collect CSS from tagged template expressions
      const result = babel.transformFromAstSync (ast!, code, {
        plugins: [ () => ({
          visitor: {
            TaggedTemplateExpression (path)
	    {
              // Check if this is our CSS tag
              const tagName = path.node.tag.type === 'Identifier'
			    ? path.node.tag.name
			    : path.node.tag.type === 'MemberExpression' && path.node.tag.property.type === 'Identifier'
			    ? path.node.tag.property.name
			    : null;
              if (tagName && tagNames.includes (tagName)) {
                // Extract the template string content
                const quasis = path.node.quasi.quasis;
                let cssContent = '';
		const { start } = quasis[0].loc; // || path.node.loc || { start: {} }
		// if (start.line > 1)
		//   cssContent += '\n'.repeat (start.line - 1);
                for (let i = 0; i < quasis.length; i++) {
		  cssContent += quasis[i].value.raw;
                  if (i < quasis.length - 1)
                    cssContent += `/* DYNAMIC_VALUE_${i} */`; // Placeholder for dynamic values
                }
		const extracted_line = (extractedCss.match (/\n/g) || "").length + 1;
		// FIXME: console.error (fileName + ':' + start.line + ':', "ADD:", extracted_line - start.line);
		if (start.line > extracted_line)
		  extractedCss += '\n'.repeat (start.line - extracted_line);
		extractedCss += `/*${fileName}:${start.line}:${start.column}: ${tagName}:*/`;
                extractedCss += cssContent;
              }
            }
          }
        }) ],
        filename: filepath,
        sourceMaps: true,
        configFile: false,
        babelrc: false,
      });
      if (extractedCss) {
        // Create a unique ID for our virtual CSS module
        const virtualCssPath = make_css_filename (filepath);
        // Store the CSS in our map for the resolveId and load hooks
        cssModulesMap.set (virtualCssPath, `${css_prefix}${extractedCss}`);
        // Import the virtual CSS module to trigger Vite's CSS processing pipeline
        const importStatement = `\nimport "${virtualCssPath}?stamp=${ Date.now() }";\n`;
        return {
	  // modify code to import virtual CSS module
          code: code + importStatement,
          map: null
        };
      }
      return null; // No changes needed
    },

    /** Clear extracted CSS from cache */
    handleHotUpdate (ctx: {
      file: string;
      timestamp: number;
      modules: ModuleNode[];
      read: () => Promise<string>;
      server: ViteDevServer;
    })
    {
      if (ctx.file) {
        const virtualCssPath = make_css_filename (ctx.file);
	debug (`handleHotUpdate: ${ ctx.file }: ${virtualCssPath}`);
	cssModulesMap.delete (virtualCssPath);
      }
    },
  };
}
