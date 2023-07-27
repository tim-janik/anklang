// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import { JsExtract, docs } from '../little.js';

/** # B-MENUSEPARATOR
 * A menu element that serves as a visual separator between other elements.
 */

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
customElements.define ('b-menuseparator', BMenuSeparator);
