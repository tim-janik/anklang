// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import { LitComponent, html, JsExtract, docs } from '../little.js';
import * as Util from "../util.js";

/** @class BPlayControls
 * @description
 * The <b-playcontrols> element is a container holding the play and seek controls for a Ase.song.
 */

// == STYLE ==
Extra_css`
b-playcontrols {
  button, .asbutton	{ padding: 5px; text-align: center; }
}
`;

// == HTML ==
const HTML = (t, D) => html`
<b-buttonbar class="b-playcontrols" >
  <div class="asbutton button-down" @click="${D ('-todo-Last')}" disabled >      <b-icon fw lg ic="fa-fast_backward"></b-icon></div>
  <div class="asbutton button-down" @click="${D ('-todo-Backwards')}" disabled > <b-icon fw lg ic="fa-backward"     ></b-icon></div>
  <div class="asbutton button-down" @click="${D ('stop_playback')}" data-hotkey="S"
                data-tip="**CLICK** Stop playback" >        <b-icon fw lg ic="fa-stop"         ></b-icon></div>
  <div class="asbutton button-down" @click="${t.toggle_play}" data-hotkey="RawSpace"
                data-tip="**CLICK** Start/pause playback" >  <b-icon fw lg ic="fa-play" hi="ho" ></b-icon></div>
  <div class="asbutton button-down" @click="${D ('-todo-Record')}" disabled >    <b-icon fw lg ic="fa-circle"       ></b-icon></div>
  <div class="asbutton button-down" @click="${D ('-todo-Forwards')}" disabled >  <b-icon fw lg ic="fa-forward"      ></b-icon></div>
  <div class="asbutton button-down" @click="${D ('-todo-Next')}" disabled >      <b-icon fw lg ic="fa-fast_forward" ></b-icon></div>
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
