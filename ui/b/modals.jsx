// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class B-Modals
 * A separate layer of the B-Shell used for creating modal dialogs.
 */

import { createSignal, For, Show, onMount } from 'solid-js';
import * as Signal from "../signal.js";
import * as Util from "../util.js";
import { PushButton } from './basics';
import { Icon } from './icon';

// == STYLE ==
Extra_css`
dialog.b-modals {
  margin: auto;
  footer {
    justify-content: space-between;
    button, .asbutton {
      white-space: nowrap;
      --hpadding: 0.75em;
      padding-left: var(--hpadding); padding-right: var(--hpadding);
    }
    &.-manybuttons {
      width: 100%;
      button, .asbutton {
	width: 100%;
      }
    }
  }
}
`;

const dialog_emblems = {
  PIANO:	{ mi: "piano",			style: "font-size: 300%; padding-right: 1rem; float: left; color: #ffbbbb" },
  QUESTION:	{ fa: "question-circle",	style: "font-size: 300%; padding-right: 1rem; float: left; color: #538cc1" },
  ERROR:	{ fa: "times-circle",		style: "font-size: 300%; padding-right: 1rem; float: left; color: #cc2f2a" },
  KEYBOARD:	{ mi: "keyboard",		style: "font-size: 300%; padding-right: 1rem; float: left; color: #538cc1" },
};

// == DynamicButton with .canfocus ==
function DynamicButton (props)
{
  return props.canfocus ? (
    <button {...props}>{props.children}</button>
  ) : (
    <PushButton {...props}>{props.children}</PushButton>
  );
}

// == DialogComponent ==
const DialogComponent = (props) => {
  const { dialog: d } = props;
  let divHandlerElement;
  let dialogRef; // The ref for the <dialog> element itself.

  // This hook now attaches imperative methods to the dialog object on mount.
  onMount (() => {
    // Expose imperative API on the dialog data object
    d.openModal = () => dialogRef?.showModal();
    d.closeModal = () => dialogRef?.close();

    if (d.div_handler && divHandlerElement) {
      d.div_handler (divHandlerElement, dialogRef);
    }
  });

  return (
    <dialog class="b-modals"
	    id={`MDialog_${d.dialogid}`}
	    classList={{ [d.class]: !!d.class }}
	    ref={dialogRef}
	    onClose={() => d.handleClose()}
	    exclusive={true} bwidth="9em" style="z-index: 93">
      <header>
        {d.header}
      </header>
      <main>
        <div class="hflex items-center justify-start">
          <Icon {...d.icon}/>
          <div style="flex-grow whitespace-pre-line">{d.body}</div>
          <div style="flex-grow whitespace-pre-line" innerHTML={d.vhtml}></div>
        </div>
        <Show when={d.proplist}>
          <b-fed-object class="-modal-fed" value={d.proplist}></b-fed-object>
        </Show>
        <Show when={d.div_handler}>
          <div class="-div-handler" ref={divHandlerElement}></div>
        </Show>
        {props.children}
      </main>
      <footer>
        <div class="hflex" classList={{ [d.footerclass]: !!d.footerclass }}>
          <For each={d.buttons}>{
            (b, i) => {
              return (
                <DynamicButton
		  canfocus={b.canfocus}
                  autofocus={b.autofocus}
                  onClick={(ev) => d.click (i())}
                  disabled={b.disabled}>
                  {b.label}
                </DynamicButton>
              );
            }
          }</For>
        </div>
      </footer>
    </dialog>
  );
};

// == BModals ==
class BModals extends Object {
  constructor (getset_dialogs)
  {
    super();
    this.id_counter_ = 1;
    this.dialogs_ = getset_dialogs[0];
    this.set_dialogs_ = getset_dialogs[1];
  }
  async async_modal_dialog (dialog_setup)
  {
    let resolve;
    const promise = new Promise (r => resolve = r);
    const [get_visible, set_visible] = Signal.createSignal (false);
    // TODO: animate dialog show/hide

    const m = {
      dialogid: this.id_counter_++,
      class: dialog_setup.class,
      proplist: dialog_setup.proplist || [],
      get visible() { return get_visible(); },

      // div_handler provides a minimal hook for live updates in a dialog
      div_handler: dialog_setup.div_handler,
      // TODO: improve/replace div_handler logic

      // To be populated by the component onMount
      openModal: null,
      closeModal: null,

      // This is the <dialog> element's onClose handler
      handleClose() {
        if (!this.visible) return; // Prevent re-entry
        set_visible (false);
        if (dialog_setup.destroy)
          dialog_setup.destroy();
        resolve (this.result);
        const delay = (globalThis.CONFIG && globalThis.CONFIG.transitiondelay) || 200;
        setTimeout (() => this.set_dialogs_ (dialogs => dialogs.filter (d => d !== this)), delay);
	// TODO: auto-remove dialogs after closing
      },

      result: -1,
      click (r)
      {
        this.result = r;
        this.closeModal?.(); // Imperatively close the dialog
      },
      header: dialog_setup.title,
      body: dialog_setup.text,
      vhtml: dialog_setup.html,
      icon: dialog_emblems[dialog_setup.emblem] || {},
      footerclass: '',
      buttons: []
    };

    const is_string = s => typeof s === 'string' || s instanceof String;
    const check_bool = (v, dflt) => v !== undefined ? !!v : dflt;
    const buttons = dialog_setup.buttons || [];
    for (let i = 0; i < buttons.length; i++) {
      const label = is_string (buttons[i]) ? buttons[i] : buttons[i].label;
      const disabled = check_bool (buttons[i].disabled, false);
      const canfocus = check_bool (buttons[i].canfocus, true);
      const autofocus = check_bool (buttons[i].autofocus, false);
      const button = { label, disabled, autofocus, canfocus };
      m.buttons.push (button);
    }
    if (m.buttons.length >= 2)
      m.footerclass = '-manybuttons';

    this.set_dialogs_ (dialogs => [...dialogs, m]);
    // delay openModal until after onMount
    setTimeout (() => m.openModal?.(), 0);
    return promise;
  }
};

// == ModalDialogs COMPONENT ==
export const ModalDialogs = (props) => {
  const getset_dialogs = createSignal ([]);
  const t = new BModals (getset_dialogs);
  props.ref?. (t);
  return (
    <For each={getset_dialogs[0]()}>{
      d => <DialogComponent dialog={d} />
    }</For>
  );
};
