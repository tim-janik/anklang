// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

module.exports = {
  "rules": {
    "alpha-value-notation": null,
    "at-rule-empty-line-before": null,
    "block-no-empty": null,
    "color-function-notation": null,
    "color-hex-length": null,
    "custom-property-empty-line-before": null,
    "declaration-block-no-shorthand-property-overrides": null,
    "declaration-block-single-line-max-declarations": null,
    "declaration-empty-line-before": null,
    "font-family-name-quotes": null,
    "length-zero-no-unit": null,
    "no-descending-specificity": null,
    "no-duplicate-selectors": null,
    "no-irregular-whitespace": null,
    "number-max-precision": null,
    "property-no-vendor-prefix": null,
    "rule-empty-line-before": null,
    "shorthand-property-no-redundant-values": null,
    "selector-class-pattern": [
      "^([a-z-][a-z0-9]*)(-[a-z0-9]+)*$",
      {
	"message": "Expected class selector to be kebab-case alike"
      }
    ]
  },
  "extends": "stylelint-config-standard"
};

