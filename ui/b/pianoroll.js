// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

/** # B-PIANOROLL
 * A pianoroll element.
 * ## Properties:
 * *.isactive*: bool (uri)
 * : Property callback used to check if a particular piano roll should stay active or be disabled.
 * ## Attributes:
 * *clip*
 * : The clip with notes to be edited.
 */

import { LitElement, ref, html, css, postcss, docs } from '../little.js';
import * as PianoCtrl from "./piano-ctrl.js";
const floor = Math.floor, round = Math.round;

// == STYLE ==
const STYLE = await postcss`
@import 'shadow.scss';
@import 'mixins.scss';
:host {
  display: flex; flex-direction: column; align-items: stretch;
  position: relative;
  // Make scss variables available to JS via getComputedStyle()
  $b-piano-roll-key-length: 64px;
  --piano-roll-light-row:    $b-piano-roll-light-row;
  --piano-roll-dark-row:     $b-piano-roll-dark-row;
  --piano-roll-grid-main:    zmod($b-piano-roll-light-row, Jz+=22.5);   // bar separator
  --piano-roll-grid-sub:     zmod($b-piano-roll-light-row, Jz+=13.5);   // quarter note separator
  --piano-roll-semitone12:   zmod($b-piano-roll-light-row, Jz+=22.5);   // separator per octave
  --piano-roll-semitone6:    zmod($b-piano-roll-light-row, Jz+=22.5);   // separator after 6 semitones

  --piano-roll-white-base:   $b-piano-roll-white-base;
  --piano-roll-white-border: $b-scrollboundary-color;                   // border around piano key
  --piano-roll-white-glint:  zmod($b-piano-roll-white-base, Jz+=6.5);   // highlight on piano key
  --piano-roll-key-color:    $b-scrollboundary-color;
  --piano-roll-black-base:   $b-piano-roll-black-base;
  --piano-roll-black-border: zmod($b-piano-roll-black-base, Jz+=3.8);   // border around piano key
  --piano-roll-black-glint:  zmod($b-piano-roll-black-base, Jz+=14.3);  // highlight on piano key
  --piano-roll-black-shine:  zmod($b-piano-roll-black-base, Jz+=33.5);  // reflection on piano key

  --piano-roll-font:                  $b-piano-roll-font;
  --piano-roll-num-color:             $b-piano-roll-num-color;
  --piano-roll-note-color:            $b-piano-roll-note-color;
  --piano-roll-note-focus-color:      $b-piano-roll-note-focus-color;
  --piano-roll-note-focus-border:     $b-piano-roll-note-focus-border;
  --piano-roll-key-length:            $b-piano-roll-key-length;
}
c-grid {
  background: $b-piano-roll-black-base;
  position: absolute; left: 0; right: 0; top: 0; bottom: 0;
  color: $b-piano-roll-white-base;
  align-items: stretch;

  grid-template-columns: min-content 1fr min-content;
  grid-template-rows:    min-content 1fr min-content;
  .-col1		{ grid-column: 1/2; }
  .-col2		{ grid-column: 2/3; }
  .-col3		{ grid-column: 3/4; }
  .-row1		{ grid-row: 1/2; }
  .-row2		{ grid-row: 2/3; }
  .-row3		{ grid-row: 3/4; }


  background: red;
  canvas { background: black; object-fit: contain;
    min-width: 0; min-height: 0; // https://www.w3.org/TR/css3-grid-layout/#min-size-auto
  }
  .-indicator {
    position: absolute; top: 0; bottom: 0; left: 0; width: 1px; height: 100%;
    background: $b-piano-roll-indicator;
    z-index: 2; backface-visibility: hidden; will-change: transform;
    transform: translateX(-9999px);
  }
}
`;

