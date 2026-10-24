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
const user_agent_restriction = {
  object: 'navigator', property: 'userAgent', message: 'Use browser features instead of the user agent.',
};

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
};

/** @type {import('eslint').Linter.FlatConfig[]} */
export default [
  eslint_js.configs.recommended,
  { ...html.configs["flat/recommended"], files: ['**/*.html'] },
  ...compat.extends ('plugin:better-tailwindcss/recommended').map (config => ({
    ...config, files: ['**/*.{js,mjs,cjs,html}'],
  })),
  {
    ignores: ['**/*.ts', '**/*.cts', '**/*.mts', '!ui/**/*.ts', '**/*.d.ts'],
  },

  // CJS files with node globals
  {
    files: [ "**/*.cjs",
	     "doc/jsdoc2md.js",
	     "electron/*.js",
	     "ui/sfc-compile.js",
	     "ui/xbcomments.js",
	     "misc/*.js",
	     "misc/*.mjs",
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
	     "ui/**/*.jsx",
	     "ui/**/*.ts",
	     "ui/**/*.tsx",
    ],
    plugins: common_js_plugins,
    rules: common_js_rules,
    languageOptions: {
      ecmaVersion: 2022,
      sourceType: "module", // Correct for ESM
      parser: babelParser,
      parserOptions: {
	requireConfigFile: false,
	babelOptions: { parserOpts: { plugins: ['jsx'] } },
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

  ...['ts', 'tsx'].map (extension => ({
    files: [`ui/**/*.${extension}`],
    languageOptions: {
      parserOptions: {
        babelOptions: { plugins: [['@babel/plugin-syntax-typescript', { isTSX: extension === 'tsx' }]] },
      },
    },
    rules: { 'no-undef': OFF },
  })),

  {
    files: ['**/*.{js,jsx,mjs,cjs,ts,tsx}'],
    ignores: ['ui/startup.js', 'ui/mouse.js'],
    rules: { 'no-restricted-properties': ['error', user_agent_restriction] },
  },

  {
    files: ['ui/b/*.{js,jsx,ts,tsx}'],
    rules: {
      'id-match': ['error',
        // Keep upstream Solid APIs and JSX callback names.
        '^([a-z_$][a-z0-9_$]*|[A-Z][a-zA-Z0-9_]*|create(Effect|Memo|Resource|Signal|Context)|' +
        'on(Mount|Cleanup|Change|ValueChange)|splitProps|useContext|mergeProps)$',
        { ignoreDestructuring: true }],
      'no-restricted-properties': ['error', user_agent_restriction,
        { object: 'App', property: 'project', message: 'Use Shell.project in components.' }],
    },
  },

  {
    files: ['ui/b/aboutdialog.jsx'],
    rules: {
      'no-restricted-properties': ['error',
        { object: 'App', property: 'project', message: 'Use Shell.project in components.' }],
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
      'better-tailwindcss/no-unregistered-classes': ['error', { ignore: ['^b-'] }],
    }
  },

  // CSS - tailwindcss settings
  {
    settings: {
      "better-tailwindcss": {
        entryPoint: "ui/tailwind.css",
      },
    },
  },
];
