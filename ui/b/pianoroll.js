// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** ## Piano-Roll
 * The piano-roll editor displays notes in a grid where the vertical axis denotes the pitch and the horizontal axis the time line.
 * It is modeled after a [classic piano-roll](https://en.wikipedia.org/wiki/Piano_roll#In_digital_audio_workstations).
 * Several tools can be selected via a drop down menu or hotkeys which aid with creation, selection and modification of musical events.
 * The cursor keys can be used to move selected notes, in combination with modifier keys the keys can also change note duration or note focus.
 * A context menu is available with mouse button 3, which provides extended functionality.
 */

import { LitComponent, ref, html, JsExtract, docs } from '../little.js';
import * as PianoCtrl from "./piano-ctrl.js";
import * as Util from '../util.js';
import { clamp } from '../util.js';
import * as Mouse from '../mouse.js';
const floor = Math.floor, round = Math.round;

// == STYLE ==
JsExtract.scss`
b-piano-roll {
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

  --piano-roll-font:                  $b-canvas-font;
  --piano-roll-num-color:             $b-piano-roll-num-color;
  --piano-roll-note-color:            $b-piano-roll-note-color;
  --piano-roll-note-focus-color:      $b-piano-roll-note-focus-color;
  --piano-roll-note-focus-border:     $b-piano-roll-note-focus-border;
  --piano-roll-key-length:            $b-piano-roll-key-length;
  c-grid {
    background: $b-piano-roll-black-base;
    position: absolute; inset: 0;
    align-items: stretch;
    grid-template-columns: min-content 1fr min-content;
    grid-template-rows:    min-content 1fr min-content;
    canvas { background: black; object-fit: contain;
      min-width: 0; min-height: 0; // https://www.w3.org/TR/css3-grid-layout/#min-size-auto
    }
    .-indicator {
      position: absolute; top: 0; bottom: 0; left: 0; width: 1px; height: 100%;
      background: $b-piano-roll-indicator;
      z-index: 2; backface-visibility: hidden; will-change: transform;
      transform: translateX(-9999px);
      pointer-events: none;
    }
    .-hextend {
      background: #0000; opacity: 0; visibility: hidden;
      margin-top: 0; height: 1px;
      // height: 16px; margin-top: -8px; background: #0f0;
    }
    .-vextend {
      background: #0000; opacity: 0; visibility: hidden;
      margin-left: 0; width: 1px;
      // width: 16px; margin-left: -3px; background: #00f;
    }
  }
}`;

// == HTML ==
const HTML = (t, d) => html`
  <c-grid tabindex="-1" ${ref (h => t.cgrid = h)} data-f1="#piano-roll"
    @pointerenter=${t.pointerenter} @pointerleave=${t.pointerleave} @focus=${t.focuschange} @blur=${t.focuschange}
    @keydown=${e => t.piano_ctrl.keydown (e)}
    @wheel=${t.wheel_event} >

    <v-flex class="-toolbutton col-start-1 row-start-1" style="height: 1.7em; align-items: end; padding-right: 4px;" ${ref (h => t.menu_btn = h)}
      @click=${e => t.pianotoolmenu.popup (e)} @mousedown=${e => t.pianotoolmenu.popup (e)} >
      <b-icon style="width: 1.2em; height: 1.2em" ${ref (h => t.menu_icon = h)}></b-icon>
      <b-contextmenu ${ref (h => t.pianotoolmenu = h)} id="g-pianotoolmenu" class="-pianotoolmenu" @activate=${e => t.usetool (e.detail.uri)} >
	<b-menuitem ic="mi-open_with"     uri="S" kbd="1" > Rectangular Selection  </b-menuitem>
	<b-menuitem ic="mi-multiple_stop" uri="H" kbd="2" > Horizontal Selection   </b-menuitem>
	<b-menuitem ic="fa-pencil"        uri="P" kbd="3" > Pen                    </b-menuitem>
	<b-menuitem ic="fa-eraser"        uri="E" kbd="4" > Eraser                 </b-menuitem>
      </b-contextmenu>
    </v-flex>

    <canvas class="-time_canvas col-start-2 row-start-1"  ${ref (h => t.time_canvas = h)} ></canvas>
    <canvas class="-piano_canvas col-start-1 row-start-2" ${ref (h => t.piano_canvas = h)} ></canvas>
    <canvas class="-notes_canvas col-start-2 row-start-2" ${ref (h => t.notes_canvas = h)}
      @pointermove=${Util.debounce (t.notes_canvas_pointermove.bind (t))}
      @pointerdown=${t.notes_canvas_pointerdown} ></canvas>

    <div class="col-start-3 row-start-2" style="overflow: hidden scroll; min-width: 17px; background: #000" ${ref (h => t.vscrollbar = h)} >
      <div class="-vextend" style="height: 151vh" ${ref (h => t.vscrollbar_extend = h)} >
      </div>
    </div>
    <div class="col-start-2 row-start-3" ${ref (h => t.hscrollbar = h)} style="overflow: scroll hidden; min-height: 17px; background: #000" >
      <div class="-hextend" ${ref (h => t.hscrollbar_extend = h)} style="width:999px" ></div>
    </div>
    <span class="-indicator" ${ref (h => t.indicator_bar = h)}></span>

    <b-contextmenu ${ref (h => t.pianorollmenu = h)} id="g-pianorollmenu" :showicons="true"
      class="-pianorollmenu" mapname="Piano Roll"
      .activate=${t.pianorollmenu_click.bind (t)} .isactive=${t.pianorollmenu_check.bind (t)} >
      <b-menutitle> Piano-Roll </b-menutitle>
      ${t.pianorollmenu_actions().map (ac => CONTEXTITEM (ac))}
    </b-contextmenu>
  </c-grid>
`;
// key=${ac.weakid}
const CONTEXTITEM = ac => html`
  <b-menuitem uri=${ac.weakid} ic=${ac.ic} kbd=${ac.kbd} > ${ac.label} </b-menuitem>
`;

// == SCRIPT ==
const PRIVATE_PROPERTY = { state: true };
const default_note_length = Util.PPQN / 4;

/** The <b-piano-roll> element allows note editing. */
class BPianoRoll extends LitComponent {
  createRenderRoot() { return this; }
  render()
  {
    const d = {};
    return HTML (this, d);
  }
  static properties = {
    clip: PRIVATE_PROPERTY,		///< The clip with notes to be edited.
  };
  constructor()
  {
    super();
    this.layout = null;
    this.hzoom = 3;
    this.vzoom = 1.5;
    this.pianotool = 'P';
    this.last_note_length = default_note_length;
    this.cgrid = null;
    this.menu_btn = null;
    this.menu_icon = null;
    this.pianotoolmenu = null;
    this.pianorollmenu = null;
    this.notes_canvas = null;
    this.piano_canvas = null;
    this.time_canvas = null;
    this.hscrollbar = null;
    this.hscrollbar_extend = null;
    this.vscrollbar = null;
    this.vscrollbar_extend = null;
    this.indicator_bar = null;
    this.have_focus = false;
    this.entered = false;
    this.last_pos = -9.987;
    this.srect_ = { x: 0, y: 0, w: 0, h: 0, sx: 0, sy: 0 };
    this.clip = null;
    this.wclip = null;
    this.end_click = 99999;
    this.auto_scrollto = undefined;	// positions to restore scroll & zoom
    this.stepping = [];                 // current grid stepping granularity
    this.vscroll_must_center = true;	// flag for initial vertical centering
    this.pointer_drag = null;
    this.piano_ctrl = new PianoCtrl.PianoCtrl (this);
    this.drag_event = this.piano_ctrl.drag_event.bind (this.piano_ctrl);
    // track repaint dependencies
    this.repaint_pending_ = null;
    this.queue_repaint = () =>
      {
	if (!this.repaint_pending_)
	  this.repaint_pending_ = (async () => { await 0; this.repaint(); }) ();
      };
    this.repaint = Util.reactive_wrapper (this.repaint.bind (this), this.queue_repaint);
    // trigger layout on resize
    this.resize_observer = new ResizeObserver (els => this.requestUpdate.bind (this));
    this.resize_observer.observe (this);
    this.resize_observer.observe (document.body);
  }
  disconnectedCallback()
  {
    super.disconnectedCallback();
    Shell.piano_current_tick = null;
    Shell.piano_current_clip = null;
    if (this.notes_canvas_pointermove_zmovedel)
      {
	this.notes_canvas_pointermove_zmovedel();
	this.notes_canvas_pointermove_zmovedel = null;
      }
  }
  firstUpdated()
  {
    // ensure DOM tee is complete // await this.updateComplete;
    // initial menu button update
    this.usetool (this.pianotool);
  }
  updated (changed_props)
  {
    if (this.hscrollbar && !this.hscrollbar.onscroll)
      this.hscrollbar.onscroll = e => this.hvscroll (e);
    if (this.vscrollbar && !this.vscrollbar.onscroll)
      this.vscrollbar.onscroll = e => this.hvscroll (e);
    if (changed_props.has ('clip'))
      {
	this.wclip?.__cleanup__?.();
	this.wclip = null;
      }
    if (changed_props.has ('clip') && this.clip)
      {
	this.wclip = Util.wrap_ase_object (this.clip, { end_tick: 0, name: '???' }, this.requestUpdate.bind (this));
	this.wclip.__add__ ('all_notes', [], this.queue_repaint);
	this.hscrollbar.scrollTo ({ left: 0, behavior: 'instant' });
	this.vscroll_to (0.5);
	this.last_note_length = default_note_length;
	if (this.indicator_bar)
	  this.indicator_bar.style.transform = "translateX(-9999px)";
      }
    // indicator_bar setup
    Shell.piano_current_tick = this.piano_current_tick.bind (this);
    Shell.piano_current_clip = this.clip;
    // trigger layout, track layout deps from updated()
    this.piano_layout_();
    if (this.vscroll_must_center && this.vscrollbar.clientHeight)
      this.vscroll_must_center = (this.vscroll_to (0.5), false);
    // trigger repaint, but avoid tracking paint deps in updated()
    this.queue_repaint();
  }
  piano_layout_ = piano_layout;
  get srect ()	{ return Object.assign ({}, this.srect_); }
  set srect (r)	{ Object.assign (this.srect_, r); this.queue_repaint(); }
  pointerenter (event)
  {
    this.entered = true;
    if (this.pianotoolmenu)
      this.pianotoolmenu.map_kbd_hotkeys (this.entered || this.have_focus);
  }
  pointerleave (event)
  {
    this.entered = false;
    if (this.pianotoolmenu)
      this.pianotoolmenu.map_kbd_hotkeys (this.entered || this.have_focus);
  }
  focuschange (ev) {
    if (ev?.type)
      this.have_focus = ev.type == "focus";
    if (this.pianotoolmenu)
      this.pianotoolmenu.map_kbd_hotkeys (this.entered || this.have_focus);
    if (this.pianorollmenu)
      this.pianorollmenu.map_kbd_hotkeys (this.have_focus);
  }
  notes_canvas_pointermove (event)
  {
    if (this.pointer_drag)
      return;
    if (this.notes_canvas_pointermove_zmovedel)
      {
	this.notes_canvas_pointermove_zmovedel();
	this.notes_canvas_pointermove_zmovedel = null;
      }
    this.notes_canvas_tool = PianoCtrl.notes_canvas_tool_from_hover (this, event);
    if (this.notes_canvas_tool.cursor != this.notes_canvas.style.cursor)
      this.notes_canvas.style.cursor = this.notes_canvas_tool.cursor;
  }
  notes_canvas_pointerdown (event)
  {
    if (this.pointer_drag)
      {
	this.pointer_drag.destroy();
	return false;
      }
    if (!this.clip)
      return false;
    if (document.activeElement != this.cgrid)
      this.cgrid.focus();
    if (event.button == 2)
      {
	Util.prevent_event (event);
	this.pianorollmenu.popup (event, { origin: 'none' });
	return;
      }
    if (event.button == 0 && this.notes_canvas_tool)
      {
	const ctool_this = this.notes_canvas_tool.drag_start (this);
	this.pointer_drag = new Util.PointerDrag (this, event,
						  (event, MODE) => ctool_this.drag_event (event, MODE),
						  () => this.pointer_drag = null);
	return;
      }
    debug ('b-pianoroll: pointerdown without tool', event);
  }
  pianorollmenu_actions()
  {
    const actions = [];
    for (const action of PianoCtrl.list_actions())
      {
	if (action.label)
	  {
	    const label = action.label;
	    const kbd = action.kbd;
	    const ic = action.ic;
	    actions.push ({ label, weakid: 'weakid:' + Util.weakid (action), kbd, ic });
	  }
      }
    return actions;
  }
  pianorollmenu_check (uri)
  {
    return !!this.clip;
  }
  pianorollmenu_click (uri, event)
  {
    event = Util.keyboard_click_event (event);
    if (event)
      {
	event.stopPropagation();
	event.preventDefault();
      }
    if (uri.search (/^[0-9a-z-]+@[0-9a-z-]+$/) === 0)
      return; // TODO: this.pianoroll_script (uri);
    if (uri.startsWith ('weakid:') && this.clip) {
      const action = Util.weakid_lookup (Number (uri.substr (7)));
      if (action.func instanceof Function)
	return action.func (action, this, this.clip, event);
    }
    console.trace ("piano-roll.vue:", uri, event);
  }
  usetool (uri)
  {
    this.pianotool = uri;
    // clone menu item
    const title = '**EDITOR TOOL**';
    const menuitem = this.pianotoolmenu.find_menuitem (uri);
    this.menu_icon.setAttribute ('ic', menuitem.getAttribute ('ic'));
    this.menu_icon.setAttribute ('data-kbd', menuitem.getAttribute ('kbd'));
    this.menu_icon.setAttribute ('data-tip', title + ' ' + menuitem.slot_label());
    // pick up 'data-tip' and pick cursor via hover
    if (!this.notes_canvas_pointermove_zmovedel)
      this.notes_canvas_pointermove_zmovedel = App.zmoves_add (this.notes_canvas_pointermove.bind (this));
    App.zmove(); // trigger move / hover
  }
  piano_current_tick (current_clip, current_tick)
  {
    if (this.clip != current_clip) return;
    const offst = this.layout.xscroll();
    const t = -offst + current_tick * this.layout.tickscale * this.layout.DPRDIV;
    let u = Math.round (t * this.layout.DPR) * this.layout.DPRDIV; // screen pixel alignment
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
  hvscroll (event)
  {
    // scrollbar(s) changed
    this.queue_repaint();
    // adjust selection, etc
    if (this.pointer_drag)
      this.pointer_drag.scroll (event);
  }
  repaint()
  {
    this.repaint_pending_ = null;
    if (!this.clip || !this.notes_canvas || !this.hscrollbar || !this.vscrollbar)
      return;
    paint_notes.call (this);
    paint_timeline.call (this);
    paint_piano.call (this);
  }
  wheel_event (event)
  {
    const delta = Mouse.wheel_delta (event, true);
    if (event.ctrlKey) {
      if (delta.deltaX)
	this.hzoom = clamp (this.hzoom * (delta.deltaX > 0 ? 1.1 : 0.9), 0.25, 25);
      if (delta.deltaY)
	this.vzoom = clamp (this.vzoom * (delta.deltaY > 0 ? 1.1 : 0.9), 0.5, 25);
      this.request_update_();
    } else {
      if (delta.deltaX)
	this.hscrollbar.scrollBy ({ left: delta.deltaX });
      if (delta.deltaY)
	this.vscrollbar.scrollBy ({ top: delta.deltaY });
    }
    Util.prevent_event (event);
  }
}
customElements.define ('b-piano-roll', BPianoRoll);


const hscrollbar_proportion = 20, vscrollbar_proportion = 11;

/** Determine layout in pixels.
 * @this{BPianoRoll}
 */
function piano_layout()
{
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
    DPRDIV:		1.0 / DPR,
    thickness:		Math.max (round (DPR * 0.5), 1),
    cssheight:		notes_cssheight + 1,
    piano_csswidth:	0,			// derived from white_width
    notes_csswidth:	0,			// display width, determined by parent
    virt_width:		0,			// virtual width in CSS pixels, derived from end_tick
    beat_pixels:	50,			// pixels per quarter note
    tickscale:		undefined,		// pixels per tick
    octaves:		PianoCtrl.PIANO_OCTAVES,// number of octaves to display
    yoffset:		undefined,		// y coordinate of lowest octave
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
  const end_tick = Math.max (this.wclip.end_tick || 0, min_end_tick);
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

/** Assign canvas font to drawing context
 * @this{BPianoRoll}
 */
function set_canvas_font (ctx, size) {
  const cstyle = getComputedStyle (this);
  const fontstring = cstyle.getPropertyValue ('--piano-roll-font');
  const parts = fontstring.split (/\s*\d+px\s*/i); // 'bold 10px sans' -> ['bold', 'sans']
  console.assert (parts && parts.length >= 1);
  const font = parts[0] + ' ' + size + ' ' + (parts[1] || '');
  try {
    ctx.font = font;
  } catch (err) {
    // FireFox throws a "NS_ERROR_FAILURE" exception on first paint
    console.error ("set_canvas_font: ctx.font assignment error" /*, err */ );
    return false;
  }
  return font;
}

/** Paint piano key canvas
 * @this{BPianoRoll}
 */
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
    if (!set_canvas_font.call (this, ctx, fpx + 'px'))
      return; // abort paint
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

/** Paint piano roll notes
 * @this{BPianoRoll}
 */
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
  const srect = { x: layout.DPR * this.srect_.x, y: layout.DPR * this.srect_.y,
		  w: layout.DPR * this.srect_.w, h: layout.DPR * this.srect_.h };
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
  const draw_notes = (selected) => {
    for (const note of this.wclip.all_notes)
      {
        if (note.selected == selected)
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
  };
  // draw selected notes over unselected notes
  draw_notes (false);
  draw_notes (true);
}

/** Paint timeline digits and indicators
 * @this{BPianoRoll}
 */
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

/** Paint timegrid into any canvas
 * @this{BPianoRoll}
 */
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
  if (!set_canvas_font.call (this, ctx, layout.DPR + 'em'))
    return; // abort paint
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