// == HTML ==
const HTML = (t, d) => html`
  <c-grid @wheel=${t.wheel} >
    <div class="-col1 -row1" ${ref(e => t.menu_btn = e)} style="height: 1.7em" >Menu</div>
    <canvas class="-col2 -row1" ${ref(e => t.time_canvas = e)} ></canvas>
    <canvas class="-col1 -row2" ${ref(e => t.piano_canvas = e)} ></canvas>
    <canvas class="-col2 -row2" ${ref(e => t.notes_canvas = e)} ></canvas>
    <div class="-col3 -row2" style="overflow: hidden scroll; min-width: 17px; background: #000" ${ref(e => t.vscrollbar = e)} >
      <div style="height: 151vh; width: 16px; margin-left: -3px; background1: #808" ${ref(e => t.vscrollbar_extend = e)} >
	S <br />
	C <br />
	R <br />
	O <br />
	L <br />
	L <br />
      </div>
    </div>
    <div class="-col2 -row3" ${ref(e => t.hscrollbar = e)} style="overflow: scroll hidden; min-height: 17px; background: #000" >
      <div ${ref(e => t.hscrollbar_extend = e)} style="width:999px; height: 16px; margin-top: -8px; width:999px; background1: #080" > Scroll Row </div>
    </div>
    <span class="-indicator" ${ref(e => t.indicator_bar = e)} />
  </c-grid>
`;

// == SCRIPT ==
const BOOL_ATTRIBUTE = { type: Boolean, reflect: true }; // sync attribute with property
const STRING_ATTRIBUTE = { type: String, reflect: true }; // sync attribute with property
const PRIVATE_PROPERTY = { state: true };

class BPianoRoll extends LitElement {
  static styles = [ STYLE ];
  static shadowRootOptions = { ...LitElement.shadowRootOptions, delegatesFocus: true };
  render()
  {
    const d = {};
    return HTML (this, d);
  }
  static properties = {
    clip: STRING_ATTRIBUTE,
    disabled: BOOL_ATTRIBUTE,
    iconclass: STRING_ATTRIBUTE,
    isactive: PRIVATE_PROPERTY,
  };
  constructor()
  {
    super();
    this.menu_btn = null;
    this.notes_canvas = null;
    this.piano_canvas = null;
    this.time_canvas = null;
    this.hscrollbar = null;
    this.vscrollbar = null;
    this.indicator_bar = null;
    this.last_pos = -9.987;
    this.srect = { x: 0, y: 0, w: 0, h: 0 };
    this.clip = null;
    this.disabled = false;
    this.iconclass = '';
    this.isactive = true;
    this.hzoom = 1;
    this.vzoom = 1;
    this.end_click = 99999;
    this.resize_observer = new ResizeObserver (els => this.repaint (true, els));
    this.resize_observer.observe (this);
    this.resize_observer.observe (document.body);
    this.vscroll_must_center = true; // flag for initial vertical centering
  }
  disconnectedCallback()
  {
    super.disconnectedCallback();
    Shell.piano_current_tick = null;
    Shell.piano_current_clip = null;
  }
  updated (changed_props)
  {
    if (this.hscrollbar && !this.hscrollbar.onscroll)
      this.hscrollbar.onscroll = e => this.repaint (false);
    if (this.vscrollbar && !this.vscrollbar.onscroll)
      this.vscrollbar.onscroll = e => this.repaint (false);
    if (changed_props.has ('clip'))
      {
	this.hscrollbar.scrollTo ({ left: 0, behavior: 'instant' });
	this.vscroll_to (0.5);
      }
    this.repaint (true);
    // indicator_bar setup
    this.dpr_mul = window.devicePixelRatio;
    this.dpr_div = 1.0 / this.dpr_mul;
    Shell.piano_current_tick = this.piano_current_tick.bind (this);
    Shell.piano_current_clip = this.clip;
  }
  piano_current_tick (current_clip, current_tick)
  {
    if (this.clip != current_clip) return;
    const offst = this.layout.xscroll();
    const t = -offst + current_tick * this.layout.tickscale * this.dpr_div;
    let u = Math.round (t * this.dpr_mul) * this.dpr_div; // screen pixel alignment
    u = u >= 0 ? u + this.notes_canvas.offsetLeft : -9999;
    if (u != this.last_pos)
      {
	const indicator_transform = "translateX(" + u + "px)";
	this.last_pos = u;
	if (this.indicator_bar)
	  this.indicator_bar.style.transform = indicator_transform;
      }
  }
  vscroll_to (fraction)
  {
    const vrange = this.vscrollbar_extend.clientHeight - this.vscrollbar.clientHeight;
    this.vscrollbar.scrollTo ({ top: fraction * vrange, behavior: 'instant' });
  }
  repaint (resize = false, els = undefined)
  {
    if (!this.notes_canvas || !this.hscrollbar || !this.vscrollbar)
      return;
    if (resize)
      piano_layout.call (this);
    if (resize && this.vscroll_must_center && this.vscrollbar.clientHeight)
      this.vscroll_must_center = (this.vscroll_to (0.5), false);
    paint_notes.call (this);
    paint_timeline.call (this);
    paint_piano.call (this);
  }
  wheel (event)
  {
    const delta = Util.wheel_delta (event);
    if (Math.abs (delta.x) > Math.abs (delta.y))
      this.hscrollbar.scrollBy ({ left: delta.x });
    else
      this.vscrollbar.scrollBy ({ top: delta.y });
    Util.prevent_event (event);
  }
}

