// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

/** @notes
 * We put vite.config.ts under ui/ because during dev time, we only want ui/
 * to be watched by vite.
 * In order to support CSS-in-JS via template literals, we have to roll our own
 * plugin that extracts the CSS and feeds it back (via import) into Vite, so the
 * usual postcss processing chain is applied to it.
 */

import process from "node:process";
import path from "node:path";
import fs from 'node:fs';
import { defineConfig, loadEnv } from 'vite';
import solidPlugin from "vite-plugin-solid";
import tailwindcss from "@tailwindcss/vite";
import extra_css from './extra-css.ts';
import stylelint from 'stylelint';
import stylelintrc from './stylelintrc.cjs';
import postcssReporter from "postcss-reporter";

const BUILDDIR = path.resolve (process.env.BUILDDIR || 'out/');
const gen_path = path.resolve (__dirname, BUILDDIR + "/gen/");
console.log (`VITE: CWD=${process.cwd()} BUILDDIR=${BUILDDIR} gen_path=${gen_path}`);

// Plugin to inject __VITE_CONFIG__ into html
const html_inject_vite_config = (__DEV__: Boolean) => {
  build_config_json.__DEV__ = __DEV__;
  if (__DEV__) {
    // Note1: For development, ports are hard coded, synchronize port numbers with serve.sh
    build_config_json.ws_port = 1776;
  }
  return {
    name: 'html_inject_vite_config',
    transformIndexHtml (html, ctx) {
      return html.replace ('__VITE_CONFIG__', JSON.stringify (build_config_json));
    },
  }
};
const build_config_json = JSON.parse (fs.readFileSync (path.resolve (__dirname, BUILDDIR + "/version.json")), 'utf8');

// Try to improve CSS error messages for Extra_css``
function postcss_formatter (input)
{
  let filename = input.source, qualify = '';
  if (input.source.endsWith ('.extra.css')) {
    filename = input.source.replace (/\.extra\.css$/, '');
    qualify = ' (.extra.css):';
  }
  return input.messages.map (m =>
    `${filename}:${m.line}:${m.column}:${qualify} ${m}`
  ).join ('\n');
}

// Mode dependent vite config
function vite_config ({ mode })
{
  const __DEV__ = mode == 'development';
  console.log (`VITE: mode=${mode}`);
  return defineConfig ({
    root: "ui/",
    publicDir: gen_path + '/public',
    resolve: { alias: {
      "/gen": gen_path,
      "/ui": path.resolve (__dirname, "../ui/"),
    }, },
    // publicDir: "../public",
    server: {
      // open: "index.html",
      watch: {
	ignored: [ BUILDDIR.replace (/\/*$/, '/**') ],
      },
    },

    css: {
      devSourcemap: true,
      postcss: {
	plugins: [
          // FIXME: stylelint (stylelintrc),
	  postcssReporter ({
	    clearReportedMessages: true,
	    formatter: postcss_formatter,
	  })
	],
      },
    },

    build: {
      outDir: BUILDDIR + "/ui",
      target: 'esnext',
      minify: false,
      cssMinify: false,
      sourcemap: true,
      cssCodeSplit: true,
      rollupOptions: {
	// input: { app: 'index.html', },
      },
    },

    plugins: [
      tailwindcss(),
      extra_css(),
      solidPlugin(),
      html_inject_vite_config (__DEV__),
    ],
  });
}
export default vite_config;
