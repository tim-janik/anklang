// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class BToggle
 * @description
 * The <b-toggle> element implements a simple toggle button for boolean audio processor
 * input properties. Its `value` can be accessed as a property and `valuechange` is emitted
 * on changes.
 * ### Props:
 * *value*
 * : Boolean, the toggle value to be displayed, the values are `true` or `false`.
 * *label*
 * : String, label to be displayed inside the toggle button.
 * ### Events:
 * *valuechange*
 * : Event emitted whenever the value changes, which is provided as `event.target.value`.
 */

import { LitComponent, html, ref, JsExtract, docs } from '../little.js';
import * as Util from "../util.js";
import * as Kbd from '../kbd.js';
import * as ContextMenu from './contextmenu.js';

// == STYLE ==
JsExtract.scss`
b-toggle {
  display: flex; position: relative;
  margin: 0; padding: 0; text-align: center;
  user-select: none;
  .b-toggle-label {
    white-space: nowrap; overflow: hidden;
    height: 1.33em;
    &.b-toggle-empty { width: 2.2em; }
    align-self: center;
    border-radius: 3px;
    background-color: $b-toggle-0-bg;
    box-shadow: 0 0 3px #00000077;
  }
  .b-toggle-off {
    background: linear-gradient(177deg, $b-toggle-0-bh, $b-toggle-0-bl 20%, $b-toggle-0-bd);
    &.b-toggle-press:hover  	{ filter: brightness(90%); }
  }
  .b-toggle-on {
    background-color: $b-toggle-1-bg;
    background: linear-gradient(177deg, $b-toggle-1-bh, $b-toggle-1-bl 20%, $b-toggle-1-bd);
    &.b-toggle-press:hover	{ filter: brightness(95%); }
  }
}`;

// == HTML ==
const HTML = (t, d) => html`
<div ${ref (h => t.button_ = h)} class="b-toggle-label ${!t.label ? 'b-toggle-empty' : ''}"
     @pointerdown=${t.pointerdown} @pointerup=${t.pointerup} @dblclick=${t.dblclick} >
  ${t.label}
</div>
`;

// == SCRIPT ==
const BOOL_ATTRIBUTE = { type: Boolean, reflect: true }; // sync attribute with property
const STRING_ATTRIBUTE = { type: String, reflect: true }; // sync attribute with property
const PRIVATE_PROPERTY = { state: true };

class BToggle extends LitComponent {
  createRenderRoot() { return this; }
  render()
  {
    const d = {};
    return HTML (this, d);
  }
  static properties = {
    value: BOOL_ATTRIBUTE,
    label: STRING_ATTRIBUTE,
    buttondown_: PRIVATE_PROPERTY,
  };
  constructor()
  {
    super();
    this.value = false;
    this.buttondown_ = false;
    this.label = 'Toggle';
    this.button_ = null;
  }
  connectedCallback() {
    super.connectedCallback();
    this.setAttribute ('data-tip', "**CLICK** Toggle Value");
  }
  updated (changed_props)
  {
    if (changed_props.has ('value')) {
      this.button_.classList.remove (this.value ? 'b-toggle-off' : 'b-toggle-on');
      this.button_.classList.add (this.value ? 'b-toggle-on' : 'b-toggle-off');
    }
  }
  dblclick (event)
  {
    // prevent double-clicks from propagating, since we always
    // handled it as single click already
    event.preventDefault();
    event.stopPropagation();
  }
  pointerdown (event)
  {
    // trigger only on primary button press
    if (!this.buttondown_ && event.buttons == 1)
      {
	this.buttondown_ = true;
	this.button_.classList.add ('b-toggle-press');
	event.preventDefault();
	event.stopPropagation();
      }
  }
  pointerup (event)
  {
    if (this.buttondown_)
      {
	this.buttondown_ = false;
	this.button_.classList.remove ('b-toggle-press');
	event.preventDefault();
	event.stopPropagation();
	if (this.button_.matches (':hover')) {
	  this.value = !this.value;
	  this.dispatchEvent (new Event ('valuechange', { composed: true }));
	}
      }
  }
}
customElements.define ('b-toggle', BToggle);