customElements.define ('b-piano-roll', BPianoRoll);



const hscrollbar_proportion = 20, vscrollbar_proportion = 11;

// == piano_layout ==
function piano_layout () {
  const notes_canvas = this.notes_canvas, timeline_canvas = this.time_canvas;
  const piano_canvas = this.piano_canvas, cstyle = getComputedStyle (this);
  const notes_cssheight = Math.floor (this.vzoom * 84 / 12) * PianoCtrl.PIANO_KEYS;
  /* By design, each octave consists of 12 aligned rows that are used for note placement.
   * Each row is always pixel aligned. Consequently, the pixel area assigned to an octave
   * can only shrink or grow in 12 screen pixel intervalls.
   * The corresponding white and black keys are also always pixel aligned, variations in
   * mapping the key sizes to screen coordinates are distributed over the widths of the keys.
   */
  const DPR = window.devicePixelRatio;
  const layout = {
    DPR:		DPR,
    thickness:		Math.max (round (DPR * 0.5), 1),
    cssheight:		notes_cssheight + 1,
    piano_csswidth:	0,			// derived from white_width
    notes_csswidth:	0,			// display width, determined by parent
    virt_width:		0,			// virtual width in CSS pixels, derived from end_tick
    beat_pixels:	50,			// pixels per quarter note
    tickscale:		undefined,		// pixels per tick
    octaves:		PianoCtrl.PIANO_OCTAVES,// number of octaves to display
    yoffset:		notes_cssheight,	// y coordinate of lowest octave
    oct_length:		undefined,		// 12 * 7 = 84px for vzoom==1 && DPR==1
    row:		undefined,		// 7px for vzoom==1 && DPR==1
    bkeys:		[], 			// [ [offset,size] * 5 ]
    wkeys:		[], 			// [ [offset,size] * 7 ]
    row_colors:		[ 1, 2, 1, 2, 1,   1, 2, 1, 2, 1, 2, 1 ],		// distinct key colors
    white_width:	54,			// length of white keys, --piano-roll-key-length
    black_width:	0.59,			// length of black keys (pre-init factor)
    label_keys:		1,			// 0=none, 1=roots, 2=whites
    black2midi:         [   1,  3,     6,  8,  10,  ],
    white2midi:         [ 0,  2,  4, 5,  7,  9,  11 ],
  };
  const black_keyspans = [  [7,7], [21,7],     [43,7], [56.5,7], [70,7]   ]; 	// for 84px octave
  const white_offsets  = [ 0,    12,     24, 36,     48,       60,     72 ]; 	// for 84px octave
  const key_length = parseFloat (cstyle.getPropertyValue ('--piano-roll-key-length'));
  const min_end_tick = 16 * (4 * Util.PPQN);
  const end_tick = Math.max (this.end_tick || 0, min_end_tick);
  // scale layout
  layout.dpr_height = round (layout.DPR * layout.cssheight);
  layout.white_width = key_length || layout.white_width; // allow CSS override
  layout.piano_csswidth = layout.white_width;
  const hscrollbar_width = this.hscrollbar.clientWidth;
  const vscrollbar_height = this.vscrollbar.clientHeight;

  layout.notes_csswidth = hscrollbar_width;
  layout.beat_pixels = round (layout.beat_pixels * DPR * this.hzoom);
  layout.tickscale = layout.beat_pixels / Util.PPQN;
  layout.hpad = 10 * DPR;
  layout.virt_width = Math.ceil (layout.tickscale * end_tick);
  layout.row = Math.floor (layout.dpr_height / PianoCtrl.PIANO_KEYS);
  layout.oct_length = layout.row * 12;
  layout.white_width = round (layout.white_width * layout.DPR);
  layout.black_width = round (layout.white_width * layout.black_width);
  // assign white key positions and aligned sizes
  let last = layout.oct_length;
  for (let i = white_offsets.length - 1; i >= 0; i--) {
    const key_start = round (layout.oct_length * white_offsets[i] / 84);
    const key_size = last - key_start;
    layout.wkeys.unshift ( [key_start, key_size] );
    last = key_start;
  }
  // assign black key positions and sizes
  for (let i = 0; i < black_keyspans.length; i++) {
    const key_start = round (layout.oct_length * black_keyspans[i][0] / 84.0);
    const key_end   = round (layout.oct_length * (black_keyspans[i][0] + black_keyspans[i][1]) / 84.0);
    layout.bkeys.push ([key_start, key_end - key_start]);
  }
  // resize piano
  Util.resize_canvas (piano_canvas, layout.piano_csswidth, vscrollbar_height); // layout.cssheight
  piano_canvas.style.height = '100%';
  // resize timeline
  Util.resize_canvas (timeline_canvas, layout.notes_csswidth, this.menu_btn.clientHeight);
  timeline_canvas.style.width = '100%';
  // resize notes
  Util.resize_canvas (notes_canvas, layout.notes_csswidth, vscrollbar_height); // layout.cssheight
  notes_canvas.style.width = '100%';
  notes_canvas.style.height = '100%';

  // vscrollbar setup
  let px, layout_changed = false;
  if (0) // vscrollbar resizing unused, vscrollbar_extend is fixed atm
    {
      px = (vscrollbar_height * (vscrollbar_proportion + 1)) + 'px';
      if (this.vscrollbar_extend.style.height != px)
	{
	  layout_changed = true;
	  this.vscrollbar_extend.style.height = px;
	}
    }
  layout.yoffset = () => {
    const yscroll = this.vscrollbar.scrollTop / Math.max (1, this.vscrollbar_extend.clientHeight - this.vscrollbar.clientHeight);
    let yoffset = layout.dpr_height - yscroll * (layout.dpr_height - vscrollbar_height * DPR);
    yoffset -= 2 * layout.thickness; // leave room for overlapping piano key borders
    return yoffset;
  };
  layout.yscroll = () => layout.yoffset() / layout.DPR;
  // hscrollbar setup
  px = (hscrollbar_width * (hscrollbar_proportion + 1)) + 'px';
  if (this.hscrollbar_extend.style.width != px)
    {
      layout_changed = true;
      this.hscrollbar_extend.style.width = px;
    }
  layout.xposition = () => {
    const xscroll = this.hscrollbar.scrollLeft / (hscrollbar_width * hscrollbar_proportion);
    return xscroll * layout.virt_width - layout.hpad;
  };
  layout.xscroll = () => layout.xposition() / layout.DPR;
  // restore scroll & zoom
  if (this.auto_scrollto)
    {
      this.vscrollbar.scrollTop = this.auto_scrollto.vscrollpos * Math.max (1, this.vscrollbar_extend.clientHeight - this.vscrollbar.clientHeight);
      this.hscrollbar.scrollLeft = this.auto_scrollto.hscrollpos * (hscrollbar_width * hscrollbar_proportion);
      this.auto_scrollto = undefined;
    }
  // conversions
  layout.tick_from_x = css_x => {
    const xp = css_x * layout.DPR;
    const tick = Math.round ((layout.xposition() + xp) / layout.tickscale);
    return tick;
  };
  layout.midinote_from_y = css_y => {
    const yp = css_y * layout.DPR;
    const yoffset = layout.yoffset();
    const nthoct = Math.trunc ((yoffset - yp) / layout.oct_length);
    const inoct = (yoffset - yp) - nthoct * layout.oct_length;
    const octkey = Math.trunc (inoct / layout.row);
    const midioct = nthoct - 1;
    const midinote = (midioct + 1) * 12 + octkey;
    return midinote; // [ midioct, octkey, midinote ]
  };
  this.layout = Object.freeze (layout); // effectively 'const'
  return layout_changed;
}

