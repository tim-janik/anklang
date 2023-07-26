// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import { LitComponent, html, JsExtract, docs } from '../little.js';

/** # B-BUTTONBAR
 * A container for tight packing of buttons.
 * ## Slots:
 * *slot*
 * : All contents passed into this element will be packed tightly and styled as buttons.
 */

// == STYLE ==
JsExtract.scss`
b-buttonbar {
  display: inline-flex; background-color: $b-button-border;
  border: 1px solid $b-button-border;
  border-radius: $b-button-radius;
  > button, > push-button {
    margin: 0 0 0 1px;
    &:first-of-type	{ margin-left: 0; }
    &:first-of-type	{ border-top-left-radius: $b-button-radius; border-bottom-left-radius: $b-button-radius; }
    &:last-of-type	{ border-top-right-radius: $b-button-radius; border-bottom-right-radius: $b-button-radius; }
  }
}`;

// == HTML ==
const HTML = html`
  <slot></slot>
`;

// == SCRIPT ==
class BButtonBar extends LitComponent {
  createRenderRoot() { return this; }
  render = () => HTML;
}
customElements.define ('b-buttonbar', BButtonBar);
