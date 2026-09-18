// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class TextInput
 * @description
 * The TextInput component is a field-editor for text input.
 *
 * ### Props:
 * *value*
 * : Contains the text string being edited. Used only when no `prop` is supplied;
 *   when a `prop` is given the display follows its backend value.
 * *placeholder*
 * : Placeholder string shown when the field is empty.
 * *readonly*
 * : Make this component non editable for the user.
 * *disabled*
 * : When `true` the field is treated as readonly (mirrored onto the native
 *   `<input readonly>` attribute and also blocks the file-picker click handler).
 * *prop*
 * : An extended property (`extend_property`); edits are forwarded via
 *   `prop.apply_()` and the display is refreshed on backend notifications.
 * *label*
 * : Forwarded onto the `<input>` as `aria-label`.
 * *title*
 * : Forwarded onto the root element as the native tooltip attribute.
 * *class*
 * : Extra CSS class(es) appended to the root `.b-textinput` element.
 *
 * ### Events:
 * *valuechange*
 * : Event emitted whenever the user edits the text and the value actually
 *   changes. The new value is provided as `event.target.value` (set on the
 *   root element before dispatch). When a `prop` is supplied the value is also
 *   pushed to the backend via `prop.apply_()`.
 */

import { splitProps } from 'solid-js';
import { local_input_value } from '../input';

// == STYLE ==
Extra_css`
@reference "../tailwind.css";
.b-textinput input {
  outline-width: 0; border: none;
  text-align: left;
  padding-left: var(--b-button-radius); padding-right: var(--b-button-radius);
  @apply b-style-inset;
}
`;

// == Helpers ==
function prop_info (prop, key) {
  const md = prop?.metadata;
  if (!md || !key) return "";
  const eq = key.length;
  for (let kv of md) {
    if (kv[eq] === '=' && kv.startsWith (key))
      return kv.substring (eq + 1);
  }
  return "";
}

// == COMPONENT ==
export function TextInput (props: {
  prop?: any;
  value?: string;
  placeholder?: string;
  readonly?: boolean;
  disabled?: boolean;
  label?: string;
  class?: string;
  ref?: (el: HTMLElement) => void;
  'on:valuechange'?: (e: Event) => void;
  [key: string]: any;
})
{
  let root_el: HTMLElement | undefined;
  let input_el: HTMLInputElement | undefined;
  // Last value reflected in the input field; used to emit `valuechange` only
  // on actual user changes (mirrors the NumberInput/SwitchInput pattern).
  let last_value = '';

  // `title` is intentionally *not* split out, so it flows through `{...others}`
  // onto the root element and becomes a native tooltip.
  const [local, others] = splitProps (props, [
    'prop', 'value', 'placeholder', 'readonly', 'disabled', 'label', 'class', 'ref',
  ]);

  const merged_class = 'b-textinput' + (local.class ? ' ' + local.class : '');

  function sync_display (value: string)
  {
    last_value = value;
    input_el.value = value;
  }

  const show_edit = local_input_value (() => (local.prop ? local.prop.value : local.value) ?? '', sync_display);

  const handle_input = () => {
    if (!input_el || !root_el) return;
    const value = input_el.value;
    if (value === last_value)
      return;
    show_edit (value);
    if (local.prop)
      local.prop.apply_ (value);
    (root_el as any).value = value;    // becomes Event.target.value
    root_el.dispatchEvent (new Event ('valuechange', { composed: true, bubbles: true }));
  };

  const textinput_click = async (event: MouseEvent) => {
    if (local.readonly || local.disabled) return;
    if (!prop_info (local.prop, "extensions"))
      return;
    const opt = {
      title:  _('Select File'),
      button: _('Open File'),
      cwd:    "~MUSIC",
      // TODO: filter by extensions
    };
    const filename = await Shell.select_file (opt);
    if (!filename)
      return;
    show_edit (filename);
    local.prop.value = filename;
  };

  return (
    <div class={merged_class} ref={el => {
      root_el = el;
      local.ref?.(el);
    }} {...others}>
      <label>
        <input ref={input_el} type="text"
          readonly={local.readonly || local.disabled}
          aria-label={local.label}
          style="width: 100%; min-width: 2.5em"
          onInput={handle_input}
          onClick={textinput_click}
          placeholder={local.placeholder ?? ''} />
      </label>
    </div>
  );
}
