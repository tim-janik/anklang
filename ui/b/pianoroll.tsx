// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

/** ## Piano-Roll
 * The piano-roll editor displays notes in a grid where the vertical axis denotes the pitch and the horizontal axis the time line.
 * It is modeled after a [classic piano-roll](https://en.wikipedia.org/wiki/Piano_roll#In_digital_audio_workstations).
 * Several tools can be selected via a drop down menu or hotkeys which aid with creation, selection and modification of musical events.
 * The cursor keys can be used to move selected notes, in combination with modifier keys the keys can also change note duration or note focus.
 * A context menu is available with mouse button 3, which provides extended functionality.
 */

import { createEffect, onMount, onCleanup } from 'solid-js';
import * as PianoCtrl from "./piano-ctrl.js";
import * as Util from '../util.js';
import { clamp } from '../util.js';
import { text_content } from '../dom.js';
import * as Mouse from '../mouse.js';
import { tracking_wrapper } from "../signal.js";
import { MenuTitle } from './menutitle.tsx';
const floor = Math.floor, round = Math.round;

// == STYLE ==
Extra_css`
b-piano-roll, .b-piano-roll {
  display: flex; flex-direction: column; align-items: stretch;
  position: relative;
  /* Make scss variables available to JS via getComputedStyle() */
  --b-piano-roll-key-length: 64px;
  --piano-roll-light-row:    var(--b-piano-roll-light-row);
  --piano-roll-dark-row:     var(--b-piano-roll-dark-row);
  --piano-roll-grid-main:    oklch(from var(--b-piano-roll-light-row) calc(l + 0.225) c h);   /* bar separator */
  --piano-roll-grid-sub:     oklch(from var(--b-piano-roll-light-row) calc(l + 0.135) c h);   /* quarter note separator */
  --piano-roll-semitone12:   oklch(from var(--b-piano-roll-light-row) calc(l + 0.225) c h);   /* separator per octave */
  --piano-roll-semitone6:    oklch(from var(--b-piano-roll-light-row) calc(l + 0.225) c h);   /* separator after 6 semitones */

  --piano-roll-white-base:   var(--b-piano-roll-white-base);
  --piano-roll-white-border: var(--b-scrollboundary-color);                   /* border around piano key */
  --piano-roll-white-glint:  oklch(from var(--b-piano-roll-white-base) calc(l + 0.065) c h);   /* highlight on piano key */
  --piano-roll-key-color:    var(--b-scrollboundary-color);
  --piano-roll-black-base:   var(--b-piano-roll-black-base);
  --piano-roll-black-border: oklch(from var(--b-piano-roll-black-base) calc(l + 0.038) c h);   /* border around piano key */
  --piano-roll-black-glint:  oklch(from var(--b-piano-roll-black-base) calc(l + 0.143) c h);  /* highlight on piano key */
  --piano-roll-black-shine:  oklch(from var(--b-piano-roll-black-base) calc(l + 0.335) c h);  /* reflection on piano key */

  --piano-roll-font:                  var(--b-canvas-font);
  --piano-roll-num-color:             var(--b-piano-roll-num-color);
  --piano-roll-note-color:            var(--b-piano-roll-note-color);
  --piano-roll-note-focus-color:      var(--b-piano-roll-note-focus-color);
  --piano-roll-note-focus-border:     var(--b-piano-roll-note-focus-border);
  --piano-roll-key-length:            var(--b-piano-roll-key-length);
}
.b-pianoroll-grid {
  display: grid;
  background: var(--b-piano-roll-black-base);
  position: absolute; inset: 0;
  align-items: stretch;
  grid-template-columns: min-content 1fr min-content;
  grid-template-rows:    min-content 1fr min-content;
  canvas { background: black; object-fit: contain;
    min-width: 0; min-height: 0; /* https://www.w3.org/TR/css3-grid-layout/#min-size-auto */
  }
  .-indicator {
    position: absolute; top: 0; bottom: 0; left: 0; width: 1px; height: 100%;
    background: var(--b-piano-roll-indicator);
    z-index: 2; backface-visibility: hidden; will-change: transform;
    transform: translateX(-9999px);
    pointer-events: none;
  }
  .-hextend {
    background: #0000; opacity: 0; visibility: hidden;
    margin-top: 0; height: 1px;
    /* height: 16px; margin-top: -8px; background: #0f0; */
  }
  .-vextend {
    background: #0000; opacity: 0; visibility: hidden;
    margin-left: 0; width: 1px;
    /* width: 16px; margin-left: -3px; background: #00f; */
  }
}`;

