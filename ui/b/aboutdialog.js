// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import { LitComponent, html, render, noChange, JsExtract, docs, ref } from '../little.js';
import * as Util from "../util.js";

/** @class BAboutDialog
 * @description
 * The <b-aboutdialog> element is a modal [b-dialog] that displays version information about Anklang.
 * ### Events:
 * *close*
 * : A *close* event is emitted once the "Close" button activated.
 */

// == STYLE ==
JsExtract.scss`
b-aboutdialog {
  c-grid {
    max-width: 100%;
    & > * { // avoid visible overflow for worst-case resizing
      overflow-wrap: break-word;
      min-width: 0; }
  }
  .b-aboutdialog-label {
    grid-column: 1;
    text-align: right; vertical-align: top;
    @include b-font-weight-bold();
    padding-right: .5em; min-width: 15em; }
  .b-aboutdialog-field {
    grid-column: 2;
    overflow-wrap: break-word;
    display: inline-block; white-space: pre-wrap; }
}`;

// == HTML ==
const HTML = (t, d) => html`
<dialog class="b-dialog" ${ref (h => t.dialog = h)} @cancel=${t.close_dialog} @pointerdown=${t.backdrop_click}>
  <div class="b-dialog-header">
    About ANKLANG
  </div>
  <c-grid class="b-dialog-body">
    ${INFOS_HTML (t, d)}
  </c-grid>
  <div class="b-dialog-footer">
    <button autofocus @click=${t.close_dialog} > Close </button>
  </div>
</dialog>`;
const INFOS_HTML = (t, d) =>
  t.info_pairs.map (p => html`
    <span class="b-aboutdialog-label"> ${p[0]} </span>
    <span class="b-aboutdialog-field"> ${p[1]} </span>
  `);

// == SCRIPT ==
export class BAboutDialog extends LitComponent {
  createRenderRoot() { return this; }
  render()
  {
    const d = {};
    return HTML (this, d);
  }
  static properties = {
    shown:	{ type: Boolean },
  };
  constructor()
  {
    super();
    this.shown = false;
    this.info_pairs = [];
    this.dialog = null;
  }
  updated (changed_props)
  {
    if (changed_props.has ('shown') && this.shown) {
      const load_infos = async () => {
	this.info_pairs = await about_pairs();
	this.requestUpdate(); // showModal with info_pairs
      };
      load_infos();
    }
    if (this.shown && !this.dialog.open && this.info_pairs.length > 0) {
      this.dialog.showModal();
    }
    if (!this.shown && this.dialog.open)
      this.dialog.close();
  }
  close_dialog (event = null)
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
  backdrop_click (event)
  {
    if (event.target === event.currentTarget &&                           // target must be DIALOG or backdrop
	(event.offsetX < 0 || event.offsetX > event.target.offsetWidth || // click must be outside
	 event.offsetY < 0 || event.offsetY > event.target.offsetHeight)) // which is backdrop
      this.close_dialog (event);
  }
}
customElements.define ('b-aboutdialog', BAboutDialog);

async function about_pairs() {
  const user_agent = navigator.userAgent.replace (/([)0-9]) ([A-Z])/gi, '$1\n$2');
  let array = [
    [ 'Anklang:',		CONFIG.version + ' (' + CONFIG.revdate.split (' ')[0] + ')' ],
    [ 'SoundEngine:',		await Ase.server.get_build_id() ],
    [ 'CLAP:',			await Ase.server.get_clap_version() ],
    [ 'FLAC:',		        await Ase.server.get_flac_version() ],
    [ 'Opus:',		        await Ase.server.get_opus_version() ],
    [ 'Lit:',			CONFIG.lit_version ],
    [ 'Vuejs:',			Vue.version ],
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

