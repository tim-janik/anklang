// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class BMenuSeparator
 * @description
 * The <b-menuseparator> element is a menu element that serves as a visual separator between other elements.
 */

import { JsExtract, docs } from '../little.js';

// == STYLE ==
JsExtract.scss`
b-menuseparator {
  margin: calc(1em - 1px) 1em;
  border: 1px solid $b-menu-separator;
}
`;

// == SCRIPT ==
class BMenuSeparator extends HTMLHRElement {
}
customElements.define ('b-menuseparator', BMenuSeparator, { extends: 'hr' });
