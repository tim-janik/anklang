// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class NumberInput
 * @description
 * The NumberInput component is a field-editor for integer or floating point number ranges.
 * The input `value` will be constrained to take on an amount between `min` and `max` inclusively.
 * ### Props:
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
 * ### Events:
 * *valuechange*
 * : Event emitted whenever the value changes, which is provided as `event.target.value`.
 */

import { createEffect, splitProps } from 'solid-js';
import * as Util from '../util.js';

// == STYLE ==
Extra_css`
b-numberinput, .b-numberinput {
  display: flex; justify-content: flex-end;
  flex-grow: 1;
  input[type='range'] {
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

// == COMPONENT ==
/**
 * A field-editor for integer or floating point number ranges.
 */
export function NumberInput (props: {
  value?: number;
  min?: number;
  max?: number;
  step?: number;
  allowfloat?: boolean;
  readonly?: boolean;
  class?: string;
  'on:valuechange'?: (e: Event) => void;
  [key: string]: any;
})
{
  let root_ref: (HTMLLabelElement & { value?: number }) | undefined;
  let slider_ref: HTMLInputElement | undefined;
  let number_ref: HTMLInputElement | undefined;

  const [local, others] = splitProps (props, ['value', 'min', 'max', 'step', 'allowfloat', 'readonly', 'class']);
  const merged_class = local.class ? 'b-numberinput ' + local.class : 'b-numberinput';

  // Reactive default bounds/step: single source of truth so constrain(),
  // slidersteps(), numberstyle() and the JSX attributes all stay in sync.
  const mn = () => local.min ?? -Number.MAX_SAFE_INTEGER;
  const mx = () => local.max ?? +Number.MAX_SAFE_INTEGER;
  const sp = () => local.step ?? 1;

  function constrain (v: number | string): number
  {
    v = parseFloat (v as string);
    if (isNaN (v))
      v = 0;
    if (!local.allowfloat)                       // use v|0 to cast to int
      v = 0 | (v > 0 ? v + 0.5 : v - 0.5);
    return Util.clamp (v, mn(), mx());
  }

  function slidersteps (): number                // approximate slider steps to slider pixels
  {
    if (!local.allowfloat)
      return 1;                                  // integer stepping for slider
    // slider float stepping, should roughly amount to the granularity the slider offers in pixels
    const sliderlength = 250;                   // uneducated approximation of maximum slider length
    const delta = Math.abs (mx() - mn());
    if (delta > 0) {
      const l10 = Math.log (10);
      const deltalog = Math.log (delta / sliderlength) / l10;
      const steps = Math.pow (10, Math.floor (deltalog));
      return steps;
    }
    return 0.1;                                  // float fallback
  }

  function numberstyle (): string                // determine width for numeric inputs
  {
    const l10 = Math.log (10);
    const minimum = 123456;                      // minimum number of digits always representable
    const delta = Math.max (minimum, Math.abs (mx()), Math.abs (mn()),
                            Math.abs (mx() - mn())) + Number (mn() < 0);
    const digits = Math.log (Math.max (delta, 618)) / l10;
    const em2digit = 0.9;
    const width = 0.5 + Math.ceil (digits * em2digit + 1); // margin + digits + spin-arrows
    return `width: 100%; max-width: ${width}em; min-width: 2em`;
  }

  function emit_input_value (inputvalue: number | string)   // emit 'valuechange' with constrained value
  {
    if (!root_ref) return;
    const constrainedvalue = constrain (inputvalue);
    const expected = String (constrainedvalue);
    if (number_ref && String (number_ref.value) != expected)
      number_ref.value = expected;
    if (slider_ref && String (slider_ref.value) != expected)
      slider_ref.value = expected;
    if (String (local.value) != expected) {
      root_ref.value = constrainedvalue;         // becomes Event.target.value
      root_ref.dispatchEvent (new Event ('valuechange', { composed: true, bubbles: true }));
    }
  }

  const handle_input = (e: Event) => {
    emit_input_value ((e.target as HTMLInputElement).value);
  };

  // Constrain externally supplied values and notify the parent of the normalized number
  createEffect (() => {
    const raw = local.value;
    const constrained = constrain (raw);
    const expected = String (constrained);
    if (slider_ref && String (slider_ref.value) != expected)
      slider_ref.value = expected;
    if (number_ref && String (number_ref.value) != expected)
      number_ref.value = expected;
    if (String (raw) != expected)                // enforce constrain() on outside changes
      emit_input_value (raw);
  });

  const val = local.value ?? 0;

  return (
    <label class={merged_class + ' tabular-nums'} ref={root_ref} {...others}>
      <input ref={slider_ref} type="range"
             tabindex={CONFIG.slidertabindex} min={mn()} max={mx()}
             step={slidersteps()} disabled={local.readonly}
             value={val}
             onInput={handle_input} />
      <input ref={number_ref} type="number" style={numberstyle()}
             min={mn()} max={mx()} step={sp()} readonly={local.readonly}
             value={val}
             onInput={handle_input} />
    </label>
  );
}