// == JSX TEMPLATE ==
const render_piano_roll = (t: any, actions: any[], props: { class?: string; hidden?: boolean; style?: any; ref?: (el: HTMLElement) => void }) => (
  <div ref={el => { t.root = el; props.ref?.(el); }}
       class={['b-piano-roll', props.class].filter(Boolean).join(' ')}
       hidden={props.hidden}
       style={props.style}>

    <div class="b-pianoroll-grid" tabindex="-1" ref={h => t.cgrid = h} data-f1="the-piano-roll.html"
         onPointerEnter={t.pointerenter}
         onPointerLeave={t.pointerleave}
         onFocus={t.focuschange}
         onBlur={t.focuschange}
         onKeyDown={e => t.piano_ctrl.keydown (e)}>

      <div class="vflex -toolbutton col-start-1 row-start-1" style="height: 1.7em; align-items: end; padding-right: 4px;"
           id="g-pianoroll-toolbutton"
           ref={h => t.menu_btn = h}
           onClick={e => t.pianotoolmenu.popup (e)}
           onMouseDown={e => t.pianotoolmenu.popup (e)}>
        <b-icon style="width: 1.2em; height: 1.2em" ref={h => t.menu_icon = h}/>
        <b-contextmenu ref={h => { t.pianotoolmenu = h; }}
                       activate={t.usetool}
                       id="g-pianotoolmenu" class="-pianotoolmenu">
          <button ic="md-open_with"     uri="S" kbd="1" > Rectangular Selection  </button>
          <button ic="md-multiple_stop" uri="H" kbd="2" > Horizontal Selection   </button>
          <button ic="fa-pencil"        uri="P" kbd="3" > Pen                    </button>
          <button ic="fa-eraser"        uri="E" kbd="4" > Eraser                 </button>
        </b-contextmenu>
      </div>

      <canvas class="-time_canvas col-start-2 row-start-1" ref={h => t.time_canvas = h}></canvas>
      <canvas class="-piano_canvas col-start-1 row-start-2" ref={h => t.piano_canvas = h}></canvas>
      <canvas class="-notes_canvas col-start-2 row-start-2" ref={h => t.notes_canvas = h}
              onPointerMove={Util.debounce (t.notes_canvas_pointermove)}
              onPointerDown={t.notes_canvas_pointerdown}></canvas>

      <div class="col-start-3 row-start-2" style="overflow: hidden scroll; min-width: 17px; background: #000" ref={h => t.vscrollbar = h}>
        <div class="-vextend" style="height: 151vh" ref={h => t.vscrollbar_extend = h}>
        </div>
      </div>
      <div class="col-start-2 row-start-3" ref={h => t.hscrollbar = h} style="overflow: scroll hidden; min-height: 17px; background: #000">
        <div class="-hextend" ref={h => t.hscrollbar_extend = h} style="width:999px"></div>
      </div>
      <span class="-indicator" ref={h => t.indicator_bar = h}></span>

      <b-contextmenu ref={h => { t.pianorollmenu = h; }}
                     activate={t.pianorollmenu_click}
                     isactive={t.pianorollmenu_check}
                     id="g-pianorollmenu" showicons={true}
                     class="-pianorollmenu" mapname="Piano Roll">
        <MenuTitle> Piano-Roll </MenuTitle>
        {/* key=${ac.weakid} */}
        {actions.map (ac => (
          <button uri={ac.weakid} ic={ac.ic} kbd={ac.kbd}>{ac.label}</button>
        ))}
      </b-contextmenu>
    </div>
  </div>
);

// == SCRIPT ==
const default_note_length = Util.PPQN / 4;

