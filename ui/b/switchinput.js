// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import { LitComponent, html, JsExtract, live, docs, ref } from '../little.js';
import * as Util from '../util.js';

/** @class BSwitchInput
 * @description
 * The <b-switchinput> element is a field-editor switch to change between on and off.
 * ### Properties:
 * *value*
 * : Contains a boolean indicating whether the switch is on or off.
 * *readonly*
 * : Make this component non editable for the user.
 * ### Events:
 * *valuechange*
 * : Event emitted whenever the value changes, which is provided as `event.target.value`.
 */

// == STYLE ==
JsExtract.scss`
b-switchinput label {
  position: relative; display: inline-block; width: 2.6em; height: 1.4em;
  input {
    opacity: 0; width: 0; height: 0;
    &:focus   + .b-switchinput-trough                     { box-shadow: $b-focus-box-shadow; }
    &:checked + .b-switchinput-trough                     { background-color: $b-switch-active; /*cursor: ew-resize;*/ }
    &:checked + .b-switchinput-trough::before             { opacity: 1; /* checkmark */ }
    &:checked + .b-switchinput-trough .b-switchinput-knob { transform: translateX(1.2em); }
  }
  .b-switchinput-knob {
    position: absolute; height: 1em; width: 1em; left: 0.2em; bottom: 0.2em;
    content: ""; transition: .3s; background-color: $b-switch-knob; border-radius: $b-button-radius;
  }
  .b-switchinput-trough {
    position: absolute; inset: 0;
    transition: .3s; background-color: $b-switch-inactive; border-radius: $b-button-radius;
    &::before {
      position: absolute; top: 0.1em; left: 0.3em;
      font-size: 1em; text-transform: none; text-decoration: none !important; speak: none;
      content: "\2713"; transition: .3s; color: $b-switch-knob; opacity: 0;
    }
  }
}`;

// <HTML/>
const HTML = t =>
html`
<label @keydown=${t.keydown} >
  <input ${ref (h => t.checkboxtype = h)} type="checkbox" ?disabled=${t.readonly}
         .checked=${!!t.value} @change=${e => t.emit_input_value (e.target.checked)} >
  <span class="b-switchinput-trough"><span class="b-switchinput-knob"></span></span>
</label>
`; // use .checked - https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input/checkbox#checked

// <SCRIPT/>
const BOOL_ATTRIBUTE = { type: Boolean, reflect: true };  // sync attribute with property
class BSwitchInput extends LitComponent {
  createRenderRoot() { return this; }
  render() { return HTML (this); }
  input_element = null;
  static properties = {
    value:	BOOL_ATTRIBUTE,
    readonly:	{ type: Boolean, },
  };
  constructor() {
    super();
    this.checkboxtype = null;
    this.value = false;
    this.readonly = false;
  }
  updated (changed_props)
  {
    if (changed_props.has ('value')) {
      // enforce constrain() on *outside* changes
      const boolvalue = this.constrain (this.value);
      if (this.value != boolvalue)
	this.emit_input_value (boolvalue);
    }
  }
  constrain (v)
  {
    if (typeof (v) === "string") {
      if (v.length < 1 || v[0] == 'f' || v[0] == 'F' || v[0] == 'n' || v[0] == 'N')
	return false;
      return true;
    }
    return !!v;
  }
  emit_input_value (inputvalue)           // emit 'input' with constrained value
  {
    const boolvalue = this.constrain (inputvalue);
    if (this.value != boolvalue) {
      this.value = boolvalue; // becomes Event.target.value
      this.dispatchEvent (new Event ('valuechange', { composed: true }));
    }
  }
  keydown (event)
  {
    // allow selection changes with LEFT/RIGHT/UP/DOWN
    if (event.keyCode == Util.KeyCode.LEFT || event.keyCode == Util.KeyCode.UP)
      {
	event.preventDefault();
	event.stopPropagation();
	this.checkboxtype.checked == false || this.checkboxtype.click();
      }
    else if (event.keyCode == Util.KeyCode.RIGHT || event.keyCode == Util.KeyCode.DOWN)
      {
	event.preventDefault();
	event.stopPropagation();
	this.checkboxtype.checked == true || this.checkboxtype.click();
      }
  }
}
customElements.define ('b-switchinput', BSwitchInput);
