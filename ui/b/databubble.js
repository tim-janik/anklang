// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import { JsExtract } from '../little.js';
import * as Util from '../util.js';

/** @class DataBubbleImpl
 * @description
 * The **DataBubbleImpl** and **DataBubbleIface** classes implement the
 * logic required to extract and display `data-bubble=""` tooltip popups on mouse hover.
 */

// <STYLE/>
JsExtract.scss`
/* Bubble color setup */
$b-data-bubble-hue: 52deg;
$b-data-bubble-fg:  hsl($b-data-bubble-hue, 100%, 1%);
$b-data-bubble-bg:  hsl($b-data-bubble-hue, 100%, 90%);
$b-data-bubble-bg2: zmod($b-data-bubble-bg, Jz+=3%);
$b-data-bubble-br:  $b-data-bubble-bg2;

/* Tooltips via CSS, using the data-bubble="" attribute */
.b-data-bubble {
  position: absolute; pointer-events: none;
  display: flex; max-width: 95vw;
  margin: 0; padding: 0 0 8px 0; /* make room for triangle: 5px + 3px */
  transition: visibility 0.1s, opacity 0.09s ease-in-out;
  visibility: hidden; opacity: 0;

  .b-data-bubble-inner {
    display: block; overflow: hidden; position: relative;
    white-space: normal; margin: 0;
    max-width: 40em; border-radius: 3px;
    // border: dppx(2) solid zmod($b-data-bubble-bg2, Jz+=5%);
    box-shadow: 0 0 0 1px fade($b-data-bubble-br, 0.8), 0px 0px 2px 1px black;
    color: $b-data-bubble-fg; padding: 0.5em 0.5em 0.4em;
    background: $b-data-bubble-bg;
    background-image: linear-gradient(to bottom right, $b-data-bubble-bg, $b-data-bubble-bg2);
    font-variant-numeric: tabular-nums;
  }
  &.b-data-bubble-visible {
    visibility: visible; opacity: 0.95;
  }
  /* Triangle acting as bubble pointer */
  &::after {
    position: absolute; bottom: calc(5px - 2); /* room below triangle: 5px */
    left: calc(50% - 5px); width: 0; height: 0; content: "";
    border-top: 5px solid $b-data-bubble-br;
    border-left: 5px solid transparent;
    border-right: 5px solid transparent;
  }
  /* markdown styling for data-bubble */
  .b-markdown-it-outer {
    @include b-markdown-it-inlined;
    $fsf: 1.05; //* font size factor */
    h1 { font-size: calc(pow($fsf, 6) * 1em); }
    h2 { font-size: calc(pow($fsf, 5) * 1em); }
    h3 { font-size: calc(pow($fsf, 4) * 1em); }
    h4 { font-size: calc(pow($fsf, 3) * 1em); }
    h5 { font-size: calc(pow($fsf, 2) * 1em); }
    h6 { font-size: calc(pow($fsf, 1) * 1em); }
  }
}`;

