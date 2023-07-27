// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/**
 * # B-KNOB
 * This element provides a knob for scalar inputs.
 * It supports the Vue
 * [v-model](https://vuejs.org/v2/guide/components-custom-events.html#Customizing-Component-v-model)
 * protocol by emitting an `input` event on value changes and accepting inputs via the `value` prop.
 * ## Props:
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
 * ## Events:
 * *update:value (value)*
 * : Value change notification event, the first argument is the new value.
 * *reset:value ()*
 * : Request to reset the value.
 * ## Implementation Notes
 * The knob is rendered based on an SVG drawing, which is arranged in such a
 * way that adding rotational transforms to the SVG elements is sufficient to
 * display varying knob levels.
 * Chrome cannot render individual SVG nodes into seperate layers (GPU textures)
 * so utilizing GPU acceleration requires splitting the original SVG into
 * several SVG layers, each of which can be utilized as a seperate GPU texture with
 * the CSS setting `will-change: transform`.
 */

import { LitComponent, html, JsExtract, docs } from '../little.js';
import * as Util from "../util.js";

// == STYLE ==
JsExtract.scss`
b-knob {
  display: flex; position: relative;
  margin: 0; padding: 0; text-align: center;
  &.b-knob-h4w svg { position: absolute; width:  100%; } //* height for width */
  &.b-knob-w4h svg { position: absolute; height: 100%; } //* width for height */
  .b-knob-trf {
    will-change: transform; /* request GPU texture for fast transforms */
  }
  & svg.b-knob-sizer {
    //* empty SVG element, used by .b-knob to determine width from height from viewBox */
    position: relative; //* participate in layout space allocation */
  }
  #sprite {
    display: inline-block;
    background: url("assets/cknob193u.png");
    &[bidir] { background: url("assets/cknob193b.png"); }
    background-repeat: no-repeat;
    --pxsize: 40px;
    background-size: calc(1.0 * var(--pxsize));
    will-change: background-position;
    width:  calc(1.0 * var(--pxsize));
    height: calc(1.0 * var(--pxsize));
  }
}`;

// == HTML ==
const HTML = (t,d) => html`
  <div id="sprite" ?bidir=${d.bidir}   @wheel="${t.wheel}"
    @pointerdown="${t.pointerdown}"
    @dblclick="${Util.prevent_event}"
    style="background-position: 0px calc(-270 * var(--pxsize))"></div>
`;

// == SCRIPT ==
const OBJ_ATTRIBUTE = { type: Object, reflect: true };  // sync attribute with property
const BOOL_ATTRIBUTE = { type: Boolean, reflect: true };  // sync attribute with property
const USE_PTRLOCK = true;

class BKnob extends LitComponent {
  createRenderRoot() { return this; }
  render()
  {
    const bidir = this.prop.hints_.search (/:bidir:/) >= 0;
    return HTML (this, { bidir });
  }
  static properties = {
    prop: OBJ_ATTRIBUTE,
    nosize: BOOL_ATTRIBUTE,
  };
  constructor()
  {
    super();
    this.sprite = null;
    this.nosize = false;
    this.hscroll = false; // allow horizontal device panel scrolling
    this.vscroll = true;
    this.notify_value_cb = () => this.notify_value();
    this.clear_notify_cb = undefined;
    this.relabel_cb = Util.debounce (this.relabel);
    this.queue_commit = Util.debounce (this.commit_value);
    this.setters_inflight = 0;
    this.button1date = 0;
    this.last_ = 0; // knob value to paint
    this.text_ = '';
    this.prop = null;
  }
  updated (changed_properties)
  {
    this.sprite = this.querySelector ('#sprite');
    foreach_dispatch_propery_changed.call (this, changed_properties);
    this.reposition();
  }
  reposition()
  {
    if (!this.sprite)
      return;
    const steps = 193 - 1;
    const r = Math.round (steps * this.last_);
    /**@type{HTMLElement}*/ (this.sprite).style.backgroundPosition = "0px calc(" + -r + " * var(--pxsize))";
    this.relabel_cb();
  }
  async relabel()
  {
    let text = this.prop?.get_text();
    const label = this.prop?.label_;
    const nick = this.prop?.nick_;
    let tip = "**DRAG** Adjust Value **DBLCLICK** Reset Value";
    text = await text;
    if (nick)
      tip = '**' + nick + '** ' + text + ' ' + tip;
    let bubble = text;
    if (label)
      bubble = '' + label + '   ' + text;
    this.setAttribute ('data-tip', tip);
    this.setAttribute ('data-bubble', bubble);
    App.zmove();
  }
  prop_changed (newprop, oldprop) // this.prop changed
  {
    this.clear_notify_cb?.();
    this.clear_notify_cb = undefined;
    console.assert (newprop === this.prop);
    if (!this.prop)
      return;
    this.clear_notify_cb = this.prop.on ('notify', this.notify_value_cb);
    this.setters_inflight = 0;
    this.last_ = this.prop?.fetch_();
    this.text_ = '';
    this.reposition();
    this.notify_value();
  }
  disconnectedCallback()
  {
    this.clear_notify_cb?.();
    this.clear_notify_cb = undefined;
  }
  async notify_value() // this.prop value changed
  {
    // interactive knob changes may cause bursts of notify_value() calls, to avoid
    // paint jitter, notifications are ignored taht are dispatched before setters return
    if (this.setters_inflight)
      return;
    // perform actual update
    let val = this.prop.get_normalized(), text = this.prop.get_text();
    val = await val;
    text = await text;
    if (!this.setters_inflight &&
	(this.last_ !== val || this.text_ !== text))
      {
	// if (Math.abs (val - this.last_) > 0.001) debug ("%cDIFF: " + (val - this.last_), "color: red", val, this.last_);
	this.last_ = val;
	this.text_ = text;
	this.reposition();
      }
  }
  wheel (event)
  {
    const d = Util.wheel_delta (event);
    if (this[SPIN_DRAG].captureid === undefined && // not dragging
	((!this.hscroll && d.x != 0) ||
	 (!this.vscroll && d.y != 0)))
      return;	// only consume scroll events if enabled
    // handle one scroll direction at a time
    const delta = -d.y || d.x * 0.75;
    // base changes on last known knob state
    if ((delta > 0 && this.last_ < 1) || (delta < 0 && this.last_ > 0))
      {
	const wheel_accel = spin_drag_granularity (event);
	this.last_ = Util.clamp (this.last_ + delta * wheel_accel, 0, +1);
	this.queue_commit(); // commit this.last_
      }
    event.preventDefault();
    event.stopPropagation();
  }
  async commit_value()
  {
    console.assert (this.last_ >= 0 && this.last_ <= 1.0);
    // assign value and maintain counter to ignore self-induced notifications
    this.setters_inflight += 1;
    const promise = this.prop.set_normalized (this.last_);
    // reflect interactive updates in current frame
    this.reposition();
    // synchronization point for ignored notifications
    await promise;
    if (this.setters_inflight)   // might have been reset meanwhile
      {
	this.setters_inflight -= 1;
	if (!this.setters_inflight)
	  this.prop?.update_();  // update to catch potential outside value changes
      }
  }
  pointerdown (event)
  {
    // handle double click
    if (event.buttons == 1)
      {
        const button1date = Number (new Date());
        if (this.button1date && button1date - this.button1date <= 500)
          {
            this.button1date = 0;
            spin_drag_stop (this);
            this.prop.reset();
            return;
          }
        else
          this.button1date = button1date;
      }
    spin_drag_start (this, event, this.drag_change.bind (this));
  }
  drag_change (distance)
  {
    this.last_ = Util.clamp (this.last_ + distance, 0, +1);
    this.queue_commit(); // commit this.last_
  }
  dblclick (event)
  {
    spin_drag_stop (this);
    this.prop.reset();
  }
}
customElements.define ('b-knob', BKnob);

/** @this{BKnob} */
function foreach_dispatch_propery_changed (changed_properties)
{
  changed_properties.forEach ((oldprop, propname) => {
    const changed_method = this[propname + '_changed'];
    if (changed_method)
      changed_method.call (this, this[propname], oldprop);
  });
  // when ´this.foo´ changed, call `this.foo_changed ('foo', oldfoo)´.
}

const SPIN_DRAG = Symbol ('SpinDrag');

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
  const spin_drag = {};
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
    console.warn ('drag_start: error:', /**@type{Error}*/ (e).message);
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

/** Stop sping drag event handlers and pointer grab.
 * @this{any}
 */
function spin_drag_stop (event_or_element= undefined)
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

/** Handle sping drag pointer motion.
 * @this{any}
 */
function spin_drag_pointermove (event)
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

/** Turn accumulated spin drag motions into actual value changes.
 * @this{any}
 */
function spin_drag_change ()
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

/// Calculate spin drag acceleration (slowdown) from event type and modifiers.
function spin_drag_granularity (event)
{
  const radius = 64;                    // assumed knob size
  const circum = 2 * radius * Math.PI;
  let gran = 1 / circum;                // steps per full turn to feel natural
  if (event.shiftKey)
    gran = gran * 0.1;                  // slow down
  else if (event.ctrlKey)
    gran = gran * 10;                   // speed up
  if (event.type == 'wheel')
    gran /= 2;                          // approximate mouse movements in pixels
  return gran;
}
