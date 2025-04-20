// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

module.exports = {
  extends: [ "stylelint-config-standard" ],
  ignoreFiles: [
    "../**/gen/**/*.css",
  ],
  plugins: [
    { // https://github.com/stylelint/stylelint/issues/8524
      ruleName: 'my/no-standalone-custom-properties',
      rule: function (primaryOption, secondaryOptions) {
        return function (root, result) {
          root.walkDecls (decl => {
	    if (decl.prop && decl.prop.startsWith ('--') && // CSS var
		! ['rule', 'atrule'].includes (decl.parent.type)) // outside CSS rule
	      {
		if (0) console.error (`CSS custom property must be declared inside a selector: ${decl.prop}\ndecl.parent.type: ${JSON.stringify (decl.parent.type)}`);
		result.warn (`CSS custom property must be declared inside a selector: ${decl.prop}`, {
                  node: decl,
                  message: `Encountered standalone custom property: ${decl.prop}: ${decl.value}`
		});
              }
          });
        };
      }
    },
  ],
  rules: {
    'hue-degree-notation': null,
    'selector-pseudo-element-colon-notation': null,
    'my/no-standalone-custom-properties': true,
    "selector-type-no-unknown": [ true, { "ignoreTypes": [ /b-.*/ ] }],
    'at-rule-no-unknown': null, // true, // [ true, { ignoreAtRules: [ 'tailwind', 'apply', 'variants', 'responsive', 'screen' ] },
    'alpha-value-notation': null,
    'at-rule-empty-line-before': null,
    'block-no-empty': null,
    // 'color-function-notation': null,
    'color-hex-length': null,
    'comment-empty-line-before': null,
    'comment-whitespace-inside': null,
    'custom-property-empty-line-before': null,
    // 'declaration-block-no-shorthand-property-overrides': null,
    'declaration-block-single-line-max-declarations': null,
    'declaration-empty-line-before': null,
    'font-family-name-quotes': null,
    'import-notation': null,
    'length-zero-no-unit': null,
    'no-descending-specificity': null,
    'no-duplicate-selectors': null,
    // 'no-invalid-position-at-import-rule': null,
    'no-irregular-whitespace': null,
    'number-max-precision': null,
    'property-no-vendor-prefix': null,
    'rule-empty-line-before': null,
    'selector-class-pattern': [ "^([a-z\\][a-z\\0-9]*)(-[a-z\\0-9]+)*$", { message: 'Expected class selector to be kebab-case alike' } ],
    'shorthand-property-no-redundant-values': null,
    'value-keyword-case': null,
  },
};