// == canvas_font ==
function canvas_font (size) {
  const cstyle = getComputedStyle (this);
  const fontstring = cstyle.getPropertyValue ('--piano-roll-font');
  const parts = fontstring.split (/\s*\d+px\s*/i); // 'bold 10px sans' -> ['bold', 'sans']
  console.assert (parts && parts.length >= 1);
  const font = parts[0] + ' ' + size + ' ' + (parts[1] || '');
  return font;
}

// == paint_piano ==
function paint_piano()
{
  const canvas = this.piano_canvas, cstyle = getComputedStyle (this);
  const ctx = canvas.getContext ('2d'), csp = cstyle.getPropertyValue.bind (cstyle);
  const layout = this.layout, DPR = layout.DPR, yoffset = layout.yoffset();
  // resize canvas to match onscreen pixels, paint bg with white key row color
  const light_row = csp ('--piano-roll-light-row');
  ctx.fillStyle = light_row;
  ctx.fillRect (0, 0, layout.piano_csswidth * layout.DPR, layout.cssheight * layout.DPR);
  // we draw piano keys horizontally within their boundaries, but verticaly overlapping by one th
  const th = layout.thickness, hf = th * 0.5; // thickness/2 fraction

  // draw piano keys
  const white_base = csp ('--piano-roll-white-base');
  const white_glint = csp ('--piano-roll-white-glint');
  const white_border = csp ('--piano-roll-white-border');
  const black_base = csp ('--piano-roll-black-base');
  const black_glint = csp ('--piano-roll-black-glint');
  const black_shine = csp ('--piano-roll-black-shine');
  const black_border = csp ('--piano-roll-black-border');
  for (let oct = 0; oct < layout.octaves; oct++) {
    const oy = yoffset - oct * layout.oct_length;
    // draw white keys
    ctx.fillStyle = white_base;
    ctx.lineWidth = th;
    for (let k = 0; k < layout.wkeys.length; k++) {
      const p = layout.wkeys[k];
      const x = DPR, y = oy - p[0];
      const w = layout.white_width - DPR, h = p[1];
      ctx.fillRect (x, y - h + th, w - 2 * th, h - th);		// v-overlap by 1*th
      const sx = x + hf, sy = y - h + hf;			// stroke coords
      ctx.strokeStyle = white_glint;				// highlight
      ctx.strokeRect (sx, sy + th, w - th, h - th);
      ctx.strokeStyle = white_border;				// border
      ctx.strokeRect (sx - th, sy, w, h);			// v-overlap by 1*th
    }
    // draw black keys
    ctx.fillStyle = black_base;
    ctx.lineWidth = th;
    for (let k = 0; k < layout.bkeys.length; k++) {
      const p = layout.bkeys[k];
      const x = DPR, y = oy - p[0];
      const w = layout.black_width, h = p[1];
      const gradient = [ [0, black_base], [.08, black_base], [.15, black_shine],   [1, black_base] ];
      ctx.fillStyle = Util.linear_gradient_from (ctx, gradient, x + th, y - h / 2, x + w - 2 * th, y - h / 2);
      ctx.fillRect   (x + th, y - h + th, w - 2 * th, h - th);	// v-overlap by 1*th
      const sx = x + hf, sy = y - h + hf;			// stroke coords
      ctx.strokeStyle = black_glint;		// highlight
      ctx.strokeRect (sx, sy + th, w - 2 * th, h - th);
      ctx.strokeStyle = black_border;		// border
      ctx.strokeRect (sx, sy, w - th, h);
    }
    // outer border
    ctx.fillStyle = white_border;
    ctx.fillRect (0, 0, DPR, canvas.height);
  }

  // figure font size for piano key labels
  const avg_height = layout.wkeys.reduce ((a, p) => a += p[1], 0) / layout.wkeys.length;
  let fpx = avg_height - 2 * (th + 1);	// base font size on average white key size
  fpx = Util.clamp (fpx / layout.DPR, 7, 12) * layout.DPR;
  if (fpx >= 6) {
    ctx.fillStyle = csp ('--piano-roll-key-color');
    // const key_font = csp ('--piano-roll-font');
    ctx.font = canvas_font.call (this, fpx + 'px');
    // measure Midi labels, faster if batched into an array
    const midi_labels = Util.midi_label ([...Util.range (0, layout.octaves * (layout.wkeys.length + layout.bkeys.length))]);
    // draw names
    ctx.textAlign = 'left';
    ctx.textBaseline = 'bottom';
    // TODO: use actualBoundingBoxAscent once measureText() becomes more sophisticated
    for (let oct = 0; oct < layout.octaves; oct++) {
      const oy = yoffset - oct * layout.oct_length;
      // skip non-roots / roots according to configuration
      for (let k = 0; k < layout.wkeys.length; k++) {
	if ((k && layout.label_keys < 2) || layout.label_keys < 1)
	  continue;
	// draw white key
	const p = layout.wkeys[k];
	const x = 0, y = oy - p[0];
	const w = layout.white_width;
	const midi_key = oct * 12 + layout.white2midi[k];
	const label = midi_labels[midi_key];
	const twidth = ctx.measureText (label).width;
	const tx = x + w - 2 * (th + 1) - twidth, ty = y;
	ctx.fillText (label, tx, ty);
      }
    }
  }
}

