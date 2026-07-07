// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import { LitComponent, html, JsExtract, live, docs, ref } from '../little.js';
import * as Util from '../util.js';

/** @class BPositionView
 * @description
 * The <b-positionview> element displays the project
 * transport position pointer and related information.
 */

// <STYLE/>
Extra_css`
@reference "../tailwind.css";
b-positionview { @apply hflex; }
b-positionview {
  --b-positionview-fg: var(--b-lcdscreen-fg);
  --b-positionview-bg: var(--b-lcdscreen-bg);
  --b-positionview-b0: oklch(from var(--b-positionview-bg) calc(l - 0.01) c h);
  --b-positionview-b1: oklch(from var(--b-positionview-bg) calc(l + 0.01) c h);
  margin: 0; padding: 5px 1em;
  letter-spacing: 0.05em;
  border-radius: var(--b-button-radius); align-items: baseline;
  border-top:    1px solid oklch(from var(--b-positionview-bg) calc(l * 0.97) c h);
  border-left:   1px solid oklch(from var(--b-positionview-bg) calc(l * 0.97) c h);
  border-right:  1px solid oklch(from var(--b-positionview-bg) calc(l * 1.03) c h);
  border-bottom: 1px solid oklch(from var(--b-positionview-bg) calc(l * 1.03) c h);
  background: var(--b-positionview-bg);
  background: linear-gradient(to bottom, var(--b-positionview-b0) 0%, var(--b-positionview-b1) 100%);
  color: var(--b-positionview-fg);
  .b-positionview-counter,
  .b-positionview-timer	{ font-size: 110%; margin-right: .5em; }
  .b-positionview-counter { width: 7em; } /* fixed size reduces layouting during updates */
  .b-positionview-timer	  { width: 7em; } /* fixed size reduces layouting during updates */
}`;

// <HTML/>
const HTML = (t, project) =>  html`
  <b-editable class="w-16 text-center" @change=${event => t.apply_sig (event.detail.value)} selectall
    value=${project.numerator + '/' + project.denominator}></b-editable>
  <span class="b-positionview-counter" ${ref (h => t.counter = h)} ></span>
  <b-editable class="w-16 text-center" @change=${event => project.bpm = 0 | event.detail.value} selectall
    value=${project.bpm}></b-editable>
  <span class="b-positionview-timer" ${ref (h => t.timer = h)}></span>
`;

// <SCRIPT/>
class BPositionView extends LitComponent {
  createRenderRoot() { return this; }
  render()
  {
    return HTML (this, App.project);
  }
  constructor()
  {
    super();
    this.fps = 0;
    this.counter = null;
    this.timer = null;
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
    const telemetry_fields = Object.freeze (await App.project.telemetry());
    if (telemetry_fields) {
      const telefields = [ 'current_bar', 'current_beat', 'current_sixteenth', 'current_minutes', 'current_seconds' ];
      const subscribefields = telemetry_fields.filter (field => telefields.includes (field.name));
      this.tsub = Util.telemetry_subscribe (this.recv_telemetry.bind (this), subscribefields);
    }
  }
  disconnectedCallback()
  {
    super.disconnectedCallback();
    this.tsub && Util.telemetry_unsubscribe (this.tsub);
    this.tsub = null;
    this.counter_text = null;
    this.timer_text = null;
  }
  apply_sig (v)
  {
    const parts = ("" + v).split ('/');
    if (parts.length == 2) {
      const n = Number (parts[0]), d = Number (parts[1]);
      if (n > 0 && d > 0) {
	App.project.numerator = n;
	App.project.denominator = d;
      }
    }
  }
  recv_telemetry (tsub, arrays)
  {
    if (!this.timer_text) return;
    const ds = "\u2007"; // FIGURE SPACE - "Tabular width", the width of digits
    const s3 = n => (n >= 100 ? "" : n >= 10 ? ds : ds + ds) + n;
    const s2 = n => (n >= 10 ? "" : ds) + n;
    const z2 = n => (n >= 10 ? "" : "0") + n;
    const ff = (n, d = 2) => Number.parseFloat (n).toFixed (d);
    // const tick = arrays[tsub.current_tick.type][tsub.current_tick.index];
    // const bpm = arrays[tsub.current_bpm.type][tsub.current_bpm.index];
    const bar = arrays[tsub.current_bar.type][tsub.current_bar.index];
    const beat = arrays[tsub.current_beat.type][tsub.current_beat.index];
    const sixteenth = arrays[tsub.current_sixteenth.type][tsub.current_sixteenth.index];
    const minutes = arrays[tsub.current_minutes.type][tsub.current_minutes.index];
    const seconds = arrays[tsub.current_seconds.type][tsub.current_seconds.index];
    const barpos = s3 (1 + bar) + "." + s2 (1 + beat) + "." + (1 + sixteenth).toFixed (2);
    const timepos = z2 (minutes) + ":" + z2 (ff (seconds, 3));
    if (this.counter_text.nodeValue != barpos)
      this.counter_text.nodeValue = barpos;
    if (this.timer_text.nodeValue != timepos)
      this.timer_text.nodeValue = timepos;
  }
}
customElements.define ('b-positionview', BPositionView);
