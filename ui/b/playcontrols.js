// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import { LitComponent, html, JsExtract, docs } from '../little.js';
import * as Util from "../util.js";

/** # B-PLAYCONTROLS
 * A container holding the play and seek controls for a Ase.song.
 */

// == STYLE ==
JsExtract.scss`
b-playcontrols {
  button, push-button	{ padding: 5px; }
}
`;

// == HTML ==
const HTML = (t, D) => html`
<b-buttonbar class="b-playcontrols" >
  <push-button @click="${D ('-todo-Last')}" disabled >      <b-icon fw lg ic="fa-fast-backward"></b-icon></push-button>
  <push-button @click="${D ('-todo-Backwards')}" disabled > <b-icon fw lg ic="fa-backward"     ></b-icon></push-button>
  <push-button @click="${D ('stop_playback')}" data-hotkey="S"
                data-tip="**CLICK** Stop playback" >        <b-icon fw lg ic="fa-stop"         ></b-icon></push-button>
  <push-button @click="${t.toggle_play}" data-hotkey="RawSpace"
                data-tip="**CLICK** Start/stop playback" >  <b-icon fw lg ic="fa-play" hi="ho" ></b-icon></push-button>
  <push-button @click="${D ('-todo-Record')}" disabled >    <b-icon fw lg ic="fa-circle"       ></b-icon></push-button>
  <push-button @click="${D ('-todo-Forwards')}" disabled >  <b-icon fw lg ic="fa-forward"      ></b-icon></push-button>
  <push-button @click="${D ('-todo-Next')}" disabled >      <b-icon fw lg ic="fa-fast-forward" ></b-icon></push-button>
</b-buttonbar>
`;

// == SCRIPT ==
class BPlayControls extends LitComponent {
  createRenderRoot() { return this; }
  render()
  {
    const dispatcher = (method) =>
      (ev) => this.dispatch (method, ev);
    return HTML (this, dispatcher);
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
