// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
import { LitElement, html, css, postcss, live, docs, ref } from '../little.js';

/**
 * # B-FED-TEXT
 *
 * A field-editor for text input.
 *
 * ## Properties:
 * *value*
 * : Contains the text string being edited.
 * *readonly*
 * : Make this component non editable for the user.
 *
 * ## Events:
 * *input*
 * : This event is emitted whenever the value changes through user input or needs to be constrained.
 */

// <STYLE/>
const STYLE = await
postcss`
@import 'shadow.scss';
input {
  outline-width: 0; border: none; border-radius: $b-button-radius;
  text-align: left; background-color: rgba(255,255,255,.3); color: #fff;
  padding-left: $b-button-radius; padding-right: $b-button-radius;
  &:focus	{ box-shadow: $b-focus-box-shadow; }
  @include b-style-inset;
}
`;

// <HTML/>
const HTML = t =>
html`
<label class="b-fed-text">
  <input ${ref(e => t.input_element = e)} type="text" ?readonly=${t.readonly}
	 style="width: 100%; min-width: 2.5em" @input=${t.handle_input}
	 placeholder=${t.placeholder} .value=${live (t.value)} >
</label>
`;

// <SCRIPT/>
class BTextInput extends LitElement {
  render() { return HTML (this); }
  input_element = null;
  static styles = [ STYLE ];
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
  static shadowRootOptions = { ...LitElement.shadowRootOptions, delegatesFocus: true };
  updated() {
    // debug ("textinput.js: value:", this.value, "live:", this.input_element.value);
  }
  handle_input (event) {	// emit 'input' with constrained value
    const constrainedvalue = this.constrain (this.input_element.value);
    if (constrainedvalue !== this.value) {
      this.value = constrainedvalue; // becomes Event.target.value
      this.dispatchEvent (new Event ('input', { composed: true }));
    }
  }
  constrain (txt) {
    return '' + txt;
  }
}
customElements.define ('b-textinput', BTextInput);

// FIXME: RM: fed
