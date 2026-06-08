// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import imported_globals from "globals";
import eslint_js from "@eslint/js";

import html from "@html-eslint/eslint-plugin";
import htmlParser from "@html-eslint/parser";

import babelParser from "@babel/eslint-parser";

import jsdoc from "eslint-plugin-jsdoc";

// TODO: validate tailwindcss checks in html and js
// TODO: fix config errors when enabling eslint-plugin-unused-imports@4.1.4

import { FlatCompat } from '@eslint/eslintrc';
const compat = new FlatCompat();
const OFF = 'off';

// Plugins for both CJS and ESM files
const common_js_plugins = {
  jsdoc,
};

// Rules for both CJS and ESM files
const common_js_rules = {
  indent: [ OFF, 2 ],
  quotes: [ OFF, "single" ],
  semi: [ "warn", "always" ],
  "@html-eslint/attrs-newline": OFF,
  "@html-eslint/element-newline": OFF,
  "@html-eslint/indent": OFF,
  "@html-eslint/no-extra-spacing-attrs": OFF,
  "@html-eslint/require-closing-tags": OFF,
  "@html-eslint/require-img-alt": OFF,
  "linebreak-style": [ "error", "unix" ],
  "lit/attribute-value-entities": OFF,
  "no-console": [ OFF ],
  "no-constant-binary-expression": OFF,
  "no-constant-condition": [ OFF ],
  "no-debugger": [ "warn" ],
  "no-empty": OFF,
  "no-extra-semi": [ "warn" ],
  "no-loss-of-precision": OFF,
  "no-mixed-spaces-and-tabs": [ OFF ],
  "no-restricted-globals": ["warn", "event", /*"error"*/ ],
  "no-unreachable": [ "warn" ],
  "no-unused-vars": OFF, // [ "warn" ],
  'no-inner-declarations': OFF,
  'no-irregular-whitespace': OFF, /* ["error", { 'skipStrings': true, 'skipComments': true, 'skipTemplates': true, 'skipRegExps':true } ], */
  'no-useless-escape': OFF,
  // 'prefer-const': [ 'warn' ],
  // 'tailwindcss/no-custom-classname': OFF,
};

/** @type {import('eslint').Linter.FlatConfig[]} */
export default [
  eslint_js.configs.recommended,
  html.configs["flat/recommended"],
  ...compat.extends ('plugin:lit/recommended'),
  ...compat.extends ('plugin:better-tailwindcss/recommended'),
  {
    ignores: [ "**/*.ts", "**/*.cts", "**/*.mts", ],
  },

  // CJS files with node globals
  {
    files: [ "**/*.cjs",
	     "doc/jsdoc2md.js",
	     "electron/*.js",
	     "ui/jsextract.js",
	     "ui/sfc-compile.js",
	     "ui/xbcomments.js",
	     "x11test/epuppeteer.mjs",
    ],
    plugins: common_js_plugins,
    rules: common_js_rules,
    languageOptions: {
      ecmaVersion: 2025,
      sourceType: "commonjs",
      globals: {
        ...imported_globals.node, // exports, require, module, process
      },
    },
  },

  // ESM files with browser globals
  {
    files: [ "**/*.js",
	     "**/*.mjs",
    ],
    plugins: common_js_plugins,
    rules: common_js_rules,
    languageOptions: {
      ecmaVersion: 2022,
      sourceType: "module", // Correct for ESM
      parser: babelParser,
      parserOptions: {
	requireConfigFile: false,
      },
      globals: {
        ...imported_globals.browser,
        // custom application globals
        _: false,
        App: false,
        Ase: false,
        Extra_css: false,
        Shell: false,
        CONFIG: false,
        __DEV__: false,
        debug: false,
        assert: false,
        log: false,
        host: false,
      },
    },
  },

  // HTML files
  {
    files: ["**/*.html"],
    languageOptions:		{ parser: htmlParser },
    plugins:			{ "@html-eslint": html },
    rules: {
      "@html-eslint/quotes": OFF, // TODO: "warn",
      "@html-eslint/indent": OFF,
      "@html-eslint/no-extra-spacing-attrs": OFF,
      "@html-eslint/attrs-newline": OFF,
    }
  },

  // CSS - tailwindcss settings
  {
    settings: {
      tailwindcss: {
        // whitelist: [/^b-/], // Regex for custom classes
      },
    },
  },
];
