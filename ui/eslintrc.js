// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import imported_globals from "globals";
import eslint_js from "@eslint/js";

import html from "@html-eslint/eslint-plugin";
import htmlParser from "@html-eslint/parser";

import babelParser from "@babel/eslint-parser";

import jsdoc from "eslint-plugin-jsdoc";

// TODO: add eslint-plugin-unused-imports for eslint-9
// TODO: validate tailwindcss checks in html and js

import { FlatCompat } from '@eslint/eslintrc';
const compat = new FlatCompat();

const OFF = 'off';

/** @type {import('eslint').Linter.FlatConfig[]} */
export default [
  eslint_js.configs.recommended,
  html.configs["flat/recommended"],
  ...compat.extends ('plugin:lit/recommended'),
  ...compat.extends ('plugin:tailwindcss/recommended'),

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

  {
    files: [ "**/*.js", "**/*.cjs", "**/*.mjs", ],
    plugins: {
      jsdoc,
    },

    languageOptions: {
      ecmaVersion: 2022,
      sourceType: "module",
      parser: babelParser,
      parserOptions: {
	requireConfigFile: false,
      },
      globals: {
	...imported_globals.browser,
	_: false,
	App: false,
	Ase: false,
	Data: false,
	Shell: false,
	CONFIG: false,
	__DEV__: false,
	debug: false,
	assert: false,
	log: false,
	host: false,
	module: false,
	process: false,
	require: false, // TODO: remove
      },
    },
    rules: {
    "no-restricted-globals": ["warn", "event", /*"error"*/ ],
    "no-empty": [ 'warn' ],
    "no-loss-of-precision": OFF,
    "no-unused-vars": OFF, // see unused-imports/no-unused-vars
    //"unused-imports/no-unused-vars": [ "warn", { args: "none", varsIgnorePattern: "^_.*" } ],
    //"unused-imports/no-unused-imports": OFF,
    "no-unreachable": [ "warn" ],
    semi: [ "warn", "always" ],
    "no-extra-semi": [ "warn" ],
    "no-console": [ OFF ],
    "no-constant-condition": [ OFF ],
    "no-constant-binary-expression": OFF,
    "no-debugger": [ "warn" ],
    indent: [ OFF, 2 ],
    "linebreak-style": [ "error", "unix" ],
     "lit/attribute-value-entities": OFF,
    "no-mixed-spaces-and-tabs": [ OFF ],
    'no-irregular-whitespace': OFF, /* ["error", { 'skipStrings': true, 'skipComments': true, 'skipTemplates': true, 'skipRegExps':true } ], */
    'no-useless-escape': OFF,
    'no-inner-declarations': OFF,
    // 'prefer-const': [ 'warn' ],
    // 'tailwindcss/no-custom-classname': OFF,
    quotes: [ OFF, "single" ],
    },
  },
];
