// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import { LitComponent, html, JsExtract, live, docs, ref } from '../little.js';
import * as Util from '../util.js';

/** @class BTextInput
 * @description
 * The <b-textinput> element is a field-editor for text input.
 *
 * ### Properties:
 * *value*
 * : Contains the text string being edited.
 * *readonly*
 * : Make this component non editable for the user.
 *
 * ### Events:
 * *valuechange*
 * : Event emitted whenever the value changes, which is provided as `event.target.value`.
 */

// == STYLE ==
JsExtract.scss`
b-textinput input {
  outline-width: 0; border: none; border-radius: $b-button-radius;
  text-align: left;
  padding-left: $b-button-radius; padding-right: $b-button-radius;
  @include b-style-inset;
}
`;

// <HTML/>
const HTML = t =>
html`
<label>
  <input ${ref (h => t.input_element = h)} type="text" ?readonly=${t.readonly}
	 style="width: 100%; min-width: 2.5em" @input=${t.handle_input}
	 placeholder=${t.placeholder} .value=${live (t.value)} >
</label>
`;

// <SCRIPT/>
class BTextInput extends LitComponent {
  createRenderRoot() { return this; }
  render() { return HTML (this); }
  input_element = null;
  static properties = {
    value:	 { type: String, },
    placeholder: { type: String, },
    readonly:	 { type: Boolean, },
  };
  constructor() {
    super();
    this.value = '';
    this.placeholder = '';
    this.readonly = false;
  }
  updated() {
    // debug ("textinput.js: value:", this.value, "live:", this.input_element.value);
  }
  handle_input (event) {	// emit 'input' with constrained value
    const constrainedvalue = this.constrain (this.input_element.value);
    if (constrainedvalue !== this.value) {
      this.value = constrainedvalue; // becomes Event.target.value
      this.dispatchEvent (new Event ('valuechange', { composed: true }));
    }
  }
  constrain (txt) {
    return '' + txt;
  }
}
customElements.define ('b-textinput', BTextInput);
