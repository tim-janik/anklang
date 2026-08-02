// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class BKnob
 * @description
 * The <b-knob> element provides a knob for scalar inputs.
 * It supports the Vue
 * [v-model](https://vuejs.org/v2/guide/components-custom-events.html#Customizing-Component-v-model)
 * protocol by emitting an `input` event on value changes and accepting inputs via the `value` prop.
 * ### Props:
 * *bidir*
 * : Boolean, flag indicating bidirectional inputs with value range `-1…+1`.
 * *value*
 * : Float, the knob value to be displayed, the value range is `0…+1` if *bidir* is `false`.
 * *format*
 * : String, format specification for popup bubbles, containinig a number for the peak amplitude.
 * *label*
 * : String, text string for popup bubbles.
 * *hscroll*
 * : Boolean, adjust value with horizontal scrolling (without dragging).
 * *vscroll*
 * : Boolean, adjust value with vertical scrolling (without dragging).
 * *width4height*
 * : Automatically determine width from externally specified height (default), otherwise determines height.
 * ### Implementation Notes
 * The knob is rendered based on an SVG drawing, which is arranged in such a
 * way that adding rotational transforms to the SVG elements is sufficient to
 * display varying knob levels.
 * Chrome cannot render individual SVG nodes into seperate layers (GPU textures)
 * so utilizing GPU acceleration requires splitting the original SVG into
 * several SVG layers, each of which can be utilized as a seperate GPU texture with
 * the CSS setting `will-change: transform`.
 */

import { createEffect, mergeProps, onCleanup } from 'solid-js';
import * as Util from "../util.js";
import * as Mouse from '../mouse.js';

// == STYLE ==
Extra_css`
b-knob, .b-knob {
  display: flex; position: relative;
  margin: 0; padding: 0; text-align: center;
  &[aria-disabled="true"] { opacity: 0.5; cursor: not-allowed; pointer-events: none; }
  &.b-knob-h4w svg { position: absolute; width:  100%; } /* height for width */
  &.b-knob-w4h svg { position: absolute; height: 100%; } /* width for height */
  .b-knob-trf {
    will-change: transform; /* request GPU texture for fast transforms */
  }
  svg.b-knob-sizer {
    /* empty SVG element, used by .b-knob to determine width from height from viewBox */
    position: relative; /* participate in layout space allocation */
  }
  #sprite {
    display: inline-block;
    background-image: url("/gen/assets/cknob193u.png");
    &[bidir] { background-image: url("/gen/assets/cknob193b.png"); }
    background-repeat: no-repeat;
    --b-knob-size: min(var(--b-prop-width), var(--b-prop-height));
    background-size: calc(1.0 * var(--b-knob-size));
    will-change: background-position;
    width:  calc(1.0 * var(--b-knob-size));
    height: calc(1.0 * var(--b-knob-size));
  }
}`;

// == Symbols & Constants ==
const SPIN_DRAG = Symbol ('SpinDrag');
const USE_PTRLOCK = true;

// == Helpers ==
/// Calculate spin drag acceleration (slowdown) from event type and modifiers.
function spin_drag_granularity (event)
{
  if (event.type == 'wheel') {
    let gran = 0.025;                   // approximate wheel delta "step" to percent
    if (event.shiftKey)
      gran = 0.005;                     // slow down
    else if (event.ctrlKey)
      gran = 0.10;                      // speed up
    return gran;
  }
  // pixel drag
  const radius = 64;                    // assumed knob size
  const circum = 2 * radius * Math.PI;
  let gran = 1 / circum;                // steps per full turn to feel natural
  if (event.shiftKey)
    gran = gran * 0.1;                  // slow down
  else if (event.ctrlKey)
    gran = gran * 10;                   // speed up
  return gran;
}

/** Turn accumulated spin drag motions into actual value changes.
 * @this{any}
 */
function spin_drag_change (this: any)
{
  const spin_drag = this, element = spin_drag.element;
  const drag = spin_drag.drag, last = spin_drag.last;
  const has_ptrlock = Util.has_pointer_lock (element);
  const dx = (has_ptrlock ? drag.x : drag.x - last.x) * 0.5;
  const dy =  has_ptrlock ? drag.y : drag.y - last.y;
  let s = true;       // adjust via Increase and:
  if (dy > 0)         // if DOWN
    s = dx >= dy;     //   Decrease unless mostly RIGHT
  else if (dx < 0)    // if LEFT
    s = dy <= dx;     //   Decrease unless mostly UP
  // reset drag accumulator
  if (has_ptrlock)
    spin_drag.drag = { x: 0, y: 0 };
  else
    spin_drag.last = { x: drag.x, y: drag.y };
  // determine accumulated distance
  let dist = (s ? +1 : -1) * Math.sqrt (dx * dx + dy * dy) * spin_drag.ptraccel;
  // convert to physical pixel movements, so knob behaviour is unrelated to display resolution
  if (!has_ptrlock ||
      (has_ptrlock && CONFIG.dpr_movement))
    {
      const DPR = window.devicePixelRatio || 1;
      dist *= DPR;
    }
  // assign value, stop dragging if return is true
  if (spin_drag.value_callback (dist))
    spin_drag_stop (element);
}

/** Handle sping drag pointer motion.
 * @this{any}
 */
function spin_drag_pointermove (this: any, event)
{
  console.assert (event.type === 'pointermove');
  const spin_drag = this, element = spin_drag.element;
  if (!spin_drag.pending_change) // debounce value updates
    spin_drag.pending_change = requestAnimationFrame (() => {
      spin_drag.pending_change = null;
      spin_drag_change.call (spin_drag);
    });
  const has_ptrlock = Util.has_pointer_lock (element);
  if (has_ptrlock)
    {
      spin_drag.drag.x += event.movementX;
      spin_drag.drag.y += event.movementY;
    }
  else
    spin_drag.drag = { x: event.pageX, y: event.pageY };
  spin_drag.ptraccel = spin_drag_granularity (event);
  spin_drag.stop_event (event);
}

/** Stop sping drag event handlers and pointer grab.
 * @this{any}
 */
function spin_drag_stop (this: any, event_or_element = undefined)
{
  const spin_drag = event_or_element instanceof MouseEvent ? this : event_or_element[SPIN_DRAG];
  if (!spin_drag?.stop)
    return;
  const element = spin_drag.element;
  if (event_or_element instanceof MouseEvent)
    spin_drag.stop_event (event_or_element);
  element.removeEventListener ('pointerup', spin_drag.stop);
  element.removeEventListener ('pointermove', spin_drag.pointermove);
  document.body.removeEventListener ('wheel', spin_drag.stop_event, { capture: true, /*passive: false*/ });
  // unset drag mode
  spin_drag.unlock_pointer = spin_drag.unlock_pointer?.();
  if (spin_drag.captureid !== undefined)
    element.releasePointerCapture (spin_drag.captureid);
  spin_drag.captureid = undefined;
  spin_drag.pending_change = cancelAnimationFrame (spin_drag.pending_change);
  spin_drag.last = null;
  spin_drag.drag = null;
  spin_drag.pointermove = null;
  spin_drag.stop = null;
  delete element[SPIN_DRAG];
  Shell.data_bubble.unforce (element);
}

/// Setup drag handlers for numeric spin button behavior.
export function spin_drag_start (element, event, value_callback)
{
  console.assert (element instanceof Element);
  console.assert (event.type === 'pointerdown');
  // allow only primary button press (single click)
  if (event.buttons != 1 || element[SPIN_DRAG])
    {
      if (element[SPIN_DRAG])
	spin_drag_stop (element);
      return false;
    }
  const spin_drag: any = {};
  Object.assign (spin_drag, {
    element,
    value_callback,
    pending_change: null,
    captureid: undefined,
    unlock_pointer: undefined,
    stop_event: event => { event.preventDefault(); event.stopPropagation(); },
    pointermove: spin_drag_pointermove.bind (spin_drag),
    stop: spin_drag_stop.bind (spin_drag),
    ptraccel: 1.0,
    last: null,
    drag: null,
  });
  // setup drag mode
  try {
    spin_drag.element.setPointerCapture (event.pointerId);
    spin_drag.captureid = event.pointerId;
  } catch (e) {
    // something went wrong, bail out the drag
    console.warn ('drag_start: error:', (e as Error).message);
    return false;
  }
  // use pointer lock for knob turning
  if (USE_PTRLOCK)
    spin_drag.unlock_pointer = Util.request_pointer_lock (element);
  const has_ptrlock = Util.has_pointer_lock (element);
  spin_drag.last = { x: event.pageX, y: event.pageY };
  spin_drag.drag = has_ptrlock ? { x: 0, y: 0 } : { x: event.pageX, y: event.pageY };
  spin_drag.stop_event (event);
  document.body.addEventListener ('wheel', spin_drag.stop_event, { capture: true, passive: false });
  element.addEventListener ('pointermove', spin_drag.pointermove);
  element.addEventListener ('pointerup', spin_drag.stop);
  element[SPIN_DRAG] = spin_drag;
  Shell.data_bubble.force (element);
  return true; // spin drag started
}

// == COMPONENT ==
export function Knob (props: {
  prop?: any;
  nosize?: boolean;
  hscroll?: boolean;
  vscroll?: boolean;
  disabled?: boolean;
  [key: string]: any;
})
{
  // Restore Lit class defaults: horizontal wheel scrolls the panel, vertical wheel adjusts the knob.
  const props2 = mergeProps ({ hscroll: false, vscroll: true }, props);
  let root_el: HTMLDivElement | undefined;
  let sprite_el: HTMLDivElement | undefined;
  let clear_notify_cb: (() => void) | undefined;
  const setters_inflight = { v: 0 };
  let button1date = 0;
  let last_ = 0;
  let text_ = '';

  const relabel_cb = Util.debounce (relabel);
  const queue_commit = Util.debounce (commit_value);

  function reposition()
  {
    if (!sprite_el)
      return;
    const steps = 193 - 1;
    const r = Math.round (steps * last_);
    sprite_el.style.backgroundPosition = "0px calc(" + -r + " * var(--b-knob-size))";
    relabel_cb();
  }

  async function relabel()
  {
    let text = props.prop?.get_text();
    const label = props.prop?.label_;
    const nick = props.prop?.nick_;
    let tip = "**DRAG** Adjust Value **DBLCLICK** Reset Value";
    text = await text;
    if (nick)
      tip = '**' + nick + '** ' + text + ' ' + tip;
    let bubble = text;
    if (label)
      bubble = '' + label + '   ' + text;
    root_el?.setAttribute ('data-tip', tip);
    root_el?.setAttribute ('data-bubble', bubble);
    App.zmove();
  }

  function prop_changed (newprop: any)
  {
    clear_notify_cb?.();
    clear_notify_cb = undefined;
    if (!newprop)
      return;
    clear_notify_cb = newprop.on ('notify', notify_value);
    setters_inflight.v = 0;
    last_ = newprop?.fetch_() ?? 0;
    text_ = '';
    reposition();
    notify_value();
  }

  async function notify_value()
  {
    // interactive knob changes may cause bursts of notify_value() calls, to avoid
    // paint jitter, notifications are ignored that are dispatched before setters return
    if (setters_inflight.v)
      return;
    // perform actual update
    let val = props.prop?.get_normalized(), text = props.prop?.get_text();
    val = await val;
    text = await text;
    if (!setters_inflight.v &&
	(last_ !== val || text_ !== text))
      {
	// if (Math.abs (val - this.last_) > 0.001) debug ("%cDIFF: " + (val - this.last_), "color: red", val, this.last_);
	last_ = val;
	text_ = text;
	reposition();
      }
  }

  function wheel_event (event: WheelEvent)
  {
    if (props.disabled)   // let scroll events pass through, no value changes
      return;
    const d = Mouse.wheel_delta (event);
    if ((sprite_el as any)?.[SPIN_DRAG]?.captureid === undefined && // not dragging
	((!props2.hscroll && d.x != 0) ||
	 (!props2.vscroll && d.y != 0))) // consume scroll iff hscroll/vscroll enabled
      return;	// only consume scroll events if enabled
    // handle one scroll direction at a time
    const delta = -d.y || d.x * 0.75;
    // base changes on last known knob state
    if ((delta > 0 && last_ < 1) || (delta < 0 && last_ > 0))
      {
	const wheel_accel = spin_drag_granularity (event);
	last_ = Util.clamp (last_ + delta * wheel_accel, 0, +1);
	queue_commit(); // commit this.last_
      }
    event.preventDefault();
    event.stopPropagation();
  }

  async function commit_value()
  {
    if (props.disabled)   // ignore stale queued commits
      return;
    console.assert (last_ >= 0 && last_ <= 1.0);
    // assign value and maintain counter to ignore self-induced notifications
    setters_inflight.v += 1;
    const promise = props.prop?.set_normalized (last_);
    // reflect interactive updates in current frame
    reposition();
    // synchronization point for ignored notifications
    await promise;
    if (setters_inflight.v)   // might have been reset meanwhile
      {
	setters_inflight.v -= 1;
	if (!setters_inflight.v)
	  props.prop?.update_();  // update to catch potential outside value changes
      }
  }

  function pointerdown (event: PointerEvent)
  {
    if (props.disabled)   // no dragging or double-click reset when disabled
      return;
    // handle double click
    if (event.buttons == 1)
      {
        const now = Number (new Date());
        if (button1date && now - button1date <= 500)
          {
            button1date = 0;
            spin_drag_stop (sprite_el);
            props.prop?.reset();
            return;
          }
        else
          button1date = now;
      }
    spin_drag_start (sprite_el, event, drag_change);
  }

  function drag_change (distance: number)
  {
    last_ = Util.clamp (last_ + distance, 0, +1);
    queue_commit(); // commit this.last_
  }

  // External prop changes trigger setup
  createEffect (() => {
    const p = props.prop;
    prop_changed (p);
  });

  // Cleanup on unmount: release notify subscription and abort any in-flight spin drag
  onCleanup (() => {
    spin_drag_stop (sprite_el);
    clear_notify_cb?.();
    clear_notify_cb = undefined;
  });

  // Determine bidir from prop hints (reactive getter, tracks props.prop changes)
  const bidir = () => props.prop?.hints_?.search (/:bidir:/) >= 0;

  return (
    <div class="b-knob" aria-disabled={props.disabled || undefined} ref={root_el}>
      <div id="sprite" {...{ "bool:bidir": bidir() }} ref={sprite_el}
        onWheel={wheel_event}
        onPointerDown={pointerdown}
        onDblClick={Util.prevent_event}
      >
      </div>
    </div>
  );
}
