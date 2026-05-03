// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import { LitComponent, html, render, noChange, JsExtract, docs, ref } from '../little.js';
import * as Util from "../util.js";
import * as Dom from "../dom.js";

// == HTML ==
const HTML = (t, d) => html`
<dialog class="floating-dialog [&:not([open])]:hidden" ${ref (h => t.dialog = h)}>
  <div class="dialog-header">
    About ANKLANG
  </div>
  <div class="grid max-w-full">
    ${INFOS_HTML (t, d)}
  </div>
  <div class="dialog-footer">
    <button class="button-xl" autofocus @click=${e => t.emit_close()} > Close </button>
  </div>
</dialog>`;
const INFOS_HTML = (t, d) =>
  t.info_pairs.map (p => html`
    <span class="col-start-1 min-w-[15em] pr-2 text-right align-top font-bold"> ${p[0]} </span>
    <span class="col-start-2 whitespace-pre-wrap break-words"> ${p[1]} </span>
  `);

// == SCRIPT ==

/** @class BAboutDialog
 * @description
 * The <b-aboutdialog> element is a modal [dialog] that displays version information about Anklang.
 * ### Events:
 * *close*
 * : A *close* event is emitted once the "Close" button activated.
 */
export class BAboutDialog extends LitComponent {
  createRenderRoot() { return this; }
  render() { return HTML (this, {}); }
  static properties = {
    shown:	{ type: Boolean },
  };
  constructor()
  {
    super();
    this.shown = false;
    this.info_pairs = [];
    this.dialog = null;
    this.completed_promise = null;
  }
  updated (changed_props)
  {
    let info_promise;
    if (changed_props.has ('shown') && this.shown) {
      const load_infos = async () => {
	this.info_pairs = await about_pairs();
	this.requestUpdate(); // showModal with info_pairs
      };
      info_promise = load_infos();
    }
    if (this.shown && !this.dialog.open && this.info_pairs.length > 0) {
      Dom.show_modal (this.dialog, () => this.emit_close());
      const old_completed_promise = this.completed_promise;
      this.completed_promise = (async () => {
	await Promise.all ([old_completed_promise, info_promise, this.updateComplete]);
	this.completed_promise = null;
      }) ();
    }
    if (!this.shown && this.dialog.open) {
      this.close_dialog();
      this.requestUpdate();
    }
  }
  close_dialog (event = null)
  {
    Util.prevent_event (event);
    this.dialog.close();
    const old_completed_promise = this.completed_promise;
    this.completed_promise = (async () => {
      await Promise.all ([old_completed_promise, this.updateComplete]);
      this.completed_promise = null;
    }) ();
    return this.completed_promise;
  }
  emit_close (event = null)
  {
    Util.prevent_event (event);
    this.dispatchEvent (new CustomEvent ('close', { detail: {} }));
  }
  disconnectedCallback()
  {
    if (this.dialog.open) // ensure focus is restored
      this.dialog.close();
    super.disconnectedCallback();
  }
}
customElements.define ('b-aboutdialog', BAboutDialog);

async function about_pairs() {
  const user_agent = navigator.userAgent.replace (/([)0-9]) ([A-Z])/gi, '$1\n$2');
  let array = [
    [ 'Anklang:',		CONFIG.version + ' (' + CONFIG.revdate.split (' ')[0] + ')' ],
    [ 'SoundEngine:',		await Ase.server.get_build_id() ],
    [ 'FLAC:',		        await Ase.server.get_flac_version() ],
    [ 'Opus:',		        await Ase.server.get_opus_version() ],
    [ 'Sndfile:',	        await Ase.server.get_sndfile_version() ],
    [ 'Lit:',			CONFIG.lit_version ],
    [ 'User Agent:',		user_agent ],
  ];
  const Electron = window['Electron'];
  if (Electron)
    {
      const operating_system = Electron.platform + ' ' + Electron.arch + ' (' + Electron.os_release + ')';
      const parray = [
	[ 'Electron:',          Electron.versions.electron ],
	[ 'Chrome:',            Electron.versions.chrome ],
	[ 'Node.js:',           Electron.versions.node ],
	[ 'Libuv:',             Electron.versions.uv ],
	[ 'V8:',                Electron.versions.v8 ],
	[ 'OS:',		operating_system ],
      ];
      array = Array.prototype.concat (array, parray);
    }
  return array;
}

