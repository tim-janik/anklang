// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import { LitComponent, html, JsExtract, docs } from '../little.js';
import * as Util from "../util.js";

/** # B-MENUTITLE
 * An element to be used as menu title.
 * ## Slots:
 * *default*
 * : All contents passed into this slot will be rendered as contents of this element.
 */

// == STYLE ==
const STYLE_URL = await JsExtract.css_url (import.meta);
JsExtract.scss`
@import 'mixins.scss';
:host { // b-menutitle
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
}
`;

// == HTML ==
const HTML = html`
<div class="b-menutitle">
  <span class="menulabel"><slot></slot></span>
</div>
`;

// == SCRIPT ==
class BMenuTitle extends LitComponent {
  render = () => HTML;
  createRenderRoot()
  {
    const rroot = super.createRenderRoot();
    Util.add_style_sheet (rroot, STYLE_URL);
    return rroot;
  }
}
customElements.define ('b-menutitle', BMenuTitle);