/** A mechanism to display data-bubble="" tooltip popups */
class DataBubbleImpl {
  bubble_layer() {
    const bubble_layer_id = '#b-shell-bubble-layer';
    const el = document.body.querySelectorAll (bubble_layer_id);
    console.assert (el.length, "Missing container:", bubble_layer_id);
    return el[0];
  }
  constructor() {
    // create one toplevel div.data-bubble element to deal with all popups
    this.bubble = document.createElement ('div');
    this.bubble.classList.add ('b-data-bubble');
    this.bubblediv = document.createElement ('div');
    this.bubblediv.classList.add ('b-data-bubble-inner');
    this.bubble.appendChild (this.bubblediv);
    this.bubble_layer().appendChild (this.bubble);
    this.current = null; // current element showing a data-bubble
    this.stack = []; // element stack to force bubble
    this.lasttext = "";
    this.last_event = null;
    this.buttonsdown = false;
    this.coords = null;
    // milliseconds to wait to detect idle time after mouse moves
    const IDLE_DELAY = 115;
    // trigger popup handling after mouse rests
    this.restart_bubble_timer = Util.debounce (() => this.check_showtime (true),
					       { restart: true, wait: IDLE_DELAY });
    this.debounced_check = Util.debounce (this.check_showtime);

    const recheck_event = event => {
      this.last_event = event;
      this.check_showtime();
    };
    this.zmovedel = App.zmoves_add (recheck_event);

    this.resizeob = new ResizeObserver (() => !!this.current && this.debounced_check());
  }
  check_showtime (showtime = false) {
    // check stack hide/show needs
    if (this.stack.length)
      {
	const next = this.stack[0];
	if (next != this.current)
	  {
	    this.hide();
	    this.restart_bubble_timer.cancel();
	  }
	if (this.current)
	  this.update_now();
	else
	  this.show (next);
	return;
      }
    // restart timer on move
    if (this.last_event)
      {
	const coords = { x: this.last_event.screenX, y: this.last_event.screenY };
	this.buttonsdown = !!this.last_event.buttons;
	if (!this.buttonsdown &&
	    !this.stack.length && this.coords &&
	    (this.coords.x != coords.x || this.coords.y != coords.y) &&
	    this.last_event.type === "pointermove")
	  {
	    this.restart_bubble_timer();
	  }
	this.coords = coords; // needed to ignore 0-distance moves
	this.last_event = null;
      }
    // hide on button events
    if (this.buttonsdown)
      {
	this.hide();
	this.restart_bubble_timer.cancel();
	return;
      }
    // handle visible bubble
    if (!showtime && !this.current)
      return;
    // bubble matches hovered element?
    const els = document.body.querySelectorAll ('*:hover[data-bubble]');
    const next = els.length ? els[els.length - 1] : null;
    if (next != this.current)
      this.hide();
    // show new bubble or update
    if (next && showtime && !this.current)
      this.show (next);
    else if (this.current)
      this.update_now();
  }
  hide() {
    // ignore the next 0-distance move
    this.coords = null;
    // hide bubble if any
    if (this.current)
      {
	delete this.current.data_bubble_active;
	this.current = null;
	this.resizeob.disconnect();
	this.bubble.classList.remove ('b-data-bubble-visible');
      }
    this.lasttext = ""; // no need to keep textContent for fade-outs
  }
  update_now() {
    if (this.current)
      {
	let cbtext;
	if (this.current.data_bubble_callback)
	  {
	    cbtext = this.current.data_bubble_callback();
	    this.current.setAttribute ('data-bubble', cbtext);
	  }
	const newtext = cbtext || this.current.getAttribute ('data-bubble');
	if (!newtext)
	  this.hide();
	else if (newtext != this.lasttext)
	  {
	    this.lasttext = newtext;
	    Util.markdown_to_html (this.bubblediv, this.lasttext);
	  }
      }
  }
  show (element) {
    console.assert (!this.current);
    this.current = element;
    this.current.data_bubble_active = true;
    this.resizeob.observe (this.current);
    this.bubble.classList.add ('b-data-bubble-visible');
    this.lasttext = "";
    this.update_now(); // might hide()
    if (this.current) // resizing
      {
	this.bubble_layer().appendChild (this.bubble); // restack the bubble
	const viewport = {
	  width:  Math.max (document.documentElement.clientWidth || 0, window.innerWidth || 0),
	  height: Math.max (document.documentElement.clientHeight || 0, window.innerHeight || 0),
	};
	// request ideal layout
	const r = this.current.getBoundingClientRect();
	this.bubble.style.top = '0px';
	this.bubble.style.right = '0px';
	let s = this.bubble.getBoundingClientRect();
	let top = Math.max (0, r.y - s.height);
	let right = Math.max (0, viewport.width - (r.x + r.width / 2 + s.width / 2));
	this.bubble.style.top = top + 'px';
	this.bubble.style.right = right + 'px';
	s = this.bubble.getBoundingClientRect();
	// constrain layout
	if (s.left < 0)
	  {
	    right += s.left;
	    right = Math.max (0, right);
	    this.bubble.style.right = right + 'px';
	  }
      }
  }
}

const UNSET = Symbol ('UNSET');

class DataBubbleIface {
  constructor() {
    this.data_bubble = new DataBubbleImpl();
  }
  /// Set the `data-bubble` attribute of `element` to `text` or force its callback
  update (element, text = UNSET) {
    if (text !== UNSET && !element.data_bubble_callback)
      {
	if (text)
	  element.setAttribute ('data-bubble', text);
	else
	  element.removeAttribute ('data-bubble');
      }
    App.zmove();
  }
  /// Assign a callback function to fetch the `data-bubble` attribute of `element`
  callback (element, callback) {
    element.data_bubble_callback = callback;
    if (callback)
      element.setAttribute ('data-bubble', "");	// [data-bubble] selector needs existing attribute
    App.zmove();
  }
  /// Force `data-bubble` to be shown for `element`
  force (element) {
    this.data_bubble.stack.unshift (element);
    this.data_bubble.debounced_check();
  }
  /// Cancel forced `data-bubble` for `element`
  unforce (element) {
    if (this.data_bubble.stack.length)
      Util.array_remove (this.data_bubble.stack, element);
    this.data_bubble.debounced_check();
  }
  /// Reset the `data-bubble` attribute, its callback and cancel a forced bubble
  clear (element) {
    if (this.data_bubble.stack.length)
      Util.array_remove (this.data_bubble.stack, element);
    if (element.data_bubble_active)
      this.data_bubble.hide();
    if (element.data_bubble_callback)
      element.data_bubble_callback = undefined;
    element.removeAttribute ('data-bubble');
    this.data_bubble.debounced_check();
  }
}
export default DataBubbleIface;