// == paint_notes ==
function paint_notes()
{
  const canvas = this.notes_canvas, cstyle = getComputedStyle (this);
  const ctx = canvas.getContext ('2d'), csp = cstyle.getPropertyValue.bind (cstyle);
  const layout = this.layout, yoffset = layout.yoffset();
  const light_row = cstyle.getPropertyValue ('--piano-roll-light-row');
  // resize canvas to match onscreen pixels, paint bg with white key row color
  ctx.fillStyle = light_row;
  ctx.fillRect (0, 0, layout.notes_csswidth * layout.DPR, layout.cssheight * layout.DPR);
  // we draw piano keys verticaly overlapping by one th and align octave separators accordingly
  const th = layout.thickness;

  // paint black key rows
  const dark_row = csp ('--piano-roll-dark-row');
  ctx.fillStyle = dark_row;
  for (let oct = 0; oct < layout.octaves; oct++) {
    const oy = yoffset - oct * layout.oct_length;
    for (let r = 0; r < layout.row_colors.length; r++) {
      if (layout.row_colors[r] > 1) {
	ctx.fillRect (0, oy - r * layout.row - layout.row, canvas.width, layout.row);
      }
    }
  }

  // line thickness and line cap
  ctx.lineWidth = th;
  ctx.lineCap = 'butt'; // chrome 'butt' has a 0.5 pixel bug, so we use fillRect
  const lsx = layout.xposition();

  // draw half octave separators
  const semitone6 = csp ('--piano-roll-semitone6');
  ctx.fillStyle = semitone6;
  const stipple = round (3 * layout.DPR), stipple2 = 2 * stipple;
  const qy = 5 * layout.row; // separator between F|G
  for (let oct = 0; oct < layout.octaves; oct++) {
    const oy = yoffset - oct * layout.oct_length;
    Util.hstippleRect (ctx, th - lsx % stipple2, oy - qy - th, canvas.width, th, stipple);
  }

  // draw vertical grid lines
  paint_timegrid.call (this, canvas, false);

  // draw octave separators
  const semitone12 = csp ('--piano-roll-semitone12');
  ctx.fillStyle = semitone12;
  for (let oct = 0; oct <= layout.octaves; oct++) {	// condiiton +1 to include top border
    const oy = yoffset - oct * layout.oct_length;
    ctx.fillRect (0, oy, canvas.width, th);
  }

  // paint selection rectangle
  const srect = { x: layout.DPR * this.srect.x, y: layout.DPR * this.srect.y,
		  w: layout.DPR * this.srect.w, h: layout.DPR * this.srect.h };
  if (srect.w > 0 && srect.h > 0)
    {
      const note_focus_color = csp ('--piano-roll-note-focus-color');
      ctx.strokeStyle = csp ('--piano-roll-note-focus-border');
      ctx.lineWidth = layout.DPR; // layout.thickness;
      ctx.fillStyle = note_focus_color;
      ctx.fillRect (srect.x, srect.y, srect.w, srect.h);
      ctx.strokeRect (srect.x, srect.y, srect.w, srect.h);
    }

  // paint notes
  if (!this.clip)
    return;
  const tickscale = layout.tickscale;
  const note_color = csp ('--piano-roll-note-color');
  const note_selected_color = csp ('--piano-roll-note-focus-color');
  const note_focus_color = csp ('--piano-roll-note-focus-color');
  ctx.lineWidth = layout.DPR; // layout.thickness;
  ctx.fillStyle = note_color;
  ctx.strokeStyle = csp ('--piano-roll-note-focus-border');
  // draw notes
  for (const note of Shell.get_note_cache (this.clip).notes)
    {
      const oct = floor (note.key / 12), key = note.key - oct * 12;
      const ny = yoffset - oct * layout.oct_length - key * layout.row + 1;
      const nx = round (note.tick * tickscale), nw = Math.max (1, round (note.duration * tickscale));
      if (note.selected)
	ctx.fillStyle = note_selected_color;
      else
	ctx.fillStyle = note_color;
      ctx.fillRect (nx - lsx, ny - layout.row, nw, layout.row - 2);
      if (0) // frame notes
	{
	  ctx.fillStyle = note_focus_color;
	  ctx.strokeRect (nx - lsx, ny - layout.row, nw, layout.row - 2);
	}
    }
}

