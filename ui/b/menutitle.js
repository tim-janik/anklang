// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { LitElement, html, JsExtract, docs } from '../little.js';

/** # B-MENUTITLE
 * An element to be used as menu title.
 * ## Slots:
 * *default*
 * : All contents passed into this slot will be rendered as contents of this element.
 */

// == STYLE ==
const STYLE = await JsExtract.fetch_css (import.meta);
JsExtract.scss`
@import 'mixins.scss';
:host { // b-menutitle
  display: inline-flex; flex: 0 0 auto; flex-wrap: nowrap; flex-direction: row;
  align-items: baseline;      //* distribute extra cross-axis space */
  margin: 0; padding: 5px 1em; text-align: left;
  background: transparent; cursor: pointer; user-select: none;
  border: 1px solid transparent;
  color: $b-menu-foreground;
  font-variant: small-caps; font-weight: bolder;
}
`;

// == HTML ==
const HTML = html`
<div class="b-menutitle">
<span class="menulabel"><slot /></span>
</div>
`;

// == SCRIPT ==
class BMenuTitle extends LitElement {
  render = () => HTML;
  static styles = [ STYLE ];
}
customElements.define ('b-menutitle', BMenuTitle);
