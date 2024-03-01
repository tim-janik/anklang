// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

const OFF = 'off';

module.exports = {
  env: {
    es2022: true,
    browser: true,
    node: true },

  // parser: "babel-eslint", // <- moved to parserOptions under vue-eslint-parser (eslint-plugin-vue)
  parserOptions: {
    parser: "@babel/eslint-parser",
    requireConfigFile: false,
    sourceType: "module" },

  globals: {
    _: false,
    App: false,
    Ase: false,
    Data: false,
    Shell: false,
    CONFIG: false,
    Vue: false,
    __DEV__: false,
    debug: false,
    assert: false,
    log: false,
    host: false,
    start_view_transition: false,
    sfc_template: false },

  rules: {
    "no-restricted-globals": ["warn", "event", /*"error"*/ ],
    "no-empty": [ 'warn' ],
    "no-loss-of-precision": OFF,
    "no-unused-vars": OFF, // see unused-imports/no-unused-vars
    "unused-imports/no-unused-vars": [ "warn", { args: "none", varsIgnorePattern: "^_.*" } ],
    "unused-imports/no-unused-imports": OFF,
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
    'vue/no-v-html': OFF,
    'vue/multi-word-component-names': OFF,
    'vue/first-attribute-linebreak': OFF,
    quotes: [ OFF, "single" ]
  },

  plugins: [ "html", "unused-imports" ],

  extends: [
    "eslint:recommended",
    "plugin:lit/recommended",
    'plugin:vue/vue3-recommended'
  ]
};
