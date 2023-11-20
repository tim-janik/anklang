// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import { LitComponent, html, JsExtract, live, docs, ref } from '../little.js';
import * as Util from '../util.js';

/** @class BTrackVolume
 * @description
 * The <b-trackvolume> element is an editor for that track volume
 * The input `value` will be constrained to take on an amount between `min` and `max` inclusively.
 * ### Properties:
 * *value*
 * : Contains the number being edited.
 * *track*
 * : The track
 * ### Events:
 * *valuechange*
 * : Event emitted whenever the volume changes.
 */

// <STYLE/>
JsExtract.scss`
b-trackvolume {
  display: flex; justify-content: flex-end;
  .-trackvolume-bg {
    background-color: rgba( 0, 0, 0, .80);
    width: 10em;
    height: 1em;
    margin: 2px;
  }
  .-trackvolume-fg {
    background-color: #999999;
    height: 1em;
  }
}`;

// <HTML/>
const HTML = t => [
html`
<div class="-trackvolume-bg" @pointerdown="${t.pointerdown}">
<div class="-trackvolume-fg" style="width: ${t.percent}%">
</div>
</div>
`
];

const OBJ_ATTRIBUTE = { type: Object, reflect: true };    // sync attribute with property

// <SCRIPT/>
class BTrackVolume extends LitComponent {
  createRenderRoot() { return this; }
  render() { return HTML (this); }
  static properties = {
    track:      OBJ_ATTRIBUTE,
    value:	{ type: Number },
    percent:    { type: Number },
  };
  constructor() {
    super();
    this.value = 0;
    this.percent = 0;
    this.last_ = 0;
    this.track = null;
    this.prop = null;
  }
  updated (changed_props)
  {
    this.update_value();
    var db_value = 20 * Math.log10 (2*this.value**3);
    var db_string;
    if (this.value === 0)
      db_string = "-" + "\u221E";
    else
      db_string = db_value.toFixed (1);
    this.setAttribute ('data-bubble', "Volume " + db_string + " dB");
  }
  async update_value()
  {
    if (!this.prop)
      {
        let prop = await this.track.access_property ("volume");
        this.prop = prop;
      }
    this.value = await this.prop.get_normalized();
    this.percent = this.value * 100;
    this.last_ = this.value;
  }
  pointerdown (event)
  {
    spin_drag_start (this, event, this.drag_change.bind (this));
  }
  drag_change (distance)
  {
    this.last_ = Util.clamp (this.last_ + distance, 0, +1);
    this.value = this.last_;
    this.percent = this.value * 100;
    if (this.prop)
      this.prop.set_normalized (this.value);
  }
}
customElements.define ('b-trackvolume', BTrackVolume);

const SPIN_DRAG = Symbol ('SpinDrag');
const USE_PTRLOCK = true;

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
