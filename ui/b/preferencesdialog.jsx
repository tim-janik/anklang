// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class B-PreferencesDialog
 * SolidJS component that displays a modal dialog to edit preferences.
 *
 * ### Props:
 * *shown*
 * : Boolean controlling dialog visibility.
 * *onClose*
 * : Callback invoked when the Close button is activated.
 */

import { createSignal, createEffect, onMount, onCleanup } from 'solid-js';
import * as Util from "../util.js";
import * as Ase from '../../ase/gen/api-jsonipc.g.ts';
import * as Dom from "../dom.js";

// == STYLE ==
Extra_css`
dialog.b-preferencesdialog {
  margin: auto;
}
`;

// == Component ==
export function PreferencesDialog (props)
{
  let dialogRef;
  let fedobjectRef;
  const [proplist, set_proplist] = createSignal ([]);
  let cancelled = false; // guard against showModal() after unmount

  onMount (() => {
    // Set augment function on b-objecteditor (function props don't flow through JSX to web components)
    if (fedobjectRef)
      fedobjectRef.augment = augment_prop;
  });

  // Watch shown prop to open/close dialog
  createEffect (() => {
    const shown = props.shown;
    if (shown && !dialogRef?.open) {
      document.startViewTransition (async () => {
        Dom.show_modal (dialogRef, () => props.onClose?.());
        await fetch_preferences ();
      });
    }
    if (!shown && dialogRef?.open) {
      dialogRef.close ();
    }
  });

  onCleanup (() => {
    cancelled = true;
    dialogRef?.close ();
    props.onClose?. ();
  });

  const handleClose = () => {
    props.onClose?. ();
  };

  const close_button_click = (event) => {
    if (event.shiftKey && event.ctrlKey && (event.altKey || event.metaKey)) {
      Util.prevent_event (event);
      fetch_preferences ();
      return;
    }
    if (!props.shown) return;
    Util.prevent_event (event);
    document.startViewTransition (() => {
      props.onClose?. ();
    });
  };

  async function fetch_preferences ()
  {
    const list = await access_preferences ();
    if (cancelled) return;
    set_proplist (list);
    debug ("fetch_preferences:", list);
  }

  return (
    <dialog
      class="b-preferencesdialog floating-dialog [&:not([open])]:hidden"
      ref={dialogRef}
      onClose={handleClose}
      exclusive={true}
      bwidth="9em"
      style="z-index: 93">
      <div class="dialog-header">Anklang Preferences</div>
      <b-objecteditor
        class="b-preferencesdialog-fed"
        ref={fedobjectRef}
        value={proplist ()}
      ></b-objecteditor>
      <div class="dialog-footer">
        <button class="button-xl" autofocus onClick={close_button_click}>Close</button>
      </div>
    </dialog>
  );
}

// == Helper functions (unchanged from Lit version) ==

async function access_preferences ()
{
  return Promise.all ((await Ase.server.list_preferences ()).sort ().map (id => Ase.server.access_preference (id)));
}

function augment_prop (xprop)
{
  if (xprop.has_choices_) {
    for (let i = 0; i < xprop.value_.choices.length; i++) {
      const c = xprop.value_.choices [i];
      if (xprop.ident_ == 'driver.pcm.devid')
        augment_choice_entry (c, 'pcm');
      else if (xprop.ident_.match (/midi/i))
        augment_choice_entry (c, 'midi');
    }
  }
}

function augment_choice_entry (c, devicetype)
{
  const is_midi = devicetype == 'midi';
  const is_usb = c.label.match (/^USB /) || c.blurb.match (/ at usb-/);
  if (is_usb)
    c.blurb = c.blurb.replace (/ at usb-[0-9].*/, ' (USB)');
  if (is_midi)
    c.blurb = c.blurb.replace (/\n/, ', ');
  const standard_icons = ['pcm', 'midi'];
  const icon_hints = c.icon.split (/\s*,\s*/);
  if (c.icon && icon_hints.length == 0 && !standard_icons.includes (c.icon.replace (/\W/, '')))
    return;
  const is_pcm = devicetype == 'pcm';
  if (c.ident.startsWith ("null"))
    c.icon = "md-not_interested"; // "fa-deaf";
  else if (c.ident.startsWith ("auto"))
    c.icon = "fa-cog";
  else if (is_midi) {
    if (c.label.match (/\bMIDI\W*$/))
      c.icon = 'fa-music';
    else if (is_usb)
      c.icon = 'uc-🎘';
    else
      c.icon = 'fa-music';
  } else if (is_pcm) {
    const is_rec  = c.blurb.match (/\d\*captur/i);
    const is_play = c.blurb.match (/\d\*play/i);
    if (c.ident.startsWith ("jack="))
      c.icon = "md-graphic_eq";
    else if (c.ident.startsWith ("alsa=pulse"))
      c.icon = "md-speaker_group";
    else if (c.label.startsWith ("HDMI"))
      c.icon = "fa-tv";
    else if (icon_hints.includes ("headset"))
      c.icon = "md-headset_mic";
    else if (icon_hints.includes ("recorder"))
      c.icon = "uc-🎙";
    else if (is_usb)
      c.icon = "fa-usb";
    else if (c.blurb.match (/\bModem\b/))
      c.icon = "uc-☎ ";
    else if (is_rec && !is_play)
      c.icon = "md-mic";
    else if (is_play && !is_rec)
      c.icon = "fa-volume_up";
    else
      c.icon = "uc-💻";
  } else
    c.icon = "md-not_interested";
}
