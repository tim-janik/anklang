// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import { LitComponent, html, css, ref, repeat, JsExtract } from '../little.js';
import * as Util from "../util.js";
import * as Ase from '../../ase/gen/api-jsonipc.g.ts';
import * as Dom from "../dom.js";

// == STYLE ==
Extra_css`
dialog.b-preferencesdialog {
  margin: auto;
}
`;

// == HTML ==
const HTML = (t) => html`
  <dialog class="floating-dialog [&:not([open])]:hidden" ${ref (h => t.dialog = h)} @close=${t.close_dialog}>
    <div class="dialog-header">Anklang Preferences</div>
    <b-objecteditor class="b-preferencesdialog-fed" ${ref (h => t.fedobject = h)} .value=${t.proplist} .augment=${t.augment}></b-objecteditor>
    <div class="dialog-footer">
      <button class="button-xl" autofocus @click=${e => t.close_button_click (e)}>Close</button>
    </div>
  </dialog>
`; // FIXME: use close callback and use <dialog />

/** # B-PREFERENCESDIALOG
 * A modal [dialog] to edit preferences.
 * ## Events:
 * *close*
 * : A *close* event is emitted once the "Close" button activated.
 */

// == SCRIPT ==
class BPreferencesDialog extends LitComponent {
  static properties = {
    shown: { type: Boolean, reflect: true },
    all_prefs: { type: Boolean, state: true },
    proprefresh: { type: Function, state: true },
  };

  constructor()
  {
    super();
    this.shown = false;
    this.all_prefs = false;
    this.proprefresh = null;
    this.proplist = [];
    this.fedobject = null;
    this.dialog = null;
  }

  render()
  {
    return HTML (this);
  }

  connectedCallback()
  {
    super.connectedCallback();
    this.all_prefs = true;
  }

  updated (changedProps)
  {
    if (changedProps.has ('shown') && this.shown && this.proprefresh) {
      this.proprefresh();
    }
    if (this.shown && !this.dialog.open) {
      document.startViewTransition (async () => {
	Dom.show_modal (this.dialog);
	await Promise.all ([this.updateComplete, this.fetch_preferences()]);
      });
    }
    if (!this.shown && this.dialog.open)
      this.dialog.close();
  }

  async fetch_preferences()
  {
    this.proplist = await this.access_preferences();
    debug ("fetch_preferences:", this.proplist);
    if (this.proprefresh) {
      this.proprefresh();
    }
    this.requestUpdate();
  }

  augment (p)
  {
    augment_property (p);
  }

  close_button_click (event)
  {
    if (event.shiftKey && event.ctrlKey && (event.altKey || event.metaKey)) {
      Util.prevent_event (event);
      this.all_prefs = true;
      this.fetch_preferences();
      return;
    }
    this.close_dialog (event);
  }
  close_dialog (event)
  {
    if (!this.shown) return;
    Util.prevent_event (event);
    document.startViewTransition (() => {
      this.dispatchEvent (new CustomEvent ('close', { detail: {} }));
    });
  }

  async access_preferences()
  {
    if (this.all_prefs)
      return Promise.all ((await Ase.server.list_preferences()).sort().map (id => Ase.server.access_preference (id)));
    const preferences = [ [ _("Synthesis Settings"),
			    "driver.pcm.devid", "driver.pcm.synth_latency" ],
			  [ _("MIDI Settings"),
			    "driver.midi1.devid", "driver.midi2.devid", "driver.midi3.devid", "driver.midi4.devid" ],
    ];
    let props = []; // [ [group,promise]... ]
    for (const [group, ...idents] of preferences)
      for (const ident of idents)
        props.push ([Ase.server.access_preference (ident), group]); // [ [property_promise,group]... ]
    const result = [];
    for (let [property, group] of props) {
      property = Object.create (await property); // new Object with prototype
      property.group = () => group;
      result.push (property);
    }
    return result;
  }
}
customElements.define ('b-preferencesdialog', BPreferencesDialog);



async function augment_property (xprop)
{
  if (xprop.has_choices_) {
    for (let i = 0; i < xprop.value_.choices.length; i++) {
      const c = xprop.value_.choices[i];
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
