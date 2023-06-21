// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { LitComponent, html, JsExtract, docs } from '../little.js';
import * as Util from "../util.js";

/** # B-PLAYCONTROLS
 * A container holding the play and seek controls for a Ase.song.
 */

// == STYLE ==
const STYLE_URL = await JsExtract.css_url (import.meta);
JsExtract.scss`
:host {
  button, push-button	{ padding: 5px; }
}
`;

// == HTML ==
const HTML = (t, D) => html`
<b-buttonbar class="b-playcontrols" >
  <push-button @click="${D ('-todo-Last')}" disabled >      <b-icon fw lg ic="fa-fast-backward" /></push-button>
  <push-button @click="${D ('-todo-Backwards')}" disabled > <b-icon fw lg ic="fa-backward"      /></push-button>
  <push-button @click="${D ('stop_playback')}" data-hotkey="S"
                data-tip="**CLICK** Stop playback" >        <b-icon fw lg ic="fa-stop"          /></push-button>
  <push-button @click="${t.toggle_play}" data-hotkey="RawSpace"
                data-tip="**CLICK** Start/stop playback" >  <b-icon fw lg ic="fa-play" hi="ho"  /></push-button>
  <push-button @click="${D ('-todo-Record')}" disabled >    <b-icon fw lg ic="fa-circle"        /></push-button>
  <push-button @click="${D ('-todo-Forwards')}" disabled >  <b-icon fw lg ic="fa-forward"       /></push-button>
  <push-button @click="${D ('-todo-Next')}" disabled >      <b-icon fw lg ic="fa-fast-forward"  /></push-button>
</b-buttonbar>
`;

// == SCRIPT ==
class BPlayControls extends LitComponent {
  render()
  {
    const dispatcher = (method) =>
      (ev) => this.dispatch (method, ev);
    return HTML (this, dispatcher);
  }
  createRenderRoot()
  {
    Util.add_style_sheet (this, STYLE_URL);
    return this;
  }
  async dispatch (method, ev)
  {
    const project = Data.project;
    let func = project[method], message;
    if (func !== undefined) {
      let result = await func.call (project);
      if (result == undefined)
	result = 'ok';
      message = method + ': ' + result;
    }
    else
      message = method + ': unimplemented';
    App.status (message);
  }
  async toggle_play()
  {
    const project = Data.project;
    const playing = await project.is_playing();
    this.dispatch (playing ? 'stop_playback' : 'start_playback');
  }
}
customElements.define ('b-playcontrols', BPlayControls);
