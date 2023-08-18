// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class BMenuTitle
 * @description
 * The <b-menutitle> element can be used as menu title inside a
 * [BContextMenu](contextmenu_8js.html#BContextMenu).
 * ### Slots:
 * *default*
 * : All contents passed into this slot will be rendered as contents of this element.
 */

import { LitComponent, html, JsExtract, docs } from '../little.js';
import * as Util from "../util.js";

// == STYLE ==
JsExtract.scss`
b-menutitle {
  display: inline-flex; flex: 0 0 auto; flex-flow: row nowrap;
  align-items: baseline;      //* distribute extra cross-axis space */
  margin: 0; padding: 5px 1em; text-align: left;
  background: transparent; cursor: pointer; user-select: none;
  border: 1px solid transparent;
  color: $b-menu-foreground;
  font-variant: small-caps; @include b-font-weight-bolder();
  // InterVariable-4.0beta9 has broken c2sc (small-caps), so for now use drop-caps
  // https://github.com/rsms/inter/issues/556#issuecomment-1598010623
  text-transform: uppercase; font-size: 80%;
  ::first-letter {
    font-size: 130%;
  }
}`;

// == HTML ==
const HTML = html`
<div class="b-menutitle">
  <span class="menulabel"><slot></slot></span>
</div>
`;

// == SCRIPT ==
class BMenuTitle extends LitComponent {
  render = () => HTML;
}
customElements.define ('b-menutitle', BMenuTitle);
