// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

/** == B-EDITABLE ==
 * Display an editable span.
 * ### Methods:
 * - **activate()** - Show input field and start editing.
 * ### Props:
 * - **clicks** - Set to 1 or 2 to activate for single or double click.
 * - **selectall** - Select all text on activation.
 * ### Events:
 * - **change** - Emitted when an edit ends successfully, text is in `event.detail.value`.
 */

import { createEffect } from 'solid-js';
import * as Util from "../util.js";

export function Editable (props: {
  value?: string | number;
  clicks?: number;
  class?: string;
  style?: string;
  onChange?: (e: CustomEvent) => void;
  selectall?: boolean;
  ref?: (el: HTMLInputElement) => void;
})
{
  let input_el: HTMLInputElement | undefined;

  // Coerce the (possibly numeric) value prop to a string for the input element.
  const value_string = () => String (props.value ?? '');

  const activate = () => {
    if (!input_el) return;
    input_el.inert = false;
    if (document.activeElement !== input_el) {
      input_el.focus();
      if (props.selectall) {
        input_el.select();
      } else {
        input_el.selectionEnd = input_el.selectionStart = 0;
        input_el.setSelectionRange (2**31, 2**31);
      }
    }
  };

  const handle_click = (event: MouseEvent) => {
    const clicks = 0 | (props.clicks ?? 1);
    if ((clicks === 1 && event.type === "click") ||
        (clicks === 2 && event.type === "dblclick")) {
      Util.prevent_event (event);
      activate();
    }
  };

  const handle_keydown = (event: KeyboardEvent) => {
    event.stopPropagation();
    const esc = Util.match_key_event (event, 'Escape');
    const enter = Util.match_key_event (event, 'Enter');
    if (esc || enter) {
      // Escape cancels (revert), Enter confirms (commit) the edit.
      handle_blur (null, !esc);
      // Move focus off the now-inert input; the resulting blur is ignored below.
      input_el?.blur();
    }
  };

  const handle_blur = (_event: FocusEvent | null, confirmed = true) => {
    if (!input_el) return;
    // Editing was already finished (via Enter/Escape); ignore the follow-up blur.
    if (input_el.inert) return;
    input_el.selectionEnd = input_el.selectionStart = 0;
    input_el.inert = true;
    input_el.scrollLeft = 0;
    if (!confirmed) {
      input_el.value = value_string();
    } else {
      props.onChange?.(new CustomEvent ('change', {
        detail: { value: input_el.value }
      }));
      input_el.value = value_string();
    }
  };

  // Sync external value changes when not being edited.
  createEffect (() => {
    const val = value_string();
    if (input_el && input_el.inert)
      input_el.value = val;
  });

  return (
    <span
      class={(props.class ?? '') + ' inline-flex'}
      style={props.style}
      onClick={handle_click}
      onDblClick={handle_click}
    >
      <input
        ref={el => {
          input_el = el;
          (el as any).activate = activate;
          props.ref?.(el);
        }}
        class="bg-dim-950 m-[2px] box-content h-min w-full p-0 leading-none text-inherit [&[inert]]:bg-transparent [&[inert]]:pointer-events-none"
        inert={true}
        onKeyDown={handle_keydown}
        onBlur={handle_blur}
        onChange={e => Util.prevent_event (e)}
      />
    </span>
  );
}
