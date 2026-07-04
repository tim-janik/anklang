// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class BChoiceInput
 * @description
 * The <b-choiceinput> element provides a choice popup to choose from a set of options.
 * It supports the Vue
 * [v-model](https://vuejs.org/v2/guide/components-custom-events.html#Customizing-Component-v-model)
 * protocol by emitting an `input` event on value changes and accepting inputs via the `value` prop.
 * ### Props:
 * *value*
 * : The choice value to be displayed.
 * *choices*
 * : List of choices: `[ { icon, label, blurb }... ]`
 * *title*
 * : Optional title for the popup menu.
 * *label*
 * : A label used to extend the tip attached to the choice component.
 * *small*
 * : Reduce padding and use small layout.
 * *prop*
 * : If `label` is unspecified, it can be fetched from `prop->label` instead.
 * ### Events:
 * *valuechange*
 * : Event emitted whenever the value changes, which is provided as `event.target.value`.
 */

import { createEffect, createMemo, createSignal, For, onCleanup, splitProps } from 'solid-js';
import * as Util from '../util.js';
import { get_uri } from '../dom.js';
import { ContextMenu } from './contextmenu';
import { MenuTitle } from './menutitle.tsx';

// <STYLE/>
Extra_css`
@reference "../tailwind.css";
b-choiceinput, .b-choiceinput {
  display: flex;
  flex-basis: auto;
  flex-flow: row nowrap;
  align-items: stretch;
  align-content: stretch;
  position: relative;
  margin: 0;
  white-space: nowrap;
  user-select: none;
  &[aria-disabled="true"] { opacity: 0.5; cursor: not-allowed; pointer-events: none; }
  &.b-choice-big {
    justify-content: left; text-align: left;
    padding: .1em 0;
  }
  &.b-choice-small {
    justify-content: center; text-align: center;
    padding: 0;
  }
  b-objecteditor &.b-choice, .b-objecteditor &.b-choice {
    text-align: left;
    justify-content: left;
    padding: 0;
    flex: 1 1 auto;
  }
  .-current {
    @apply flex-auto shrink grow basis-auto overflow-hidden text-ellipsis whitespace-nowrap px-2;
    width: 1em;
  }
  .-arrow {
    flex: 0 0 auto; width: 1em;
    margin: 0 0 0 .3em;
  }
  &.b-choice-small .-arrow {
    display: none;
  }
  .b-choice-current {
    align-self: center;
    width: 100%;
    margin: 0;
    white-space: nowrap; overflow: hidden;
    .b-choice-big & {
      flex-grow: 1;
      justify-content: space-between;
      padding: var(--b-button-radius) 0 var(--b-button-radius) .5em;
    }
    .b-choice-small & {
      width: 100%; height: 1.33em;
      justify-content: center;
      padding: 2px;
    }
    @include b-style-outset();
  }
}
.b-choiceinput-contextmenu {
  .b-choice-label { display: block; white-space: pre-line; }
  .b-choice-line1,
  .b-choice-line2 { display: block; white-space: pre-line; font-size: 90%; color: var(--b-style-fg-secondary); }
  .b-choice-line3 { display: block; white-space: pre-line; font-size: 90%; color: var(--b-style-fg-notice); }
  .b-choice-line4 { display: block; white-space: pre-line; font-size: 90%; color: var(--b-style-fg-warning); }
  button {
    &:focus, &.active, &:active {
      .b-choice-line1, .b-choice-line2, .b-choice-line3,
      .b-choice-line4 { filter: var(--b-style-fg-filter); } /* adjust to inverted menuitem */
    } }
  button {
    white-space: pre-line;
  }
}
.b-choiceinput-contextmenu button {
  grid-template-columns: min-content 1fr min-content;
  justify-items: start;
  b-icon, .b-icon { @apply col-start-1 row-start-1; justify-content: start; }
  span   { @apply col-start-2; }
  kbd    { @apply col-start-3 row-start-1; }
}
`;

// == COMPONENT ==
export function ChoiceInput (props: {
  value?: string;
  choices?: any[];
  title?: string;
  label?: string;
  small?: boolean;
  prop?: any;
  disabled?: boolean;
  class?: string;
  ref?: (el: HTMLElement) => void;
  'on:valuechange'?: (e: Event) => void;
  [key: string]: any;
})
{
  let root_el: HTMLElement | undefined;
  let pophere_el: HTMLElement | undefined;
  let cmenu_el: any | undefined;

  const [local, others] = splitProps (props, [
    'value', 'choices', 'title', 'label', 'small', 'prop', 'disabled', 'class', 'ref',
  ]);
  // Reactive class binding: covers small/big layout and caller-supplied `class`.
  const merged_class = () => 'b-choiceinput ' + (local.small ? 'b-choice-small' : 'b-choice-big') +
			     (local.class ? ' ' + local.class : '');

  const [value_, set_value_] = createSignal (local.value ?? '');
  const [choices_, set_choices_] = createSignal<any[]> ([]);
  const [need_cmenu, set_need_cmenu] = createSignal (false);

  // Sync external value
  createEffect (() => {
    set_value_ (local.value ?? '');
  });

  // Subscribe to backend property changes so the selected choice stays fresh
  // (undo/redo, preset loads, the reset button, other views). Mirrors the
  // TextInput subscription pattern (textinput.tsx); `prop.value_` is not
  // Solid-tracked, so a notify subscription is required.
  createEffect (() => {
    const prop = local.prop;
    if (!prop || !prop.addnotify_)
      return;
    const notify_cb = () => set_value_ (prop.value_.val ?? '');
    prop.addnotify_ (notify_cb);
    onCleanup (() => {
      prop.delnotify_ (notify_cb);
    });
  });

  // Sync prop choices
  createEffect (() => {
    if (local.prop) {
      const p = local.prop;
      // trigger read for reactivity
      p.name; p.value; p.metadata;
      (async () => {
        set_choices_ (await p.choices());
      }) ();
    }
  });

  // Update data-tip
  createEffect (() => {
    if (!root_el) return;
    root_el.setAttribute ('data-tip', data_tip());
  });

  const mchoices = createMemo (() => {
    const result: any[] = [];
    const choices = local.choices?.length ? local.choices : choices_();
    for (let i = 0; i < choices.length; i++) {
      result.push (Object.assign ({}, choices[i]));
    }
    return result;
  });

  function current()
  {
    const mc = mchoices();
    for (let i = 0; i < mc.length; i++)
      if (mc[i].ident == value_())
	return mc[i];
    return {};
  }

  function data_tip(): string
  {
    const choice = current();
    let tip = "**CLICK** Select Choice";
    const plabel = local.label || local.prop?.label_;
    if (!plabel || !choice.label)
      return tip;
    let val = "**" + plabel + "** ";
    val += choice.label;
    return val + " " + tip;
  }

  function current_span(): string
  {
    const choice = current();
    if (local.small)
      return choice.icon ? choice.icon : choice.label || "";
    else
      return choice.label ? choice.label : choice.icon || "";
  }

  function activate (uri: string)
  {
    if (local.disabled) {
      cmenu_el?.close();
      set_need_cmenu (false);
      return;
    }
    if (cmenu_el) {
      // close popup to remove focus guards
      cmenu_el.close();
      set_need_cmenu (false);
    }
    set_value_ (uri);
    if (root_el) {
      (root_el as any).value = uri;
      root_el.dispatchEvent (new Event ('valuechange', { composed: true }));
    }
  }

  function popup_menu (event: Event)
  {
    if (local.disabled)
      return;
    // Force re-render if the menu element is missing or detached; Solid refs
    // are not cleared on unmount, so a stale ref must not be reused.
    if (!cmenu_el || !cmenu_el.isConnected) {
      cmenu_el = undefined;
      set_need_cmenu (true);
    }
    // Wait for DOM update before accessing cmenu
    setTimeout (() => {
      if (local.disabled || !cmenu_el || !cmenu_el.isConnected || cmenu_el.open)
        return;
      pophere_el?.focus();
      cmenu_el.popup (event, { origin: pophere_el, focus_uri: value_() });
    }, 0);
  }

  function keydown (event: KeyboardEvent)
  {
    if (local.disabled || cmenu_el?.open)
      return;
    // allow selection changes with UP/DOWN while menu is closed
    if (event.keyCode == Util.KeyCode.DOWN || event.keyCode == Util.KeyCode.UP)
      {
	Util.prevent_event (event);
	const mc = mchoices();
	const choice = current();
	if (choice.ident)
	  for (let i = 0; i < mc.length; i++)
	    if (mc[i].ident == choice.ident) {
	      const index = i + (event.keyCode == Util.KeyCode.DOWN ? +1 : -1);
	      if (index >= 0 && index < mc.length)
		activate (mc[index].ident);
	      break;
	    }
      }
    else if (event.keyCode == Util.KeyCode.ENTER)
      popup_menu (event);
  }

  return (
    <div class={merged_class()} aria-disabled={local.disabled || undefined} ref={el => {
      root_el = el;
      local.ref?.(el);
    }} {...others}
      onClick={popup_menu}
      onMouseDown={popup_menu}
      onKeyDown={keydown}
    >
      <div class="b-choice-current hflex" ref={pophere_el} tabindex="0">
        <span class="-current">{current_span()}</span>
        <span class="-arrow"> ⬍ </span>
      </div>
      {need_cmenu() && (
        <ContextMenu class="b-choiceinput-contextmenu" ref={h => cmenu_el = h}
          onactivate={e => activate (get_uri (e.detail))}
          onclose={e => { set_need_cmenu (false); cmenu_el = undefined; }}>
          <MenuTitle style={!local.title ? 'display:none' : ''}>
            {local.title}
          </MenuTitle>
          <For each={mchoices()}>
            {(c: any) => (
              <button class="m-0 grid cursor-pointer select-none auto-rows-auto items-stretch border border-solid text-left"
                uri={c.ident} ic={c.icon}>
                <span class={`b-choice-label ${c.labelclass ?? ''}`}>{c.label}</span>
                <span class={`b-choice-line1 ${c.line1class ?? ''}`}>{c.blurb}</span>
                <span class={`b-choice-line2 ${c.line2class ?? ''}`}>{c.line2}</span>
                <span class={`b-choice-line3 ${c.line3class ?? ''}`}>{c.notice}</span>
                <span class={`b-choice-line4 ${c.line4class ?? ''}`}>{c.warning}</span>
              </button>
            )}
          </For>
        </ContextMenu>
      )}
    </div>
  );
}