/** The PianoRoll component allows note editing. */
export function PianoRoll (props: {
  clip?: any;  ///< The clip with notes to be edited.
  hidden?: boolean;
  ref?: (el: HTMLElement) => void;
  class?: string;
  style?: any;
})
{
  // Internal state container — also used as `piano_roll` by piano-ctrl.js
  const t: any = {};

  // DOM element refs — set by JSX ref callbacks; t.root is the outer .b-piano-roll div
  t.root = null;
  t.cgrid = null;
  t.menu_btn = null;
  t.menu_icon = null;
  t.pianotoolmenu = null;
  t.pianorollmenu = null;
  t.notes_canvas = null;
  t.piano_canvas = null;
  t.time_canvas = null;
  t.hscrollbar = null;
  t.hscrollbar_extend = null;
  t.vscrollbar = null;
  t.vscrollbar_extend = null;
  t.indicator_bar = null;

  // State
  t.layout = null;
  t.hzoom = 3;
  t.vzoom = 1.5;
  t.pianotool = 'P';
  t.last_note_length = default_note_length;
  t.have_focus = false;
  t.entered = false;
  t.last_pos = -9.987;
  t.srect_ = { x: 0, y: 0, w: 0, h: 0, sx: 0, sy: 0 };
  t.clip = null;
  t.end_click = 99999;
  t.auto_scrollto = undefined; // positions to restore scroll & zoom
  t.stepping = []; // current grid stepping granularity
  t.vscroll_must_center = true; // flag for initial vertical centering
  t.pointer_drag = null;
  t.piano_ctrl = new PianoCtrl.PianoCtrl (t);
  t.drag_event = t.piano_ctrl.drag_event.bind (t.piano_ctrl);
  t.notes_canvas_pointermove_zmovedel = null;

  // track repaint dependencies
  // queue_repaint — microtask-batched repaint scheduling
  {
    let update_queued = false;
    t.queue_repaint = () => {
      if (update_queued) return;
      update_queued = true;
      queueMicrotask (() => {
        update_queued = false;
        t.repaint_tracked();
      });
    };
  }

  // Repaint implementation
  const repaint_impl = () => {
    if (!t.clip || !t.notes_canvas || !t.hscrollbar || !t.vscrollbar)
      return;
    paint_notes.call (t);
    paint_timeline.call (t);
    paint_piano.call (t);
  };
  t.repaint_tracked = tracking_wrapper (t.queue_repaint, repaint_impl);

  // Lifecycle: onMount — after first render
  onMount (() => {
    if (!t.root) return;
    // initial menu button update
    t.usetool (t.pianotool);

    // setup wheel event (non-passive for preventDefault) ─ the old Lit @wheel was on cgrid,
    // but attaching to the root div is equivalent (cgrid is its sole child, events bubble)
    (t.root as HTMLElement).addEventListener ('wheel', t.wheel_event, { passive: false });
    onCleanup (() => (t.root as HTMLElement).removeEventListener ('wheel', t.wheel_event));

    // setup scroll handlers
    if (t.hscrollbar)
      t.hscrollbar.onscroll = e => t.hvscroll (e);
    if (t.vscrollbar)
      t.vscrollbar.onscroll = e => t.hvscroll (e);

    // setup resize observer — actually trigger layout + repaint on resize
    // (the previous Lit version's callback was `els => this.requestUpdate.bind (this)`,
    //  which never *called* requestUpdate, so resizes were silently ignored)
    const resize_observer = new ResizeObserver (() => {
      if (t.clip)
        piano_layout.call (t);
      t.queue_repaint();
    });
    resize_observer.observe (t.root);
    resize_observer.observe (document.body);

    onCleanup (() => {
      resize_observer.disconnect();
      Shell.piano_current (null, null);
      if (t.notes_canvas_pointermove_zmovedel)
      {
        t.notes_canvas_pointermove_zmovedel();
        t.notes_canvas_pointermove_zmovedel = null;
      }
    });

    // initial render: scroll handlers, tool, wheel listener — all set up above.
    // The createEffect below runs before onMount and already performs the initial
    // layout + repaint, so nothing else to do here.
  });

  // Re-runs only when the clip *identity* changes (different clip object assigned):
  // resets scroll position, note-length memory, indicator, and (re)registers the
  // play-position callback with the Shell. These must NOT run on mere note edits,
  // otherwise every committed modification yanks the scroll back to the origin.
  createEffect (() => {
    const clip = props.clip;
    if (!t.root) return;
    t.clip = clip;
    if (clip)
    {
      t.hscrollbar?.scrollTo ({ left: 0, behavior: 'instant' });
      t.vscroll_to (0.5);
      t.last_note_length = default_note_length;
      if (t.indicator_bar)
        t.indicator_bar.style.transform = "translateX(-9999px)";
    }

    // indicator_bar setup
    Shell.piano_current (clip, clip ? t.piano_current_tick : null);
  });

  // Re-runs on clip property/note changes (clip.all_notes / clip.end_tick are Solid
  // signals via $get): re-layout (depends on clip.end_tick) and schedule a repaint.
  // Mirrors the old tracked `updated()` minus the reset block (now above).
  createEffect (() => {
    const clip = props.clip;
    if (!t.root || !clip) return;
    // read out props to force auto-updates / subscribe to note & length signals
    clip.all_notes;
    // piano_layout reads clip.end_tick, subscribing this effect to clip resize
    piano_layout.call (t);
    if (t.vscroll_must_center && t.vscrollbar?.clientHeight)
      t.vscroll_must_center = (t.vscroll_to (0.5), false);
    t.queue_repaint();
  });

  // === Methods bound to the t object ===

  t.piano_current_tick = (current_clip: any, current_tick: number) => {
    if (t.clip != current_clip) return;
    const offst = t.layout.xscroll();
    const tpos = -offst + current_tick * t.layout.tickscale * t.layout.DPRDIV;
    let u = Math.round (tpos * t.layout.DPR) * t.layout.DPRDIV; // screen pixel alignment
    u = u >= 0 ? u + t.notes_canvas.offsetLeft : -9999;
    if (u != t.last_pos)
    {
      const indicator_transform = "translateX(" + u + "px)";
      t.last_pos = u;
      if (t.indicator_bar)
        t.indicator_bar.style.transform = indicator_transform;
    }
  };

  t.vscroll_to = (fraction: number) => {
    const vrange = t.vscrollbar_extend.clientHeight - t.vscrollbar.clientHeight;
    t.vscrollbar.scrollTo ({ top: fraction * vrange, behavior: 'instant' });
  };

  t.hvscroll = (event: Event) => {
    // scrollbar(s) changed
    t.queue_repaint();
    // adjust selection, etc
    if (t.pointer_drag)
      t.pointer_drag.scroll (event);
  };

  Object.defineProperty (t, 'srect', {
    get: () => Object.assign ({}, t.srect_),
    set: (r: any) => { Object.assign (t.srect_, r); t.queue_repaint(); },
  });

  t.pointerenter = (event: PointerEvent) => {
    t.entered = true;
    if (t.pianotoolmenu)
      t.pianotoolmenu.map_kbd_hotkeys (t.entered || t.have_focus);
  };

  t.pointerleave = (event: PointerEvent) => {
    t.entered = false;
    if (t.pianotoolmenu)
      t.pianotoolmenu.map_kbd_hotkeys (t.entered || t.have_focus);
  };

  t.focuschange = (ev: FocusEvent) => {
    if (ev?.type)
      t.have_focus = ev.type == "focus";
    if (t.pianotoolmenu)
      t.pianotoolmenu.map_kbd_hotkeys (t.entered || t.have_focus);
    if (t.pianorollmenu)
      t.pianorollmenu.map_kbd_hotkeys (t.have_focus);
  };

  t.notes_canvas_pointermove = (event: PointerEvent) => {
    if (t.pointer_drag)
      return;
    if (t.notes_canvas_pointermove_zmovedel)
    {
      t.notes_canvas_pointermove_zmovedel();
      t.notes_canvas_pointermove_zmovedel = null;
    }
    t.notes_canvas_tool = PianoCtrl.notes_canvas_tool_from_hover (t, event);
    if (t.notes_canvas_tool.cursor != t.notes_canvas.style.cursor)
      t.notes_canvas.style.cursor = t.notes_canvas_tool.cursor;
  };

  t.notes_canvas_pointerdown = (event: PointerEvent) => {
    if (t.pointer_drag)
    {
      t.pointer_drag.destroy();
      return;
    }
    if (!t.clip)
      return;
    if (document.activeElement != t.cgrid)
      t.cgrid.focus();
    if (event.button == 2)
    {
      Util.prevent_event (event);
      t.pianorollmenu.popup (event, { origin: 'none' });
      return;
    }
    if (event.button == 0 && t.notes_canvas_tool)
    {
      const ctool_this = t.notes_canvas_tool.drag_start (t);
      t.pointer_drag = new Util.PointerDrag (t, event,
                                              (event_arg: any, MODE: any) => ctool_this.drag_event (event_arg, MODE),
                                              () => t.pointer_drag = null);
      return;
    }
    debug ('b-pianoroll: pointerdown without tool', event);
  };

  t.pianorollmenu_actions = () => {
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
  };

  t.pianorollmenu_check = (uri: string) => {
    return !!t.clip;
  };

  t.pianorollmenu_click = (uri: string, event?: Event) => {
    event = Util.keyboard_click_event (event);
    if (event)
    {
      event.stopPropagation();
      event.preventDefault();
    }
    if (uri.search (/^[0-9a-z-]+@[0-9a-z-]+$/) === 0)
      return; // TODO: this.pianoroll_script (uri);
    if (uri.startsWith ('weakid:') && t.clip) {
      const action = Util.weakid_lookup (Number (uri.substr (7)));
      if (action.func instanceof Function)
        return action.func (action, t, t.clip, event);
    }
  };

  t.usetool = (uri: string) => {
    t.pianotool = uri;
    // clone menu item
    const title = '**EDITOR TOOL**';
    const menuitem = t.pianotoolmenu.find_menuitem (uri);
    t.menu_icon.setAttribute ('ic', menuitem.getAttribute ('ic'));
    t.menu_icon.setAttribute ('data-kbd', menuitem.getAttribute ('kbd'));
    t.menu_icon.setAttribute ('data-tip', title + ' ' + text_content (menuitem, false).trim());
    // pick up 'data-tip' and pick cursor via hover
    if (!t.notes_canvas_pointermove_zmovedel)
      t.notes_canvas_pointermove_zmovedel = App.zmoves_add (t.notes_canvas_pointermove);
    App.zmove(); // trigger move / hover
  };

  t.wheel_event = (event: WheelEvent) => {
    const delta = Mouse.wheel_delta (event, true);
    if (event.ctrlKey) {
      if (delta.deltaX)
        t.hzoom = clamp (t.hzoom * (delta.deltaX > 0 ? 1.1 : 0.9), 0.25, 25);
      if (delta.deltaY)
        t.vzoom = clamp (t.vzoom * (delta.deltaY > 0 ? 1.1 : 0.9), 0.5, 25);
      if (t.clip)
        piano_layout.call (t);
      t.queue_repaint();
    } else {
      if (delta.deltaX)
        t.hscrollbar.scrollBy ({ left: delta.deltaX });
      if (delta.deltaY)
        t.vscrollbar.scrollBy ({ top: delta.deltaY });
    }
    Util.prevent_event (event);
  };

  const pianorollmenu_actions = t.pianorollmenu_actions();

  return render_piano_roll (t, pianorollmenu_actions, props);
}