// == paint_timeline ==
function paint_timeline()
{
  const canvas = this.time_canvas, cstyle = getComputedStyle (this);
  const ctx = canvas.getContext ('2d'), csp = cstyle.getPropertyValue.bind (cstyle);
  const layout = this.layout, light_row = csp ('--piano-roll-light-row');
  // paint bg with white key row color
  ctx.fillStyle = light_row;
  ctx.fillRect (0, 0, layout.notes_csswidth * layout.DPR, canvas.height);

  paint_timegrid.call (this, canvas, true);
}

// == paint_timegrid ==
function paint_timegrid (canvas, with_labels)
{
  const signature = [ 4, 4 ]; // 15, 16
  const cstyle = getComputedStyle (this), gy1 = 0;
  const gy2 = canvas.height * (with_labels ? 0.5 : 0), gy3 = canvas.height * (with_labels ? 0.75 : 0);
  const ctx = canvas.getContext ('2d'), csp = cstyle.getPropertyValue.bind (cstyle);
  const layout = this.layout, lsx = layout.xposition(), th = layout.thickness;
  const grid_main = csp ('--piano-roll-grid-main'), grid_sub = csp ('--piano-roll-grid-sub');
  const TPN64 = Util.PPQN / 16;			// Ticks per 64th note
  const TPD = TPN64 * 64 / signature[1];	// Ticks per denominator fraction
  const bar_ticks = signature[0] * TPD;		// Ticks per bar
  const bar_pixels = bar_ticks * layout.tickscale;
  const denominator_pixels = bar_pixels / signature[0];
  const barjumps = 8;
  ctx.lineWidth = th; // line thickness
  ctx.lineCap = 'butt'; // chrome 'butt' has a 0.5 pixel bug, so we use fillRect

  // determine stepping granularity
  let stepping; // [ ticks_per_step, steps_per_mainline, steps_per_midline ]
  const mingap = th * 17;
  if (denominator_pixels / 16 >= mingap)
    stepping = [ TPD / 16, 16, 4 ];
  else if (denominator_pixels / 4 >= mingap)
    stepping = [ TPD / 4, 4 * signature[0], 4 ];
  else if (denominator_pixels >= mingap)
    stepping = [ TPD, signature[0], 0 ];
  else // just use bars
    stepping = [ bar_ticks, 0, 0 ];
  this.stepping = stepping;

  // first 2^x aligned bar tick before/at xposition
  const start_bar = floor ((lsx + layout.hpad) / (barjumps * bar_pixels));
  const start = start_bar * bar_ticks;

  // step through visible tick fractions and draw lines
  let tx = 0, c = 0, d = 0;
  const grid_sub2 = stepping[2] ? grid_main : grid_sub;
  for (let tick = start; tx < canvas.width; tick += stepping[0])
    {
      tx = tick * layout.tickscale - lsx;
      ctx.fillStyle = c ? d ? grid_sub : grid_sub2 : grid_main;
      const gy = c ? d ? gy3 : gy2 : gy1;
      ctx.fillRect (tx, gy, th, canvas.height);
      c += 1;
      if (c >= stepping[1])
	c = 0;
      d += 1;
      if (d >= stepping[2])
	d = 0;
    }

  if (!with_labels)
    return;

  // step through all denominators and draw labels
  ctx.fillStyle = csp ('--piano-roll-num-color');
  // const num_font = csp ('--piano-roll-font');
  // const fpx_parts = num_font.split (/\s*\d+px\s*/i); // 'bold 10px sans' -> [ ['bold', 'sans']
  ctx.font = canvas_font.call (this, layout.DPR + 'em');
  ctx.textAlign = 'left';
  ctx.textBaseline = 'top';
  c = 0;
  tx = 0;
  let bar = start_bar;
  for (let tick = start; tx < canvas.width; tick += TPD)
    {
      tx = tick * layout.tickscale - lsx;
      let label = (1 + bar) + '';
      if (c) // fractions
	label += '.' + (1 + c);
      const tm = ctx.measureText (label);
      const lh = tm.actualBoundingBoxAscent + tm.actualBoundingBoxDescent;
      if ((c && tm.width < denominator_pixels * 0.93) ||
	  (!c && tm.width < bar_pixels * 0.93) ||
	  (!c && !(bar & 0x7) && tm.width < bar_pixels * barjumps * 0.93))
	ctx.fillText (label, tx + th, (canvas.height - lh) * 0.5);
      c += 1;
      if (c >= signature[0])
	{
	  bar += 1;
	  c = 0;
	}
    }
}

