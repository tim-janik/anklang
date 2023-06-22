// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

/** # B-MENUROW
 * Menuitems are packed horizontally inside a menurow.
 * ## Props:
 * *noturn*
 * : Avoid turning the icon-label direction in menu items to be upside down.
 * ## Slots:
 * *default*
 * : All contents passed into this slot will be rendered as contents of this element.
 */

import { LitComponent, html, JsExtract, docs } from '../little.js';

// == STYLE ==
const STYLE = await JsExtract.fetch_css (import.meta);
JsExtract.scss`
:host {
  display: inline-flex; flex: 0 0 auto; flex-wrap: nowrap; flex-direction: row;
  text-align: center; justify-content: center; align-items: baseline;
  margin: 0; padding: 0;
}`;

// == HTML ==
const HTML = () => html`
  <slot></slot>
`;

// == SCRIPT ==
const BOOL_ATTRIBUTE = { type: Boolean, reflect: true }; // sync attribute with property

class BMenuRow extends LitComponent {
  static styles = [ STYLE ];
  render = HTML;
  static properties = {
    noturn: BOOL_ATTRIBUTE,
  };
  constructor()
  {
    super();
    this.noturn = false;
  }
}

customElements.define ('b-menurow', BMenuRow);
