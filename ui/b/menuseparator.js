// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { LitComponent, html, css, postcss, docs } from '../little.js';

/** # B-MENUSEPARATOR
 * A menu element that serves as a visual separator between other elements.
 */

// == STYLE ==
const STYLE = await postcss`
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
