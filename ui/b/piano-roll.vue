<!-- This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0 -->

<docs>
  # B-PIANO-ROLL
  A Vue template to display notes from a MIDI event source as a piano roll.
  ## Props:
  *msrc*
  : The MIDI event source for editing and display.
  ## Data:
  *hscrollbar*
  : The horizontal scrollbar component, used to provide the virtual scrolling position for the notes canvas.
</docs>

<style lang="scss">
@import 'mixins.scss';
@import 'fonts/AnklangCursors.scss';
$b-piano-roll-key-length: 64px;
$scrollbar-area-size: 15px;
$scrollarea-bg: transparent;
.b-piano-roll {
  //* Make scss variables available to JS via getComputedStyle() */
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
  --piano-roll-note-color:       	$b-piano-roll-note-color;
  --piano-roll-note-focus-color:      $b-piano-roll-note-focus-color;
  --piano-roll-note-focus-border:     $b-piano-roll-note-focus-border;
  --piano-roll-key-length:            $b-piano-roll-key-length;
}

.b-piano-roll {
  outline: none;
  width: 100%;
  @include scrollbar-hover-area;
  //* COLS: VTitle Piano Roll Scrollborder Scrollbar */
  grid-template-columns: min-content $b-piano-roll-key-length 1fr min-content min-content;
  //* ROWS: Timeline Roll Scrollborder Scrollbar */
  grid-template-rows: min-content 1fr min-content min-content;

  .-vtitle {
    text-align: center;
    writing-mode: vertical-rl; transform: rotate(180deg); /* FF: writing-mode: sideways-rl; */
  }
  .-buttons {
    justify-self: stretch; align-self: stretch;
    @include b-softbuttonbar;
    border-bottom: 1px solid $b-scrollboundary-color;	//* edge near scroll area */
    border-right: 1px solid $b-scrollboundary-color;	//* edge near scroll area */
    & > * {
      justify-self: stretch; align-self: stretch;
      padding: 5px;
      @include b-softbutton;
      //* tweak child next to scroll boundary */
      &:last-child {
	border-right: 1px solid transparent;			//* avoid double border */
	&.active, &:active {
	  border-right: 1px inset $b-softbutton-border-color;	//* preserve glint */
	}
      }
    }
    .-toolbutton {
      align-items: center; justify-content: center;
    }
  }
  .-timeline-wrapper {
    height: $b-timeline-outer-height;
    border-bottom: 1px solid $b-scrollboundary-color;	//* edge near scroll area */
  }
  .-vscrollborder {
    border-left: 1px solid $b-scrollboundary-color;  //* edge near scroll area */
  }
  .-hscrollborder {
    border-bottom: 1px solid $b-scrollboundary-color;  //* edge near scroll area */
  }
  .-notes-wrapper, .-piano-wrapper {} // leave these alone to preserve vertical sizing
  canvas { image-rendering: pixelated; background-color: #000; }
  &[data-pianotool='S'] canvas.b-piano-roll-notes { cursor: crosshair; }
  &[data-pianotool='S'] canvas.b-piano-roll-notes[data-notehover="true"] { cursor: default; }
  &[data-pianotool='H'] canvas.b-piano-roll-notes { cursor: col-resize; }
  &[data-pianotool='P'] canvas.b-piano-roll-notes { cursor: $anklang-cursor-pen; }
  &[data-pianotool='E'] canvas.b-piano-roll-notes { cursor: $anklang-cursor-eraser; }

  .-vscrollbar {
    display: flex; justify-self: center;
    width: $scrollbar-area-size; height: auto;
    margin: 0 4px 0 2px;
    overflow-y: scroll; overflow-x: hidden; background: $scrollarea-bg;
    .-vscrollbar-area { width: 1px; height: 0; }
  }
  .-hscrollbar {
    display: flex; flex-direction: column; align-self: center;
    height: $scrollbar-area-size; width: auto;
    margin: 2px 0 4px 0;
    overflow-x: scroll; overflow-y: hidden; background: $scrollarea-bg;
    .-hscrollbar-area { height: 1px; width: 3840px; }
  }
  .-overflow-hidden {
    display: flex;
    position: relative;
    white-space: nowrap;
    overflow: hidden;
  }
}

.b-piano-roll-key-width { width: $b-piano-roll-key-length; }

.b-piano-roll-contextmenu {
  .b-menuseparator { width: 75%; }
}
</style>

<template>

  <c-grid class="b-piano-roll" tabindex="0"
	  @keydown="piano_ctrl.keydown ($event)"
	  @pointerenter="pointerenter" @pointerleave="pointerleave"
	  @focus="focuschange" @blur="focuschange" >
    <!-- VTitle, COL-1 -->
    <span class="-vtitle" style="grid-row: 1/-1"  > VTitle </span>

    <!-- Buttons, COL-2 -->
    <div class="-buttons" style="grid-column: 2; grid-row: 1" >
      <b-color-picker style="flex-shrink: 1" ></b-color-picker>
      <h-flex class="-toolbutton" @click="Util.dropdown ($refs.toolmenu, $event)" >
	<b-icon class='-iconclass'
		v-bind="Util.clone_menu_icon ($refs.toolmenu, pianotool, '**EDITOR TOOL**')" />
	<b-contextmenu ref="toolmenu" keepmounted class="b-piano-roll-contextmenu" >
	  <b-menuitem mi="open_with"     uri="S" @click="usetool" kbd="1" > Rectangular Selection </b-menuitem>
	  <b-menuitem mi="multiple_stop" uri="H" @click="usetool" kbd="2" > Horizontal Selection </b-menuitem>
	  <b-menuitem fa="pencil"        uri="P" @click="usetool" kbd="3" > Pen          </b-menuitem>
	  <b-menuitem fa="eraser"        uri="E" @click="usetool" kbd="4" > Eraser       </b-menuitem>
	</b-contextmenu>
      </h-flex>
    </div>

    <!-- Piano, COL-2 -->
    <div class="-overflow-hidden -piano-wrapper" style="grid-column: 2; grid-row: 2"
	 @wheel.stop="wheel_event ($event, 'piano')" >
      <canvas class="b-piano-roll-piano tabular-nums" @click="$forceUpdate()" ref="piano_canvas" ></canvas>
    </div>

    <!-- Timeline, COL-3 -->
    <div class="-overflow-hidden -timeline-wrapper" style="grid-column: 3; grid-row: 1" ref="timeline_wrapper"
	 @wheel.stop="wheel_event ($event, 'timeline')" >
      <canvas class="-timeline tabular-nums" ref="timeline_canvas" ></canvas>
    </div>

    <!-- Roll, COL-3 -->
    <div class="-overflow-hidden -notes-wrapper" style="grid-column: 3; grid-row: 2" ref="scrollarea"
	 @contextmenu.prevent="pianorollmenu_popup ($event)"
	 @wheel.stop="wheel_event ($event, 'notes')" >
      <canvas class="b-piano-roll-notes tabular-nums" ref="notes_canvas"
	      @pointerdown="piano_roll_notes_pointerdown ($event)"
      ></canvas>
      <b-contextmenu ref="pianorollmenu" keepmounted :showicons="true"
		     class="b-piano-roll-contextmenu" mapname="Piano Roll"
		     @click="pianorollmenu_click" :check="pianorollmenu_check" >
	<b-menutitle> Piano-Roll </b-menutitle>
	<b-menuitem v-for="ac in piano_roll_actions()" :key="ac.weakid" :uri="ac.weakid" :ic="ac.ic"
		    :kbd="ac.kbd" > {{ac.label}} </b-menuitem>
	<b-menuseparator />
	<b-menuitem v-for="(script, index) in piano_roll_scripts()" :key="script.funid" :uri="script.funid"
		    ic="mi-javascript" > {{script.label}} </b-menuitem>
      </b-contextmenu>
    </div>

    <!-- VScrollborder, COL-4 -->
    <div class="-vscrollborder" style="grid-column: 4; grid-row: 1/4" ></div>

    <!-- VScrollbar, COL-5 -->
    <div class="-vscrollbar" ref="vscrollbar" style="grid-column: 5; grid-row: 2" @scroll="scrollbar_scroll ($event)" >
      <div class="-vscrollbar-area" ref="vscrollbar_area" ></div>
    </div>

    <!-- HScrollborder, ROW-3 -->
    <div class="-hscrollborder" style="grid-column: 2/4; grid-row: 3" ></div>

    <!-- HScrollbar, ROW-4 -->
    <div class="-hscrollbar" ref="hscrollbar" style="grid-column: 3; grid-row: 4" @scroll="scrollbar_scroll ($event)" >
      <div class="-hscrollbar-area" ref="hscrollbar_area" ></div>
    </div>

  </c-grid>

</template>

<script>
import * as PianoCtrl from "./piano-ctrl.js";

import * as Script from '../script.js';

const floor = Math.floor, round = Math.round;

function observable_msrc_data () {
  const data = {
    vzoom:         { default: 1.5, },
    hzoom:         { default: 2.0, },
    focus_noteid:  undefined,
    have_focus:	   { default: false, },
    srect:         { default: { x: -1, y: -1, w: 0, h: 0, sx: 0, sy: 0 } },
    end_tick:      { getter: c => this.msrc.end_tick(), notify: n => this.msrc.on ("notify:end_tick", n), },
  };
  return this.observable_from_getters (data, () => this.msrc);
}

export default {
  sfc_template,
  props: {
    msrc: { required: true },
  },
  data() {
    return { pianotool:    'S',
	     adata:        observable_msrc_data.call (this) }; },
  watch: {
    msrc (new_msrc, old_msrc) {
      // restore old and new zoom & scroll positions
      this.auto_scrollto = this.auto_scrolls[new_msrc?.$id] || this.snapshot_zoomscroll (true);
      this.adata.hzoom = this.auto_scrollto.hzoom;
      this.adata.vzoom = this.auto_scrollto.vzoom;
      // reset other state
      this.adata.focus_noteid = undefined;
      this.stepping = [ Util.PPQN, 0, 0 ];
      // re-layout, even if just this.auto_scrollto changed
      this.$forceUpdate();
    },
  },
  created () {
    this.piano_ctrl = new PianoCtrl.PianoCtrl (this);
    this.stepping = [ Util.PPQN, 0, 0 ];
    this.move_event = Util.debounce (this.piano_ctrl.move_event.bind (this.piano_ctrl), 50);
  },
  mounted () {
    // forward ctrl events
    this.drag_event = this.piano_ctrl.drag_event.bind (this.piano_ctrl);
    // setup tool state
    this.usetool (this.pianotool);
    // keep vertical scroll position for each msrc, non-reactive
    this.auto_scrolls = {};
    this.auto_scrollto = this.snapshot_zoomscroll (true);
    // observer to watch for canvas size changes
    this.resize_observer = new Util.ResizeObserver (els => {
      if (this.hscrollbar_width != this.$refs.hscrollbar.clientWidth ||
	  this.vscrollbar_height != this.$refs.vscrollbar.clientHeight)
	this.$forceUpdate();
    });
  },
  unmounted() {
    this.resize_observer.disconnect();
    this.piano_ctrl = null;
    if (this.pointer_drag)
      this.pointer_drag.destroy();
    this.pointer_drag = null;
  },
  methods: {
    pointerenter (event) {
      this.entered = true;
      if (this.$refs.toolmenu)
	this.$refs.toolmenu.map_kbd_hotkeys (this.entered || this.adata.have_focus);
    },
    pointerleave (event) {
      this.entered = false;
      if (this.$refs.toolmenu)
	this.$refs.toolmenu.map_kbd_hotkeys (this.entered || this.adata.have_focus);
    },
    focuschange (ev) {
      if (ev?.type)
	this.adata.have_focus = ev.type == "focus";
      if (this.$refs.toolmenu)
	this.$refs.toolmenu.map_kbd_hotkeys (this.entered || this.adata.have_focus);
      if (this.$refs.pianorollmenu)
	this.$refs.pianorollmenu.map_kbd_hotkeys (this.adata.have_focus);
    },
    piano_roll_notes_pointerdown (event) {
      if (document.activeElement != this.$el)
	this.$el.focus();
      const methods = {
	'S0': this.piano_ctrl.drag_select.bind (this.piano_ctrl),
      };
      if (this.pointer_drag) {
	this.pointer_drag.pointercancel (event);
	this.pointer_drag.destroy();
	this.pointer_drag = null;
      }
      if (!this.msrc)
	return false;
      const combo = this.pianotool + event.button;
      const method = methods[combo];
      if (method) {
	this.pointer_drag = new Util.PointerDrag (this, event, method);
      }
    },
    piano_roll_scripts() {
      this.scripts_ = Script.registry_keys ('PianoRoll');
      return this.scripts_;
    },
    piano_roll_actions() {
      const actions = [];
      for (const actionfunc of PianoCtrl.list_actions()) {
	if (actionfunc.label) {
	  const label = actionfunc.label;
	  const kbd = actionfunc.kbd;
	  const ic = actionfunc.ic;
	  actions.push ({ label, weakid: 'weakid:' + Util.weakid (actionfunc), kbd, ic });
	}
      }
      return actions;
    },
    pianorollmenu_popup (event) {
      this.$refs.pianorollmenu.popup (event, { checker111111: this.pianorollmenu_check.bind (this) });
    },
    pianorollmenu_check (uri, component) {
      return !!this.msrc;
    },
    pianorollmenu_click (uri, event) {
      event = Util.keyboard_click_event (event);
      event.stopPropagation();
      event.preventDefault();
      if (uri.search (/^[0-9a-z-]+@[0-9a-z-]+$/) === 0)
	return this.pianoroll_script (uri);
      if (uri.startsWith ('weakid:') && this.msrc) {
	const actionfunc = Util.weakid_lookup (Number (uri.substr (7)));
	if (actionfunc instanceof Function)
	  return actionfunc (event, this.msrc);
      }
      console.trace ("piano-roll.vue:", uri, event);
    },
    async pianoroll_script (funid) {
      const script = this.scripts_.find (entry => entry.funid == funid);
      const propvalues = {}, proplist = [];
      for (const key of Object.keys (script.params)) {
	const param = script.params[key];
	propvalues[key] = param.dflt;
	const callbacklist = [], p = {
	  hints() { return param.hints; },
	  is_bool() { return param.hints.indexOf (':bool:') >= 0; },
	  is_range() { return param.hints.indexOf (':range:') >= 0; },
	  is_choice() { return param.hints.indexOf (':choice:') >= 0; },
	  is_numeric() { return p.is_bool() || p.is_range(); },
	  identifier() { return key; },
	  label() { return param.label; },
	  nick() { return param.nick; },
	  unit() { return param.unit; },
	  group() { return ''; },
	  blurb() { return param.blurb; },
	  description() { return param.description; },
	  get_min() { return param.min; },
	  get_max() { return param.max; },
	  get_step() { return 0; },
	  reset() { p.set_value (param.dflt); },
	  set_value: (v) => {
	    propvalues[key] = v;
	    (async () => {
	      await undefined;
	      for (const c of callbacklist)
		c();
	    }) (); // async callback invocations
	  },
	  get_value () { return propvalues[key]; },
	  get_text () { return '' + p.get_value(); },
	  choices() { return param.choices; },
	  get_normalized () { return (p.get_value() - param.min) / param.max; },
	  on: (signal, callback) => {
	    const idx = callbacklist.length;
	    callbacklist.push (callback);
	    return () => { delete callbacklist[idx]; };
	  },
	};
	proplist.push (p);
      }
      let v = 1;
      if (proplist.length > 0) {
	v = App.async_modal_dialog ({
	  title: "Run Script: " + script.label,
	  text: script.blurb,
	  buttons: [ "Cancel", { label: "OK", autofocus: true }, ],
	  proplist, emblem: 'PIANO',
	});
	v = await v;
      }
      if (v === 1) {
	const result = await Script.run_script_fun (funid, propvalues);
	if (result)
	  console.log ("piano-roll.vue: result from " + funid + ':', result);
      }
    },
    usetool (uri) {
      this.pianotool = uri;
      this.$el?.setAttribute ('data-pianotool', this.pianotool);
    },
    adjust_zoom (which, dir)
    {
      if (which == 'timeline' || which == 'notes')
	this.adata.hzoom = Util.clamp (this.adata.hzoom * (dir < 0 ? 1.01 : 0.99), 0.05, 20);
      else if (which == 'piano')
	this.adata.vzoom = Util.clamp (this.adata.vzoom * (dir < 0 ? 1.03 : 0.97), 1, 5);
    },
    wheel_event (event, which)
    {
      if (event.ctrlKey) // use Ctrl+Scroll for zoom; ignore Shift, needed by Firefox
	{
	  const delta = Util.wheel_delta (event);
	  const dir = Math.sign (delta.y || delta.x);
	  this.adjust_zoom (which, dir);
	  return;
	}
      if (which == 'timeline')
	Util.wheel2scrollbars (event, this.$refs, 'hscrollbar');
      else if (which == 'piano')
	Util.wheel2scrollbars (event, this.$refs, 'vscrollbar');
      else if (which == 'notes')
	Util.wheel2scrollbars (event, this.$refs, 'hscrollbar', 'vscrollbar');
    },
    dom_update() {
      // DOM, $el and $refs are in place now
      this.layout = undefined;
      this.resize_observer.disconnect();
      this.resize_observer.observe (this.$refs.hscrollbar);
      this.hscrollbar_width = this.$refs.hscrollbar.clientWidth;
      this.resize_observer.observe (this.$refs.vscrollbar);
      this.vscrollbar_height = this.$refs.vscrollbar.clientHeight;
      if (!Util.is_displayed (this.$el))
	return; // laoyut calculation needs DOM element sizes (only assigned when visible)
      // canvas setup
      if (piano_layout.call (this))
	return this.$forceUpdate();	// laoyut components resized
      this.dom_queue_draw();
      // store new zoom & scroll positions
      if (this.msrc && this.msrc.$id && this.auto_scrolls)
	this.auto_scrolls[this.msrc.$id] = this.snapshot_zoomscroll();
      this.piano_ctrl.dom_update();
    },
    scrollbar_scroll (event) {
      this.dom_queue_draw();
      // store new zoom & scroll positions
      if (this.msrc && this.msrc.$id && this.auto_scrolls)
	this.auto_scrolls[this.msrc.$id] = this.snapshot_zoomscroll();
      // adjust selection, etc
      if (this.pointer_drag)
	this.pointer_drag.scroll (event);
    },
    dom_draw() {
      if (this.layout)
	{
	  render_timeline.call (this);
	  render_piano.call (this);
	  render_notes.call (this);
	}
    },
    snapshot_zoomscroll (defaults = false) {
      if (defaults && this.snapshot_zoomscroll.defaults)
	return this.snapshot_zoomscroll.defaults;
      const vscrollpos = !this.vscrollbar_height || defaults ? 0.5 :
			 this.$refs.vscrollbar.scrollTop / (this.vscrollbar_height * vscrollbar_proportion);
      const hscrollpos = !this.hscrollbar_width || defaults ? 0 :
			 this.$refs.hscrollbar.scrollLeft / (this.hscrollbar_width * hscrollbar_proportion);
      const zs = { hzoom: this.adata.hzoom, vzoom: this.adata.vzoom, vscrollpos, hscrollpos };
      if (defaults)
	this.snapshot_zoomscroll.defaults = zs;
      return zs;
    },
  },
};

const hscrollbar_proportion = 20, vscrollbar_proportion = 11;

function piano_layout () {
  const piano_canvas = this.$refs.piano_canvas, piano_style = getComputedStyle (piano_canvas);
  const notes_canvas = this.$refs.notes_canvas, timeline_canvas = this.$refs.timeline_canvas;
  const notes_cssheight = Math.floor (this.adata.vzoom * 84 / 12) * PianoCtrl.PIANO_KEYS;
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
  const key_length = parseFloat (piano_style.getPropertyValue ('--piano-roll-key-length'));
  const min_end_tick = 16 * (4 * Util.PPQN);
  const end_tick = Math.max (this.adata.end_tick || 0, min_end_tick);
  // scale layout
  layout.dpr_height = round (layout.DPR * layout.cssheight);
  layout.white_width = key_length || layout.white_width; // allow CSS override
  layout.piano_csswidth = layout.white_width;
  layout.notes_csswidth = this.hscrollbar_width;
  layout.beat_pixels = round (layout.beat_pixels * DPR * this.adata.hzoom);
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
  Util.resize_canvas (piano_canvas, layout.piano_csswidth, this.vscrollbar_height); // layout.cssheight
  // resize timeline
  Util.resize_canvas (timeline_canvas, layout.notes_csswidth, this.$refs.timeline_wrapper.clientHeight);
  // resize notes
  Util.resize_canvas (notes_canvas, layout.notes_csswidth, this.vscrollbar_height); // layout.cssheight
  // vscrollbar setup
  let px, layout_changed = false;
  px = (this.vscrollbar_height * (vscrollbar_proportion + 1)) + 'px';
  if (this.$refs.vscrollbar_area.style.height != px)
    {
      layout_changed = true;
      this.$refs.vscrollbar_area.style.height = px;
    }
  layout.yoffset = () => {
    const yscroll = this.$refs.vscrollbar.scrollTop / (this.vscrollbar_height * vscrollbar_proportion);
    let yoffset = layout.dpr_height - yscroll * (layout.dpr_height - this.vscrollbar_height * DPR);
    yoffset -= 2 * layout.thickness; // leave room for overlapping piano key borders
    return yoffset;
  };
  layout.yscroll = () => layout.yoffset() / layout.DPR;
  // hscrollbar setup
  px = (this.hscrollbar_width * (hscrollbar_proportion + 1)) + 'px';
  if (this.$refs.hscrollbar_area.style.width != px)
    {
      layout_changed = true;
      this.$refs.hscrollbar_area.style.width = px;
    }
  layout.xposition = () => {
    const xscroll = this.$refs.hscrollbar.scrollLeft / (this.hscrollbar_width * hscrollbar_proportion);
    return xscroll * layout.virt_width - layout.hpad;
  };
  layout.xscroll = () => layout.xposition() / layout.DPR;
  // restore scroll & zoom
  if (this.auto_scrollto)
    {
      this.$refs.vscrollbar.scrollTop = this.auto_scrollto.vscrollpos * (this.vscrollbar_height * vscrollbar_proportion);
      this.$refs.hscrollbar.scrollLeft = this.auto_scrollto.hscrollpos * (this.hscrollbar_width * hscrollbar_proportion);
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

function canvas_font (size) {
  const canvas = this.$refs.piano_canvas, cstyle = getComputedStyle (canvas);
  const fontstring = cstyle.getPropertyValue ('--piano-roll-font');
  const parts = fontstring.split (/\s*\d+px\s*/i); // 'bold 10px sans' -> ['bold', 'sans']
  console.assert (parts && parts.length >= 1);
  const font = parts[0] + ' ' + size + ' ' + (parts[1] || '');
  return font;
}

function render_piano()
{
  const canvas = this.$refs.piano_canvas, cstyle = getComputedStyle (canvas);
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

function render_notes()
{
  const canvas = this.$refs.notes_canvas, cstyle = getComputedStyle (canvas);
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
  render_timegrid.call (this, canvas, false);

  // draw octave separators
  const semitone12 = csp ('--piano-roll-semitone12');
  ctx.fillStyle = semitone12;
  for (let oct = 0; oct <= layout.octaves; oct++) {	// condiiton +1 to include top border
    const oy = yoffset - oct * layout.oct_length;
    ctx.fillRect (0, oy, canvas.width, th);
  }

  // paint selection rectangle
  const srect = { x: layout.DPR * this.adata.srect.x, y: layout.DPR * this.adata.srect.y,
		  w: layout.DPR * this.adata.srect.w, h: layout.DPR * this.adata.srect.h };
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
  const tickscale = layout.tickscale;
  const note_color = csp ('--piano-roll-note-color');
  const note_selected_color = csp ('--piano-roll-note-focus-color');
  const note_focus_color = csp ('--piano-roll-note-focus-color');
  const focus_noteid = this.adata.have_focus ? this.adata.focus_noteid : -1;
  ctx.lineWidth = layout.DPR; // layout.thickness;
  ctx.fillStyle = note_color;
  ctx.strokeStyle = csp ('--piano-roll-note-focus-border');
  // draw notes
  for (const note of Shell.get_note_cache (this.msrc).notes)
    {
      const oct = floor (note.key / 12), key = note.key - oct * 12;
      const ny = yoffset - oct * layout.oct_length - key * layout.row + 1;
      const nx = round (note.tick * tickscale), nw = Math.max (1, round (note.duration * tickscale));
      if (note.selected)
	ctx.fillStyle = note_selected_color;
      else
	ctx.fillStyle = note_color;
      ctx.fillRect (nx - lsx, ny - layout.row, nw, layout.row - 2);
      if (note.id == focus_noteid)
	{
	  ctx.fillStyle = note_focus_color;
	  ctx.strokeRect (nx - lsx, ny - layout.row, nw, layout.row - 2);
	}
    }
}

function render_timeline()
{
  const canvas = this.$refs.timeline_canvas, cstyle = getComputedStyle (canvas);
  const ctx = canvas.getContext ('2d'), csp = cstyle.getPropertyValue.bind (cstyle);
  const layout = this.layout, light_row = csp ('--piano-roll-light-row');
  // paint bg with white key row color
  ctx.fillStyle = light_row;
  ctx.fillRect (0, 0, layout.notes_csswidth * layout.DPR, canvas.height);

  render_timegrid.call (this, canvas, true);
}

function render_timegrid (canvas, with_labels)
{
  const signature = [ 4, 4 ]; // 15, 16
  const cstyle = getComputedStyle (canvas), gy1 = 0;
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
</script>
