// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

/** ## Clip-View
 * Display a small view of a MIDI clip.
 */

import { LitComponent, LitElement, html, JsExtract, docs, ref } from '../little.js';

// == STYLE ==
const STYLE = await JsExtract.fetch_css (import.meta);
JsExtract.scss`
@import 'mixins.scss';
:host {
  display: inline-grid;
  width: 7em;
  margin: 0 0 0 2px;
}
$b-clipview-font: normal 9px $b-theme-font-family !default;    // the '9px' part is dynamically resized
$b-clipview-font-color: rgba(255, 255, 255, 0.7) !default;
$b-clipview-note-color: rgba(255, 255, 255, 0.7) !default;
$b-clipview-color-hues: 75, 177, 320, 225, 45, 111, 5, 259, 165, 290;

.b-clipview {
  display: flex; position: relative;
  flex-shrink: 0;
  border: 0;
  .-canvas {
    display: inline; position: absolute; top: 0px; bottom: 0px; left: 0px; right: 0px;
    --clipview-font-color: #{$b-clipview-font-color}; --clipview-font: #{$b-clipview-font};
    --clipview-note-color: #{$b-clipview-note-color};
    --clipview-color-hues: $b-clipview-color-hues;
    box-shadow: inset 0px 0 1px #fff9, inset -1px 0 1px #000;
    border-radius: $b-button-radius;
  }
  .-play {
    display: inline;
    position: absolute; top: 0px; left: 0px;
    padding: 3px;
    position: absolute;
    padding: 3px;
    top: 50%;
    left: 50%;
    transform: translate(-50%, -50%);
    color: $b-clip-play-fg;
    background: $b-clip-play-bg;
    border-radius: calc($b-button-radius * 0.66);
  }
}
`;

// == HTML ==
const HTML = (t, d) => html`
<h-flex class="b-clipview"
  @click=${t.click}
  >
  <canvas class="-canvas" ${ref (h => t.canvas = h)} ></canvas>
  <span class="-play" @click.stop="click_play" >▶</span>
</h-flex>
`;

// == SCRIPT ==
import * as Util from '../util.js';
import * as Ase from '../aseapi.js';
const tick_quant = Util.PPQN;
const NUMBER_ATTRIBUTE = { type: Number, reflect: true }; // sync attribute with property
const PRIVATE_PROPERTY = { state: true };
const OBJECT_PROPERTY = { attribute: false };

class BClipView extends LitComponent {
  static styles = [ STYLE ];
  static shadowRootOptions = { ...LitElement.shadowRootOptions, delegatesFocus: true };
  static properties = {
    clip: OBJECT_PROPERTY,
    track: OBJECT_PROPERTY,
    trackindex: NUMBER_ATTRIBUTE,
    end_tick: PRIVATE_PROPERTY,	// trigger this.requestUpdate()
  };
  render()
  {
    const d = {};
    return HTML (this, d);
  }
  constructor()
  {
    super();
    this.clip = null;
    this.wclip_ = null;
    this.canvas = null;
    this.starttick = 0;
    this.end_tick = null; // FIXME
    this.trackindex = -1;
    this.tickscale = 10.0 / Util.PPQN; // TODO: adjust hzoom
    this.notes_changed = () => this.repaint (false);
  }
  get pxoffset()	{ return this.starttick * this.tickscale; }
  get canvas_width()	{ return this.tickscale * Math.floor ((this.end_tick + tick_quant - 1) / tick_quant) * tick_quant; }
  updated (changed_props)
  {
    if (changed_props.has ('clip'))
      {
	const weakthis = new WeakRef (this); // avoid strong wclip->this refs for automatic cleanup
	this.wclip_ = Util.wrap_ase_object (this.clip, { end_tick: 0, name: '   ' }, () => weakthis.deref()?.requestUpdate());
	this.wclip_.__add__ ('all_notes', [], () => weakthis.deref()?.notes_changed());
      }
    this.repaint (true);
  }
  repaint (layoutchange)
  {
    render_canvas.call (this);
  }
  click (event)
  {
    App.open_piano_roll (this.clip);
    Util.prevent_event (event);
  }
  click_play() {
    debug ("PLAY: clip:", this.clip.$id);
  }
}

customElements.define ('b-clipview', BClipView);

import * as Z from '../zcam-js.mjs';

const sRGB_viewing_conditions = {
  Fs: Z.ZCAM_DIM,       // DIM comes closest to CIELAB L* in ZCAM and CIECAM97
  Yb: 9.1,              // Background luminance factor so Jz=50 yields #777777
  La: 80,               // La = Lw * Yb / 100; Safdar21, ZCAM, a colour appearance model
  Xw: Z.ZCAM_D65.x, Yw: Z.ZCAM_D65.y, Zw: Z.ZCAM_D65.z,
};
const default_gamut = new Z.Gamut (sRGB_viewing_conditions);

function render_canvas () {
  // canvas setup
  const canvas = this.canvas;
  const pixelratio = Util.resize_canvas (canvas, canvas.parentElement.clientWidth, canvas.parentElement.clientHeight, true);
  const ctx = canvas.getContext ('2d'), cstyle = getComputedStyle (canvas), csp = cstyle.getPropertyValue.bind (cstyle);
  const width = canvas.width, height = canvas.height;
  const tickscale = this.tickscale * pixelratio;
  //const width = canvas.clientWidth, height = canvas.clientHeight;
  //canvas.width = width; canvas.height = height;
  ctx.clearRect (0, 0, width, height);
  // color setup
  let cindex;
  // cindex = Util.hash53 (this.wclip_.name);	// - color from clip name
  // cindex = this.index;				// - color per clip
  // cindex = this.trackindex;			// - color per track
  cindex = this.track.$id;			// - color per track
  const hues = csp ('--clipview-color-hues').split (',');
  const hz = hues[cindex % hues.length];
  const gamut = default_gamut, viewing = gamut.viewing;
  let zcam = { Jz: 55, Cz: 30, viewing, hz: parseFloat (hz), };
  const max_Cz = gamut.maximize_Cz (zcam);
  zcam.Cz = max_Cz * 0.7;
  let rgb = gamut.contains (zcam);
  if (!rgb.inside)
    rgb = gamut.contains (gamut.clamp_chroma (zcam));
  const bgcol = Z.srgb_hex (rgb);
  // paint clip background
  ctx.fillStyle = bgcol;
  ctx.fillRect (0, 0, width, height);
  // draw name
  const fpx = height / 3;
  const note_font = csp ('--clipview-font');
  const fpx_clips = note_font.split (/\s*\d+px\s*/i); // 'bold 10px sans' -> [ ['bold', 'sans']
  ctx.font = fpx_clips[0] + ' ' + fpx + 'px ' + (fpx_clips[1] || '');
  ctx.fillStyle = csp ('--clipview-font-color');
  ctx.textAlign = 'left';
  ctx.textBaseline = 'top';
  ctx.fillText (this.wclip_.name, 1.5, .5);
  // paint notes
  ctx.fillStyle = csp ('--clipview-note-color');
  const noteoffset = 12;
  const notescale = height / (123.0 - 2 * noteoffset); // MAX_NOTE
  for (const note of this.wclip_.all_notes) {
    ctx.fillRect (note.tick * tickscale, height - (note.key - noteoffset) * notescale, note.duration * tickscale, 1 * pixelratio);
  }
}
