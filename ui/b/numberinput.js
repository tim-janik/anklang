// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import { LitComponent, html, JsExtract, live, docs, ref } from '../little.js';
import * as Util from '../util.js';

/** # B-FED-NUMBER
 * A field-editor for integer or floating point number ranges.
 * The input `value` will be constrained to take on an amount between `min` and `max` inclusively.
 * ## Properties:
 * *value*
 * : Contains the number being edited.
 * *min*
 * : The minimum amount that `value` can take on.
 * *max*
 * : The maximum amount that `value` can take on.
 * *step*
 * : A useful amount for stepwise increments.
 * *allowfloat*
 * : Unless this setting is `true`, numbers are constrained to integer values.
 * *readonly*
 * : Make this component non editable for the user.
 * ## Events:
 * *valuechange*
 * : This event is emitted whenever the value changes through user input or needs to be constrained.
 */

// <STYLE/>
JsExtract.scss`
b-numberinput {
  display: flex; justify-content: flex-end;
  label {
    display: flex; justify-content: flex-end;
    flex-grow: 1;
  }
  input[type='range'] {
    flex-grow: 1;
    margin: auto 1em auto 0;
    @include b-style-hrange-input;
    flex: 1 1 auto;  /* grow beyond minimum width */
    max-width: 50%;  /* avoid excessive sizes */
    width: 1.5em;    /* minimum width */
  }
  input[type='number'] {
    text-align: right;
    outline-width: 0; border: none;
    @include b-style-number-input;
  }
}`;

// <HTML/>
const HTML = t =>
html`
<label class="tabular-nums">
  <input ${ref (h => t.slidertype = h)} type="range"
         tabindex=${CONFIG.slidertabindex} min=${t.min} max=${t.max}
         step=${t.slidersteps()} ?disabled=${t.readonly}
         .value=${t.value} @input=${e => t.emit_input_value (e.target.value)} >
  <input ${ref (h => t.numbertype = h)} type="number" style=${t.numberstyle()}
         min=${t.min} max=${t.max} step=${t.step} ?readonly=${t.readonly}
         .value=${t.value} @input=${e => t.emit_input_value (e.target.value)} >
</label>
`;

// <SCRIPT/>
class BNumberInput extends LitComponent {
  createRenderRoot() { return this; }
  render() { return HTML (this); }
  static properties = {
    value:	{ type: [String, Number] },
    allowfloat:	{ type: Boolean, },
    readonly:	{ type: Boolean, },
    min:	{ type: Number, },
    max:	{ type: Number, },
    step:	{ type: Number, },
  };
  constructor() {
    super();
    this.value = 0;
    this.allowfloat = false;
    this.readonly = false;
    this.min = -Number.MAX_SAFE_INTEGER;
    this.max = +Number.MAX_SAFE_INTEGER;
    this.step = 1;
    this.slidertype = null;
    this.numbertype = null;
  }
  updated (changed_props)
  {
    if (changed_props.has ('value')) {
      // enforce constrain() on *outside* changes
      const constrainedvalue = this.constrain (this.value);
      if (this.value != constrainedvalue) {
	const expected = String (constrainedvalue);
	if (expected != String (this.value))
	  this.emit_input_value (this.value);   // enforce constrain()
      }
    }
  }
  constrain (v)
  {
    v = parseFloat (v);
    if (isNaN (v))
      v = 0;
    if (!this.allowfloat)                       // use v|0 to cast to int
      v = 0 | (v > 0 ? v + 0.5 : v - 0.5);
    return Util.clamp (v, this.min, this.max);
  }
  slidersteps()                                 // aproximate slider steps to slider pixels
  {
    if (!this.allowfloat)
      return 1;                                 // use integer stepping for slider
    // slider float stepping, should roughly amount to the granularity the slider offers in pixels
    const sliderlength = 250;                   // uneducated approximation of maximum slider length
    const delta = Math.abs (this.max - this.min);
    if (delta > 0)
      {
        const l10 = Math.log (10);
        const deltalog = Math.log (delta / sliderlength) / l10;
        const steps = Math.pow (10, Math.floor (deltalog));
        return steps;
      }
    return 0.1;                                 // float fallback
  }
  numberstyle()                                 // determine width for numeric inputs
  {
    const l10 = Math.log (10);
    const minimum = 123456;                     // minimum number of digits always representable
    const delta = Math.max (minimum, Math.abs (this.max), Math.abs (this.min),
                            Math.abs (this.max - this.min)) + Number (this.min < 0);
    const digits = Math.log (Math.max (delta, 618)) / l10;
    const em2digit = 0.9;
    const width = 0.5 + Math.ceil (digits * em2digit + 1); // margin + digits + spin-arrows
    return `width: 100%; max-width: ${width}em; min-width: 2em`;
  }
  emit_input_value (inputvalue)                 // emit 'input' with constrained value
  {
    const constrainedvalue = this.constrain (inputvalue);
    const expected = String (constrainedvalue);
    if (this.numbertype && String (this.numbertype.value) != expected)
      this.numbertype.value = expected;
    if (this.slidertype && String (this.slidertype.value) != expected)
      this.slidertype.value = expected;
    if (String (this.value) != expected) {
      this.value = constrainedvalue; // becomes Event.target.value
      this.dispatchEvent (new Event ('valuechange', { composed: true }));
    }
  }
}
customElements.define ('b-numberinput', BNumberInput);
