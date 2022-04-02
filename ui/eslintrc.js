// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

const OFF = 'off';

module.exports = {
  env: {
    browser: true,
    es6: true,
    node: true },

  // babel-eslint as parser is needed for stage-3 syntax, see: https://stackoverflow.com/questions/60046847/eslint-does-not-allow-static-class-properties/60464446#60464446
  // parser: "babel-eslint",	// <- moved to parserOptions under vue-eslint-parser
  parserOptions: {
    parser: "babel-eslint",	// <- moved to parserOptions under vue-eslint-parser
    sourceType: "module" },

  globals: {
    App: false,
    Ase: false,
    Data: false,
    CONFIG: false,
    Util: false,
    Vue: false,
    __DEV__: false,
    debug: false,
    log: false,
    host: false,
    FinalizationRegistry: false,
    sfc_template: false,
    globalThis: false },

  rules: {
    "no-unused-vars": [ "warn", { args: "none", varsIgnorePattern: "^_.*" } ],
    "no-unreachable": [ "warn" ],
    semi: [ "error", "always" ],
    "no-extra-semi": [ "warn" ],
    "no-console": [ OFF ],
    "no-constant-condition": [ OFF ],
    "no-debugger": [ "warn" ],
    indent: [ OFF, 2 ],
    "linebreak-style": [ "error", "unix" ],
    "no-mixed-spaces-and-tabs": [ OFF ],
    'no-irregular-whitespace': OFF, /* ["error", { 'skipStrings': true, 'skipComments': true, 'skipTemplates': true, 'skipRegExps':true } ], */
    'no-useless-escape': OFF,
    'no-inner-declarations': OFF,
    // 'prefer-const': [ 'warn' ],
    "vue/no-unused-vars": OFF,
    "vue/mustache-interpolation-spacing": OFF,
    'vue/attributes-order': OFF,
    'vue/html-closing-bracket-newline': OFF,
    'vue/html-closing-bracket-spacing': OFF,
    'vue/html-indent': OFF,
    'vue/html-quotes': OFF,
    'vue/html-self-closing': OFF,
    'vue/max-attributes-per-line': OFF,
    'vue/multiline-html-element-content-newline': OFF,
    'vue/no-multi-spaces': OFF,
    'vue/singleline-html-element-content-newline': OFF,
    'vue/prop-name-casing': OFF,
    'vue/name-property-casing': OFF,
    'vue/require-default-prop': OFF,
    'vue/require-prop-types': OFF,
    'vue/require-prop-type-constructor': 'warn',
    'vue/component-tags-order': OFF,
    'vue/no-v-for-template-key': OFF,
    'vue/component-definition-name-casing': OFF,
    'vue/order-in-components': OFF,
    'vue/no-multiple-template-root': OFF,
    'vue/no-v-model-argument': OFF,
    'vue/v-slot-style': OFF,
    quotes: [ OFF, "single" ]
  },

  plugins: [ "html" ],

  extends: [
    "eslint:recommended",
    'plugin:vue/vue3-recommended'
  ]
};
