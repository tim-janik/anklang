// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class Toggle
 * @description
 * The Toggle component is a simple toggle button for boolean audio processor input
 * properties.
 * ### Props:
 * *value*
 * : Boolean, the toggle value to be displayed, the values are `true` or `false`.
 * *label*
 * : String, label to be displayed inside the toggle button. Defaults to an empty
 * string (the control renders as a bare nub, via the `b-toggle-empty` class).
 * *disabled*
 * : Boolean, disables interaction and visually dims the control.
 * *onValueChange*
 * : `onValueChange?: (value: boolean) => void` — callback invoked with the new
 * boolean value when the user toggles the control.
 * ### Events:
 * *valuechange*
 * : Event emitted whenever the value changes, which is provided as
 * `event.target.value`.
 */

import { createEffect, createSignal, splitProps } from 'solid-js';

// == STYLE ==
Extra_css`
b-toggle, .b-toggle {
  display: flex; position: relative;
  margin: 0; padding: 0; text-align: center;
  user-select: none;
  &[aria-disabled="true"] { opacity: 0.5; cursor: not-allowed; pointer-events: none; }
  .b-toggle-label {
    white-space: nowrap; overflow: hidden;
    height: 1.33em;
    &.b-toggle-empty { width: 2.2em; }
    align-self: center;
    border-radius: 3px;
    background-color: var(--b-toggle-0-bg);
    box-shadow: 0 0 3px #00000077;
  }
  .b-toggle-off {
    background: linear-gradient(177deg, var(--b-toggle-0-bh), var(--b-toggle-0-bl) 20%, var(--b-toggle-0-bd));
    &.b-toggle-press:hover  	{ filter: brightness(90%); }
  }
  .b-toggle-on {
    background: var(--b-toggle-1-bg);
    background: linear-gradient(177deg, var(--b-toggle-1-bh), var(--b-toggle-1-bl) 20%, var(--b-toggle-1-bd));
    &.b-toggle-press:hover	{ filter: brightness(95%); }
  }
}`;

// == COMPONENT ==
export function Toggle (props: {
  value?: boolean;
  label?: string;
  disabled?: boolean;
  class?: string;
  onValueChange?: (value: boolean) => void;
  'on:valuechange'?: (e: Event) => void;
  [key: string]: any;
})
{
  let root_el: HTMLDivElement | undefined;
  let label_el: HTMLDivElement | undefined;
  let pressed = -1;

  const [local, others] = splitProps (props, ['value', 'label', 'disabled', 'class', 'onValueChange']);
  const merged_class = () => local.class ? 'b-toggle ' + local.class : 'b-toggle';

  // Internal state mirrors the external `value` prop and is flipped optimistically
  // on user interaction, so two rapid clicks toggle twice instead of computing the
  // next value from a prop that lags a full IPC round-trip (ChoiceInput precedent).
  const [value_, set_value_] = createSignal (!!local.value);
  createEffect (() => set_value_ (!!local.value));

  const handle_pointerdown = (event: PointerEvent) => {
    if (local.disabled) return;
    if (pressed < 0 && event.buttons === 1) {
      pressed = event.buttons;
      label_el?.classList.add ('b-toggle-press');
      event.preventDefault();
      event.stopPropagation();
    }
  };

  const handle_pointerup = (event: PointerEvent) => {
    if (local.disabled) return;
    if (pressed >= 0) {
      pressed = -1;
      label_el?.classList.remove ('b-toggle-press');
      event.preventDefault();
      event.stopPropagation();
      // pointerup fires on this element only when released over it (no pointer
      // capture is used), so the old `:hover` guard was redundant and, worse,
      // never matched under headless/scripted input — hence dropped here.
      const new_val = !value_();
      set_value_ (new_val);
      if (root_el) {
        (root_el as any).value = new_val;
        root_el.dispatchEvent (new Event ('valuechange', { composed: true }));
      }
      local.onValueChange?.(new_val);
    }
  };

  const handle_dblclick = (event: MouseEvent) => {
    // prevent double-clicks from propagating, since we always
    // handled it as single click already
    event.preventDefault();
    event.stopPropagation();
  };

  return (
    <div class={merged_class()} ref={el => { root_el = el; }} {...others}
      onPointerDown={handle_pointerdown}
      onPointerUp={handle_pointerup}
      onDblClick={handle_dblclick}
      data-tip="**CLICK** Toggle Value"
      aria-disabled={local.disabled || undefined}
    >
      <div class="b-toggle-label" ref={label_el}
        classList={{ 'b-toggle-on': value_(), 'b-toggle-off': !value_(), 'b-toggle-empty': !local.label }}>
        {local.label ?? ''}
      </div>
    </div>
  );
}