// == Helper functions ==

const hscrollbar_proportion = 20, vscrollbar_proportion = 11;

/** Determine layout in pixels.
 * @this{any}
 */
function piano_layout()
{
  const notes_canvas = this.notes_canvas, timeline_canvas = this.time_canvas;
  const piano_canvas = this.piano_canvas, cstyle = getComputedStyle (this.root);
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
  const end_tick = Math.max (this.clip.end_tick || 0, min_end_tick);
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
  let layout_changed = false;
  if (0) // vscrollbar resizing unused, vscrollbar_extend is fixed atm
    {
      const px = (vscrollbar_height * (vscrollbar_proportion + 1)) + 'px';
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
  const px = (hscrollbar_width * (hscrollbar_proportion + 1)) + 'px';
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
 * @this{any}
 */
function set_canvas_font (ctx: any, size: string)
{
  const cstyle = getComputedStyle (this.root);
  const fontstring = cstyle.getPropertyValue ('--piano-roll-font');
  const parts = fontstring.split (/\s*\d+px\s*/i); // 'bold 10px sans' -> ['bold', 'sans']
  console.assert (parts && parts.length >= 1);
  const font = parts[0] + ' ' + size + ' ' + (parts[1] || '');
  try {
    ctx.font = font;
  } catch (err) {
    // FireFox throws a "NS_ERROR_FAILURE" exception on first paint
    console.error ("set_canvas_font: ctx.font assignment error" /*, err */ );
    return;
  }
  return font;
}

/** Paint piano key canvas
 * @this{any}
 */
function paint_piano()
{
  const canvas = this.piano_canvas, cstyle = getComputedStyle (this.root);
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
      ctx.fillRect (x, y - h + th, w - 2 * th, h - th); // v-overlap by 1*th
      const sx = x + hf, sy = y - h + hf; // stroke coords
      ctx.strokeStyle = white_glint; // highlight
      ctx.strokeRect (sx, sy + th, w - th, h - th);
      ctx.strokeStyle = white_border; // border
      ctx.strokeRect (sx - th, sy, w, h); // v-overlap by 1*th
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
      ctx.fillRect   (x + th, y - h + th, w - 2 * th, h - th); // v-overlap by 1*th
      const sx = x + hf, sy = y - h + hf; // stroke coords
      ctx.strokeStyle = black_glint; // highlight
      ctx.strokeRect (sx, sy + th, w - 2 * th, h - th);
      ctx.strokeStyle = black_border; // border
      ctx.strokeRect (sx, sy, w - th, h);
    }
    // outer border
    ctx.fillStyle = white_border;
    ctx.fillRect (0, 0, DPR, canvas.height);
  }

  // figure font size for piano key labels
  const avg_height = layout.wkeys.reduce ((a, p) => a += p[1], 0) / layout.wkeys.length;
  let fpx = avg_height - 2 * (th + 1); // base font size on average white key size
  fpx = Util.clamp (fpx / layout.DPR, 7, 12) * layout.DPR;
  if (fpx >= 6) {
    ctx.fillStyle = csp ('--piano-roll-key-color');
    if (!set_canvas_font.call (this, ctx, fpx + 'px'))
      return;
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
 * @this{any}
 */
function paint_notes()
{
  const canvas = this.notes_canvas, cstyle = getComputedStyle (this.root);
  const ctx = canvas.getContext ('2d'), csp = cstyle.getPropertyValue.bind (cstyle);
  const layout = this.layout, yoffset = layout.yoffset();
  const light_row = cstyle.getPropertyValue ('--piano-roll-light-row');
  // paint bg with white key row color
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
  ctx.lineWidth = th; // line thickness
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
  for (let oct = 0; oct <= layout.octaves; oct++) { // condiiton +1 to include top border
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
  const draw_notes = (selected: boolean) => {
    for (const note of this.clip.all_notes)
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
 * @this{any}
 */
function paint_timeline()
{
  const canvas = this.time_canvas, cstyle = getComputedStyle (this.root);
  const ctx = canvas.getContext ('2d'), csp = cstyle.getPropertyValue.bind (cstyle);
  const layout = this.layout, light_row = csp ('--piano-roll-light-row');
  // paint bg with white key row color
  ctx.fillStyle = light_row;
  ctx.fillRect (0, 0, layout.notes_csswidth * layout.DPR, canvas.height);

  paint_timegrid.call (this, canvas, true);
}

/** Paint timegrid into any canvas
 * @this{any}
 */
function paint_timegrid (canvas: any, with_labels: boolean)
{
  const signature = [ 4, 4 ]; // 15, 16
  const cstyle = getComputedStyle (this.root), gy1 = 0;
  const gy2 = canvas.height * (with_labels ? 0.5 : 0), gy3 = canvas.height * (with_labels ? 0.75 : 0);
  const ctx = canvas.getContext ('2d'), csp = cstyle.getPropertyValue.bind (cstyle);
  const layout = this.layout, lsx = layout.xposition(), th = layout.thickness;
  const grid_main = csp ('--piano-roll-grid-main'), grid_sub = csp ('--piano-roll-grid-sub');
  const TPN64 = Util.PPQN / 16; // Ticks per 64th note
  const TPD = TPN64 * 64 / signature[1]; // Ticks per denominator fraction
  const bar_ticks = signature[0] * TPD; // Ticks per bar
  const bar_pixels = bar_ticks * layout.tickscale;
  const denominator_pixels = bar_pixels / signature[0];
  const barjumps = 8;
  // line thickness and line cap
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
