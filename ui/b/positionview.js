// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import { LitComponent, html, JsExtract, live, docs, ref } from '../little.js';
import * as Util from '../util.js';
import * as Ase from '../aseapi.js';

/** # B-POSITIONVIEW - Display of the project transport position pointer and related information
 * ### Props:
 * - **project** - The object providing playback API.
 */

// <STYLE/>
JsExtract.scss`
b-positionview { @include h-flex(); }
b-positionview {
  $b-positionview-fg: $b-lcdscreen-fg;
  $b-positionview-bg: $b-lcdscreen-bg;
  $b-positionview-b0: zmod($b-positionview-bg,Jz-=1);
  $b-positionview-b1: zmod($b-positionview-bg,Jz+=1);
  margin: 0; padding: 5px 1em;
  letter-spacing: 0.05em;
  border-radius: $b-button-radius; align-items: baseline;
  border-top:    1px solid zmod($b-positionview-bg, Jz-=3%);
  border-left:   1px solid zmod($b-positionview-bg, Jz-=3%);
  border-right:  1px solid zmod($b-positionview-bg, Jz+=3%);
  border-bottom: 1px solid zmod($b-positionview-bg, Jz+=3%);
  background-color: $b-positionview-bg;
  background: linear-gradient(to bottom, $b-positionview-b0 0%, $b-positionview-b1 100%);
  color: $b-positionview-fg;
  .b-positionview-counter,
  .b-positionview-timer	{ font-size: 110%; margin-right: .5em; }
  .b-positionview-bpm,
  .b-positionview-sig		{ font-size: 90%; margin: 0 0.5em; }
  .b-positionview-sig, .b-positionview-counter, .b-positionview-bpm, .b-positionview-timer {
    position: relative;
    text-align: right;
    height: 1.2em;
  }
  .b-positionview-bpm {
    display: block;
    text-align: center;
    width: 3em; }
  .b-positionview-counter { width: 7em; } /* fixed size reduces layouting during updates */
  .b-positionview-timer	  { width: 7em; } /* fixed size reduces layouting during updates */
}`;

// <HTML/>
const HTML = (t, d) =>  html`
  <b-editable class="b-positionview-sig" @change=${event => t.apply_sig (event.detail.value)} selectall
    >${t.project_.numerator + '/' + t.project_.denominator}</b-editable>
  <span class="b-positionview-counter" ${ref (h => t.counter = h)} ></span>
  <b-editable class="b-positionview-bpm" @change=${event => Data.project.set_value ('bpm', 0 | event.detail.value)} selectall
    >${t.project_.bpm}</b-editable>
  <span class="b-positionview-timer" ${ref (h => t.timer = h)}></span>
`;

// <SCRIPT/>
class BPositionView extends LitComponent {
  createRenderRoot() { return this; }
  render()
  {
    return HTML (this, {});
  }
  constructor()
  {
    super();
    this.fps = 0;
    this.counter = null;
    this.timer = null;
    this.project_ = {};
  }
  updated()
  {
    if (!this.counter_text && this.counter) {
      this.counter_text = document.createTextNode ("");
      this.counter.appendChild (this.counter_text);
      this.timer_text = document.createTextNode ("");
      this.timer.appendChild (this.timer_text);
    }
  }
  async connectedCallback()
  {
    super.connectedCallback();
    let project = Data.project;
    if (!project.bpm) {
      // FIXME: work around until the TimeSignature branch is merged
      project = Object.create (project);
      for (const key of ['bpm', 'numerator', 'denominator'])
	project[key] = () => project.get_value (key);
    }
    const weakthis = new WeakRef (this); // avoid strong wtrack->this refs for automatic cleanup
    this.project_ = Util.wrap_ase_object (project, { bpm: 0, numerator: 0, denominator: 1 }, () => weakthis.deref()?.requestUpdate());
    this.telemetry = Object.freeze (await Data.project.telemetry());
    if (this.telemetry) {
      const telefields = [ 'current_bar', 'current_beat', 'current_sixteenth', 'current_minutes', 'current_seconds' ];
      const subscribefields = this.telemetry.filter (field => telefields.includes (field.name));
      this.teleobj = Util.telemetry_subscribe (this.recv_telemetry.bind (this), subscribefields);
    }
  }
  disconnectedCallback()
  {
    super.disconnectedCallback();
    this.teleobj && Util.telemetry_unsubscribe (this.teleobj);
    this.teleobj = null;
    this.telemetry = null;
    this.counter_text = null;
    this.timer_text = null;
  }
  apply_sig (v)
  {
    const parts = ("" + v).split ('/');
    if (parts.length == 2) {
      const n = Number (parts[0]), d = Number (parts[1]);
      if (n > 0 && d > 0) {
	Data.project.set_value ('numerator', n);
	Data.project.set_value ('denominator', d);
      }
    }
  }
  recv_telemetry (teleobj, arrays)
  {
    if (!this.timer_text) return;
    const ds = "\u2007"; // FIGURE SPACE - "Tabular width", the width of digits
    const s3 = n => (n >= 100 ? "" : n >= 10 ? ds : ds + ds) + n;
    const s2 = n => (n >= 10 ? "" : ds) + n;
    const z2 = n => (n >= 10 ? "" : "0") + n;
    const ff = (n, d = 2) => Number.parseFloat (n).toFixed (d);
    // const tick = arrays[teleobj.current_tick.type][teleobj.current_tick.index];
    // const bpm = arrays[teleobj.current_bpm.type][teleobj.current_bpm.index];
    const bar = arrays[teleobj.current_bar.type][teleobj.current_bar.index];
    const beat = arrays[teleobj.current_beat.type][teleobj.current_beat.index];
    const sixteenth = arrays[teleobj.current_sixteenth.type][teleobj.current_sixteenth.index];
    const minutes = arrays[teleobj.current_minutes.type][teleobj.current_minutes.index];
    const seconds = arrays[teleobj.current_seconds.type][teleobj.current_seconds.index];
    const barpos = s3 (1 + bar) + "." + s2 (1 + beat) + "." + (1 + sixteenth).toFixed (2);
    const timepos = z2 (minutes) + ":" + z2 (ff (seconds, 3));
    if (this.counter_text.nodeValue != barpos)
      this.counter_text.nodeValue = barpos;
    if (this.timer_text.nodeValue != timepos)
      this.timer_text.nodeValue = timepos;
  }
}
customElements.define ('b-positionview', BPositionView);
