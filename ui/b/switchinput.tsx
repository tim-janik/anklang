// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class SwitchInput
 * @description
 * The SwitchInput component is a field-editor switch to change between on and off.
 * ### Props:
 * *value*
 * : Contains a boolean indicating whether the switch is on or off.
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
b-switchinput label, .b-switchinput label {
  position: relative; display: inline-block; width: 2.6em; height: 1.4em;
  input {
    opacity: 0; width: 0; height: 0;
    &:focus   + .b-switchinput-trough                     { box-shadow: var(--b-focus-box-shadow); }
    &:checked + .b-switchinput-trough                     { background-color: var(--b-switch-active); /*cursor: ew-resize;*/ }
    &:checked + .b-switchinput-trough::before             { opacity: 1; /* checkmark */ }
    &:checked + .b-switchinput-trough .b-switchinput-knob { transform: translateX(1.2em); }
  }
  .b-switchinput-knob {
    position: absolute; height: 1em; width: 1em; left: 0.2em; bottom: 0.2em;
    content: ""; transition: .3s; background-color: var(--b-switch-knob); border-radius: var(--b-button-radius);
  }
  .b-switchinput-trough {
    position: absolute; inset: 0;
    transition: .3s; background-color: var(--b-switch-inactive); border-radius: var(--b-button-radius);
    &::before {
      position: absolute; top: 0.1em; left: 0.3em;
      font-size: 1em; text-transform: none; text-decoration: none !important; speak: none;
      content: "✓"; transition: .3s; color: var(--b-switch-knob); opacity: 0;
    }
  }
}`;

// == COMPONENT ==
/**
 * A toggle switch input for boolean values.
 */
export function SwitchInput (props: {
  value?: boolean;
  readonly?: boolean;
  class?: string;
  'on:valuechange'?: (e: Event) => void;
})
{
  let label_ref: HTMLLabelElement | undefined;
  let checkbox_ref: HTMLInputElement | undefined;

  const [local, others] = splitProps (props, ['value', 'readonly', 'class']);
  const merged_class = local.class ? 'b-switchinput ' + local.class : 'b-switchinput';

  // Single source of truth: the constrained boolean value, used both for the
  // checkbox binding and for normalizing externally supplied (e.g. string) values.
  const value = () => constrain (local.value);

  function constrain (v: any): boolean
  {
    if (typeof (v) === "string") {
      if (v.length < 1 || v[0] == 'f' || v[0] == 'F' || v[0] == 'n' || v[0] == 'N')
        return false;
      return true;
    }
    return !!v;
  }

  function emit_input_value (inputvalue: any)
  {
    if (!label_ref) return;
    const boolvalue = constrain (inputvalue);
    if (local.value !== boolvalue) {
      (label_ref as any).value = boolvalue;
      label_ref.dispatchEvent (new Event ('valuechange', { composed: true, bubbles: true }));
    }
  }

  function keydown (event: KeyboardEvent)
  {
    // allow selection changes with LEFT/RIGHT/UP/DOWN
    if (local.readonly) return;
    if (event.keyCode == Util.KeyCode.LEFT || event.keyCode == Util.KeyCode.UP) {
      event.preventDefault();
      event.stopPropagation();
      if (checkbox_ref?.checked)
        checkbox_ref.click();
    }
    else if (event.keyCode == Util.KeyCode.RIGHT || event.keyCode == Util.KeyCode.DOWN) {
      event.preventDefault();
      event.stopPropagation();
      if (!checkbox_ref?.checked)
        checkbox_ref.click();
    }
  }

  const handle_change = (e: Event) => {
    const checked = (e.target as HTMLInputElement).checked;
    emit_input_value (checked);
  };

  // Constrain externally supplied values and notify the parent of the normalized boolean
  createEffect (() => {
    const raw = local.value;
    const v = value ();
    if (raw !== v)
      emit_input_value (v);
  });

  // Note checked - https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input/checkbox#checked
  return (
    <label class={merged_class} ref={label_ref} onKeyDown={keydown} {...others}>
      <input ref={checkbox_ref} type="checkbox" disabled={local.readonly}
             checked={value()}
             onChange={handle_change} />
      <span class="b-switchinput-trough"><span class="b-switchinput-knob"></span></span>
    </label>
  );
}
