// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
import { LitElement, html, css, postcss, docs, ref } from '../little.js';

/** ## Clip-List
 * The Clip-List allows to start playback of individual clips.
 */

// == STYLE ==
const STYLE = await postcss`
@import 'mixins.scss';
:host {
  display: flex;
  position: relative;
  .-indicator {
    position: absolute; top: 0; bottom: 0; left: 0; width: 1px; height: 100%;
    background: $b-piano-roll-indicator;
    z-index: 2;
    transform: translateX(-9999px);
  }
}
b-clipview {
  margin: 0 1px;
  width: $b-clipthumb-width;
  flex-shrink: 0; flex-grow: 0;
}
`;

// == HTML ==
const HTML = (t, d) => [
  t.wtrack.launcher_clips.map ((clip, index) =>
    html`  <b-clipview .clip=${clip} index=${index} .track=${t.track} trackindex=${t.trackindex} ></b-clipview>`
  ),
  html`    <span class="-indicator" ${ref (h => t.indicator_bar = h)} /> `,
];

// == SCRIPT ==
import * as Ase from '../aseapi.js';

const OBJECT_PROPERTY = { attribute: false };
const NUMBER_ATTRIBUTE = { type: Number, reflect: true }; // sync attribute with property

class BClipList extends LitElement {
  static styles = [ STYLE ];
  static properties = {
    track:	OBJECT_PROPERTY,
    trackindex:	NUMBER_ATTRIBUTE,
  };
  render()
  {
    const d = {};
    return HTML (this, d);
  }
  constructor()
  {
    super();
    this.track = null;
    this.wtrack = { launcher_clips: [] };	// dummy
    this.trackindex = -1;
    this.clipviews = [];
    this.teleobj = null;
    this.telemetry = null;
  }
  connectedCallback() {
    super.connectedCallback();
    this.ratiomul = window.devicePixelRatio;
    this.ratiodiv = 1.0 / this.ratiomul;
    this.setAttribute ('data-f1', "#clip-list");
  }
  disconnectedCallback() {
    super.disconnectedCallback();
    this.telemetry = null;
    Util.telemetry_unsubscribe (this.teleobj);
    this.teleobj = null;
  }
  updated (changed_props)
  {
    if (changed_props.has ('track'))
      {
	const weakthis = new WeakRef (this); // avoid strong wtrack->this refs for automatic cleanup
	this.wtrack = Util.wrap_ase_object (this.track, { launcher_clips: [] }, () => weakthis.deref()?.requestUpdate());
	Util.telemetry_unsubscribe (this.teleobj);
	this.teleobj = null;
	const async_updates = async () => {
	  this.telemetry = await Object.freeze (this.track.telemetry());
	  if (!this.teleobj && this.telemetry)
	    this.teleobj = Util.telemetry_subscribe (this.recv_telemetry.bind (this), this.telemetry);
	};
	async_updates();
      }
    this.clipviews.length = 0;
    for (const clipview of this.shadowRoot.querySelectorAll (":host > b-clipview"))
      {
	this.clipviews.push ({ width: clipview.getBoundingClientRect().width,
			       tickscale: clipview.tickscale,
			       x: clipview.offsetLeft, });
      }
  }
  recv_telemetry (teleobj, arrays)
  {
    const current = arrays[teleobj.current_clip.type][teleobj.current_clip.index];
    const tick = arrays[teleobj.current_tick.type][teleobj.current_tick.index];
    // const next = arrays[teleobj.next.type][teleobj.next.index];
    let u;
    if (current >= 0 && current < this.clipviews.length && tick >= 0)
      {
	const cv = this.clipviews[current];
	const t = cv.x + tick * cv.tickscale;
	u = Math.round (t * this.ratiomul) * this.ratiodiv; // screen pixel alignment
      }
    else
      u = -9999;
    if (u != this.last_pos)
      {
	this.indicator_bar.style = "transform: translateX(" + u + "px);";
	this.last_pos = u;
      }
    if (Shell.piano_current_tick &&
	Shell.piano_current_clip == this.wtrack.launcher_clips[current])
      Shell.piano_current_tick (this.wtrack.launcher_clips[current], tick);
  }
}

customElements.define ('b-cliplist', BClipList);
