// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

if (0) { // debug code
  document.body.addEventListener ('wheel',       e => console.log (wheel_delta (e, true)), { capture: true, passive: true });
  document.body.addEventListener ('pointermove', e => console.log (chrome_dppx + ':', e.clientX - last_event.clientX, event_movement (e, true)), { capture: true, passive: true });
}

/// Flag indicating Chrome UserAgent.
// Indicates need for Chrome coordinate fixing.
// See: https://bugs.chromium.org/p/chromium/issues/detail?id=1092358 https://github.com/w3c/pointerlock/issues/42
export const CHROME_UA = (() => {
  const chrome_match = /\bChrome\/([0-9]+)\./.exec (navigator.userAgent);
  if (chrome_match) {
    const chrome_major = parseInt (chrome_match[1]);
    return chrome_major >= 37; // introduction of movementX, movementY
  }
  return false;
}) ();

// Measure dots-per-physical-pixel as Chrome workaround
function body_move_event_4chrome (ev)
{
  const prev = last_event;
  const ret = body_move_event (ev);
  if (prev && prev != last_event &&
      prev.screenY != last_event.screenY) {
    const cy = last_event.clientY - prev.clientY;
    const sy = last_event.screenY - prev.screenY;
    chrome_dppx = Math.min (Math.max (cy * devicePixelRatio / sy, 1), 4);
    // Done, swap-in the real handler
    document.body.removeEventListener ("pointermove", body_move_event_4chrome, { capture: true, passive: true });
    document.body.addEventListener ("pointermove", body_move_event, { capture: true, passive: true });
  }
  return ret;
}
let chrome_dppx = 1; // dppx based on clientY/screenY

// Install global event listeners for mouse movements.
function body_move_event (event)
{
  if (event.isTrusted &&
      (!last_event || last_event.timeStamp <= event.timeStamp)) {
    last_event = event;
    zmove_trigger();
  }
  // FF-122: clientX scales with devicePixelRatio, movementX and screenX scale with devicePixelRatio
  // CR-120: clientX scales with devicePixelRatio, movementX and screenX remain unscaled
}
let last_event = null;
document.body.addEventListener ("pointermove", CHROME_UA ? body_move_event_4chrome : body_move_event, { capture: true, passive: true });

/// Determine factor for Chrome to convert movementX to CSS px (based on event history)
export function chrome_movement_factor()
{
  return chrome_dppx / devicePixelRatio;
}

/// Determine `movementX,movementY` in CSS pixels, independent of browser quirks.
export function event_movement (event, shift_swaps = false)
{
  let mx = event.movementX, my = event.movementY;
  if (CHROME_UA) {
    const cf = chrome_movement_factor();
    mx *= cf;
    my *= cf;
  }
  let shiftSwapped = false;
  if (shift_swaps && event.shiftKey) {
    const tx = mx;
    mx = my;
    my = tx;
    shiftSwapped = true;
  }
  return { movementX: mx, movementY: my, coalesced: event.getCoalescedEvents().length,
	   shiftSwapped, shiftKey: event.shiftKey, ctrlKey: event.ctrlKey, altKey: event.altKey };
  // FF-122: movementX,movementY values in getCoalescedEvents() are utterly broken
}

/** Determine mouse wheel deltas in bare units, independent of zoom or DPI.
 * This returns an object `{deltaX,deltaY}` with negative values pointing LEFT/UP and positive values RIGHT/DOWN respectively.
 * For zoom step interpretation, the x/y pixel values should be reduced via `Math.sign()`.
 * For scales the pixel values might feel more natural, because browsers sometimes increase the number of events with
 * increasing wheel distance, in other cases values are accumulated so fewer events with larger deltas are sent instead.
 * The members `{x,y}` approximate the wheel delta in pixels.
 */
export function wheel_delta (event, shift_swaps = false)
{
  // The delta values must be read *first* in FF, see: https://bugzilla.mozilla.org/show_bug.cgi?id=1392460#c33
  let dx = event.deltaX, dy = event.deltaY, dz = event.deltaZ;
  let dppx = CHROME_UA ? devicePixelRatio / Math.round (chrome_dppx) : 1;
  // Also: https://github.com/facebook/Rapid/blob/724a7c92f73c295b87ca9b6a4568ce4e25074057/modules/pixi/PixiEvents.js#L359-L417
  if (event.deltaMode == WheelEvent.DOM_DELTA_LINE)
    dppx *= 8;
  else if (event.deltaMode == WheelEvent.DOM_DELTA_PAGE)
    dppx *= 24;
  dx *= dppx;
  dy *= dppx;
  dz *= dppx;
  let shiftSwapped = false;
  if (shift_swaps && event.shiftKey && dx == 0 && dy) {
    dx = dy;
    dy = 0;
    shiftSwapped = true;
  }
  let x = dx, y = dy, z = dz;
  if (event.deltaMode == WheelEvent.DOM_DELTA_PIXEL) {
    x *= 1.0 / 120;
    y *= 1.0 / 120;
    z *= 1.0 / 120;
  }
  return { deltaX: dx, deltaY: dy, deltaZ: dz, deltaMode: WheelEvent.DOM_DELTA_PIXEL, x, y, z,
	   shiftSwapped, shiftKey: event.shiftKey, ctrlKey: event.ctrlKey, altKey: event.altKey };
  // FF-122: deltaY is mostly close to ±120 for mouse wheel on Linux regardless of zoom mode, and regardless of screen DPI
  // CR-121: deltaY is ±120 for mouse wheel on Linux when devicePixelRatio=1, but is scaled along with zoom and screen DPI
  // CR-120: deltaY is ±6 for touchpad on Linux
}

/// Peek at the last pointer coordination event.
export function zmove_last()
{
  return last_event || null;
}

/// Add hook to be called once the mouse position or stacking state changes, returns deleter.
export function zmove_add (hook)
{
  zmove_hooks.push (hook);
  return () => {
    const i = zmove_hooks.indexOf (hook);
    i <= -1 || zmove_hooks.splice (i, 1);
  };
}
const zmove_hooks = [];

let zmove_rafid;
function zmove_raf_handler()
{
  zmove_rafid = undefined;
  if (!last_event) return;
  for (const hook of zmove_hooks)
    hook (last_event);
}

/// Trigger zmove hooks, this is useful to get debounced notifications for pointer movements,
/// including 0-distance moves after significant UI changes.
export function zmove_trigger (ev = undefined)
{
  if (!zmove_rafid)
    zmove_rafid = requestAnimationFrame (zmove_raf_handler);
}
document.body.addEventListener ("pointerdown", zmove_trigger, { capture: true, passive: true });
document.body.addEventListener ("pointerup", zmove_trigger, { capture: true, passive: true });
