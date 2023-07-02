// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import { LitComponent, html, JsExtract, docs } from '../little.js';

/** # B-MENUSEPARATOR
 * A menu element that serves as a visual separator between other elements.
 */

// == STYLE ==
const STYLE = await JsExtract.fetch_css (import.meta);
JsExtract.scss`
@import 'mixins.scss';
hr {
  border-color: $b-menu-separator;
}
`;

// == HTML ==
const HTML = html`
  <hr class="b-menuseparator" />
`;

// == SCRIPT ==
class BMenuSeparator extends LitComponent {
  render = () => HTML;
  static styles = [ STYLE ];
}
customElements.define ('b-menuseparator', BMenuSeparator);
