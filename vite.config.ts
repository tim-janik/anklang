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
import { defineConfig, loadEnv, PluginOption } from 'vite';
import solidPlugin from "vite-plugin-solid";
import tailwindcss from "@tailwindcss/vite";
import extra_css from './ui/extra-css';
import stylelint from 'stylelint';
import stylelintrc from './ui/stylelintrc.cjs';
import postcssReporter from "postcss-reporter";

const BUILDDIR = path.resolve (process.env.BUILDDIR || 'out/');
const gen_path = path.resolve (BUILDDIR + "/gen/");
// Note: For development, ports are hard coded, synchronize port numbers with serve.sh
const DEVPORT_ANKLANG = process.env.DEVPORT_ANKLANG || 1776;
const DEVPORT_VITE = process.env.DEVPORT_VITE || 1777;
const DEVPORT_MKDOCS = process.env.DEVPORT_MKDOCS || 1778;

// Debug Info:
console.log (`VITE: CWD=${process.cwd()} BUILDDIR=${BUILDDIR} gen_path=${gen_path}`);

// Plugin to inject __VITE_CONFIG__ into html
const html_inject_vite_config = (__DEV__: Boolean) => {
  build_config_json.__DEV__ = __DEV__;
  if (__DEV__) {
    build_config_json.ws_port = DEVPORT_ANKLANG;
  }
  return {
    name: 'html_inject_vite_config',
    transformIndexHtml (html, ctx) {
      return html.replace ('__VITE_CONFIG__', JSON.stringify (build_config_json));
    },
  }
};
const build_config_json = JSON.parse (fs.readFileSync (path.resolve (BUILDDIR + "/version.json"), 'utf8'));

// Plugin to force full reloads if anything changed
const full_reload_always: PluginOption = {
  name: 'full-reload-always',
  handleHotUpdate ({ server }) {
    server.ws.send ({ type: "full-reload" });
    return [];
  },
} as PluginOption;
const maybe_full_reload_always = [];
// force full reloads
if (false)
  maybe_full_reload_always.push (full_reload_always);

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
      "/assets": gen_path + "/assets",
    }, },
    // publicDir: "../public",
    server: {
      // open: "index.html",
      proxy: {
	'/anklang/': {
          target: 'http://localhost:' + String (DEVPORT_MKDOCS) + '/',
          changeOrigin: true,               			// Adjusts the origin header
          // rewrite: (path) => path.replace (/^\/anklang\//, ''),	// Strip prefix if needed
          secure: false,                    			// For self-signed certificates
          ws: true,                         			// Enable WebSocket proxying
	},
      },
      watch: {
	ignored: [ BUILDDIR.replace (/\/*$/, '/**') ],
      },
    },

    css: {
      devSourcemap: true,
      postcss: {
	plugins: [
          // TODO: enable stylelint (stylelintrc),
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
      ...maybe_full_reload_always,
    ],

  });
}
export default vite_config;
