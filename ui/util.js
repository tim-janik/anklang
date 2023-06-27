// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
'use strict';

import * as Kbd from './kbd.js';

// == Compat fixes ==
class FallbackResizeObserver {
  constructor (resize_handler) {
    this.observables = new Set();
    this.resizer = () => resize_handler.call (null, [], this);
  }
  disconnect() {
    this.observables.clear();
    window.removeEventListener ('resize', this.resizer);
  }
  observe (ele) {
    if (!this.observables.size)
      window.addEventListener ('resize', this.resizer);
    this.observables.add (ele);
  }
  unobserve (ele) {
    this.observables.delete (ele);
    if (!this.observables.size())
      this.disconnect();
  }
}
/// Work around FireFox 68 having ResizeObserver disabled
export const ResizeObserver = window.ResizeObserver || FallbackResizeObserver;

/// Retrieve current time in milliseconds.
export function now () {
  return window.Date.now();
}

/// Retrieve a timestamp that is unique per (animation) frame.
export function frame_stamp()
{
  if (0 === last_frame_stamp_)
    {
      last_frame_stamp_ = 1 * new Date();
      requestAnimationFrame (() => last_frame_stamp_ = 0);
    }
  return last_frame_stamp_;
}
let last_frame_stamp_ = 0;

/** Yield a wrapper function for `callback` that throttles invocations.
 * Regardless of the frequency of calls to the returned wrapper, `callback`
 * will only be called once per `requestAnimationFrame()` cycle, or
 * after `milliseconds`.
 * The return value of the wrapper functions is the value of the last
 * callback invocation.
 * A `cancel()` method can be called on the returned wrapper to cancel the
 * next pending `callback` invocation.
 * Options:
 * - `wait` - number of milliseconds to pass until `callback` may be called.
 * - `restart` - always restart the timer once the wrapper is called.
 * - `immediate` - immediately invoke `callback` and then start the timeout period.
 */
export function debounce (callback, options = {}) {
  if ('number' == typeof options)
    options = { wait: options };
  if (!(callback instanceof Function))
    throw new TypeError ('argument `callback` must be of type Function');
  const restart = !!options.restart;
  const milliseconds = Number.isFinite (options.wait) ? options.wait : -1;
  let immediate = milliseconds >= 0 && !!options.immediate;
  options = undefined; // allow GC
  let handlerid = undefined;
  let cresult = undefined;
  let args = undefined;
  function callback_caller() {
    handlerid = undefined;
    if (args)
      {
	const cthis = args.cthis, cargs = args.cargs;
	args = undefined;
	if (immediate)
	  handlerid = setTimeout (() => callback_caller (false), milliseconds);
	cresult = callback.apply (cthis, cargs);
      }
  }
  function wrapper_func (...newargs) {
    const first_handler = !handlerid;
    if (restart) // always restart timer
      wrapper_func.cancel();
    args = { cthis: this, cargs: newargs };
    if (!handlerid)
      {
	if (milliseconds < 0)
	  handlerid = requestAnimationFrame (callback_caller);
	else if (immediate && first_handler)
	  handlerid = setTimeout (callback_caller, 0);
	else
	  handlerid = setTimeout (callback_caller, milliseconds);
      }
    return cresult;
  }
  wrapper_func.cancel = () => {
    if (handlerid)
      {
	if (milliseconds < 0)
	  handlerid = cancelAnimationFrame (handlerid);
	else // milliseconds >= 0
	  handlerid = clearTimeout (handlerid);
	args = undefined;
      } // keep last result
  };
  return wrapper_func;
}

/** Process all events of type `eventname` with a single callback exclusively */
export function capture_event (eventname, callback) {
  const handler = ev => {
    callback (ev);
    ev.preventDefault();
    ev.stopPropagation();
    return true;
  };
  const options = { capture: true, passive: false };
  document.body.addEventListener (eventname, handler, options);
  const uncapture = () => {
    document.body.removeEventListener (eventname, handler, options);
  };
  return uncapture;
}

/// Expand pointer events into a list of possibly coalesced events.
export function coalesced_events (event) {
  const pevents = event.getCoalescedEvents ? event.getCoalescedEvents() : null;
  if (!pevents || pevents.length == 0)
    return [ event ];
  return pevents;
}

/** Get Vue component handle from element or its ancestors */
export function vue_component (element) {
  let el = element;
  while (el)
    {
      if (el.__vueParentComponent?.proxy)
	return el.__vueParentComponent.proxy; // Vue3
      if (el.__vue__)
	return el.__vue__;                    // Vue2
      el = el.parentNode;
    }
  return undefined;
}

/** Get Envue `$object` from element or its ancestors */
export function envue_object (element) {
  if (element?.$el && element?.$object)
    return element.$object;	// element is Vue component already
  const vm = vue_component (element);
  return vm?.$object;
}

export const START = "START";
export const STOP = "STOP";
export const CANCEL = "CANCEL";
export const MOVE = "MOVE";
export const SCROLL = "SCROLL";

/// Meld all pointer drag handling functions into a single `drag_event(event,MODE)` method.
export class PointerDrag {
  constructor (vuecomponent, event, method = 'drag_event', cleanup = null) {
    this.vm = vuecomponent;
    this.cleanup = cleanup;
    if (typeof (method) === 'function')
      this.drag_method = method;
    else
      this.drag_method = this.vm[method].bind (this.vm);
    this.el = event.target;
    this.pointermove = this.pointermove.bind (this);
    this.el.addEventListener ('pointermove', this.pointermove);
    this.pointerup = this.pointerup.bind (this);
    this.el.addEventListener ('pointerup', this.pointerup);
    this.pointercancel = this.pointercancel.bind (this);
    this.el.addEventListener ('pointercancel', this.pointercancel);
    this.el.addEventListener ('lostpointercapture', this.pointercancel);
    this.keydown = this.keydown.bind (this);
    window.addEventListener ('keydown', this.keydown);
    this.start_stamp = event.timeStamp;
    try {
      this.el.setPointerCapture (event.pointerId);
      this._pointerid = event.pointerId;
    } catch (e) {
      // something went wrong, bail out the drag
      this._disconnect (event);
    }
    if (this.el)
      this.drag_method (event, START);
    if (!this.el)
      this.destroy();
  }
  _disconnect (event) {
    if (this._pointerid)
      {
        this.el.releasePointerCapture (this._pointerid);
	this._pointerid = 0;
      }
    this.el.removeEventListener ('pointermove', this.pointermove);
    this.el.removeEventListener ('pointerup', this.pointerup);
    this.el.removeEventListener ('pointercancel', this.pointercancel);
    this.el.removeEventListener ('lostpointercapture', this.pointercancel);
    window.removeEventListener ('keydown', this.keydown);
    // swallow dblclick after lengthy drags with bouncing start click
    if (this.el && (this.seen_move || event.timeStamp - this.start_stamp > 500)) // milliseconds
      {
	const swallow_dblclick = event => {
	  event.preventDefault();
	  event.stopPropagation();
	  // debug ("PointerDrag: dblclick swallowed");
	};
	window.addEventListener ('dblclick', swallow_dblclick, true);
	setTimeout (_ => window.removeEventListener ('dblclick', swallow_dblclick, true), 40);
      }
    this.el = null;
  }
  destroy (event = null) {
    if (this.el)
      {
	event = event || new PointerEvent ('pointercancel');
	this._disconnect (event);
	this.drag_method (event, CANCEL);
	this.el = null;
      }
    this.vm = null;
    if (this.cleanup)
      {
	this.cleanup.call (null);
	this.cleanup = null;
      }
  }
  scroll (event) {
    if (this.el)
      this.drag_method (event, SCROLL);
    //event.preventDefault();
    //event.stopPropagation();
    this.seen_move = true;
  }
  pointermove (event) {
    if (this.el)
      this.drag_method (event, MOVE);
    event.preventDefault();
    event.stopPropagation();
    this.seen_move = true;
  }
  pointerup (event) {
    if (!this.el)
      return;
    this._disconnect (event);
    this.drag_method (event, STOP);
    this.destroy (event);
    event.preventDefault();
    event.stopPropagation();
  }
  keydown (event) {
    if (event.keyCode == 27) // Escape
      {
	event.stopPropagation();
	return this.pointercancel (event);
      }
  }
  pointercancel (event) {
    if (!this.el)
      return;
    this._disconnect (event);
    this.drag_method (event, CANCEL);
    this.destroy (event);
    event.preventDefault();
    event.stopPropagation();
  }
}

/** Start `drag_event (event)` handling on a Vue component's element, use `@pointerdown="drag_event"` */
export function drag_event (event) {
  console.assert (event.type == "pointerdown" && event.pointerId);
  const vuecomponent = vue_component (event.target);
  if (vuecomponent && vuecomponent.$el &&
      vuecomponent.drag_event)			// ignore request if we got wrong vue component (child)
  {
    const pdrag = new PointerDrag (vuecomponent, event);
    return _ => pdrag.destroy();
  }
  return _ => undefined;
}

const unrequestpointerlock = Symbol ('unrequest_pointer_lock');

// Maintain pending_pointer_lock state
function pointer_lock_changed (ev) {
  if (document.pointerLockElement !== pending_pointer_lock)
    {
      // grabbing the lock did not work
      pending_pointer_lock = null;
      // exit erroneous pointer lock
      if (document.pointerLockElement &&
	  document.pointerLockElement[unrequestpointerlock])
	document.pointerLockElement[unrequestpointerlock]();
    }
}
let pending_pointer_lock = null;
document.addEventListener ('pointerlockchange', pointer_lock_changed, { passive: true });
document.addEventListener ('pointerlockerror', pointer_lock_changed, { passive: true });

/** Clear (pending) pointer locks
 * Clear an existing pointer lock on `element` if any and
 * ensure it does not get a pointer lock granted unless
 * request_pointer_lock() is called on it again.
 */
export function unrequest_pointer_lock (element) {
  console.assert (element instanceof Element);
  // this API only operates on elements that have the [unrequestpointerlock] member set
  if (element[unrequestpointerlock])
    {
      if (pending_pointer_lock === element)
	pending_pointer_lock = null;
      if (document.pointerLockElement === element)
	document.exitPointerLock();
      element[unrequestpointerlock] = null;
    }
}

/** Check if `element` has a (pending) pointer lock
 * Return:
 * - 2- if `element` has the pointer lock;
 * - 1- if the pointer lock is pending;
 * - 0- otherwise.
 */
export function has_pointer_lock (element) {
  if (document.pointerLockElement === element)
    return 2;
  if (pending_pointer_lock === element)
    return 1;
  return 0;
}

/** Request a pointer lock on `element` and track its state
 * Use this function to maintain pointer locks to avoid stuck
 * locks that can get granted *after* exitPointerLock() has been called.
 */
export function request_pointer_lock (element) {
  console.assert (element instanceof Element);
  if (has_pointer_lock (element) && element[unrequestpointerlock])
    return element[unrequestpointerlock];
  // this API only operates on elements that have the [unrequestpointerlock] member set
  element[unrequestpointerlock] = () => unrequest_pointer_lock (element);
  pending_pointer_lock = element;
  if (document.pointerLockElement != element)
    {
      if (document.pointerLockElement)
	document.exitPointerLock();
      pending_pointer_lock.requestPointerLock();
    }
  return element[unrequestpointerlock];
}

/// Ensure the root node of `element` contains a `csstext` (replaceable via `stylesheet_name`)
export function adopt_style (element, csstext, stylesheet_name)
{
  if (Array.isArray (csstext))
    csstext = csstext.join ('');
  stylesheet_name = stylesheet_name || element.nodeName;
  const root = element.getRootNode();
  let ads;
  for (const adoptedsheet of root.adoptedStyleSheets)
    if (adoptedsheet[stylesheet_name] == stylesheet_name)
      {
	ads = adoptedsheet;
	break;
      }
  if (ads)
    ads.replaceSync (csstext);
  else // !ads
    {
      ads = new CSSStyleSheet();
      ads[stylesheet_name] = stylesheet_name;
      ads.replaceSync (csstext);
      root.adoptedStyleSheets = [...root.adoptedStyleSheets, ads];
    }
}

/// Ensure the root node of `element` has a `url` stylesheet link.
export function add_style_sheet (element, url)
{
  let root = element.getRootNode();
  root = root.head || root;
  const has_link = root.querySelector (`LINK[rel="stylesheet"][href="${url}"]`);
  if (has_link)
    return;
  const link = document.createElement ("LINK");
  link.setAttribute ("rel", "stylesheet");
  link.setAttribute ("href", url);
  root.append (link);
}

// == Vue Helpers ==
export const vue_mixins = {};
export const vue_directives = {};

/// Retrieve CSS scope selector for vm_scope_style()
export function vm_scope_selector (vm) {
  console.assert (vm.$);
  return vm.$el.nodeName + `[b-scope${vm.$.uid}]`;
}

/// Attach `css` to Vue instance `vm`, use vm_scope_selector() for the `vm` CSS scope
export function vm_scope_style (vm, css) {
  if (!vm.$el._vm_style)
    {
      vm.$el.setAttribute ('b-scope' + vm.$.uid, '');
      vm.$el._vm_style = document.createElement ('style');
      vm.$el._vm_style.type = 'text/css';
      vm.$el.appendChild (vm.$el._vm_style);
    }
  if (!css.startsWith ('\n'))
    css = '\n' + css;
  if (vm.$el._vm_style.innerHTML != css)
    vm.$el._vm_style.innerHTML = css;
}

/// Loop over all properties in `source` and assign to `target*
export function assign_forin (target, source) {
  for (let p in source)
    target[p] = source[p];
  return target;
}

/// Loop over all elements of `source` and assign to `target*
export function assign_forof (target, source) {
  for (let e of source)
    target[e] = source[e];
  return target;
}

/** Remove element `item` from `array` if present via `indexOf` */
export function array_remove (array, item) {
  const index = array.indexOf (item);
  if (index >= 0)
    {
      array.splice (index, 1);
      return true;
    }
  return false;
}

/** Find `array` index of element that equals `item` */
export function array_index_equals (array, item) {
  for (let i = 0; i < array.length; i++)
    if (equals_recursively (item, array[i]))
      return i;
  return -1;
}

/// Generate map by splitting the key=value pairs in `kvarray`
export function map_from_kvpairs (kvarray) {
  let o = {};
  for (let kv of kvarray) {
    const key = kv.split ('=', 1)[0];
    const value = kv.substr (key.length + 1);
    o[key] = value;
  }
  return o;
}

/** Generate integers [0..`bound`[ if one arg is given or [`bound`..`end`[ by incrementing `step`. */
export function* range (bound, end, step = 1) {
  if (end === undefined) {
    end = bound;
    bound = 0;
  }
  for (; bound < end; bound += step)
    yield bound;
}

/** Freeze object and its properties */
export function freeze_deep (object) {
  for (const key of Object.getOwnPropertyNames (object)) {
    const value = object[key];
    if (value instanceof Object)
      freeze_deep (value);
  }
  return Object.freeze (object);
}

/** Create a new object that has the same properties and Array values as `src` */
export function copy_deep (src = {}) {
  let o;
  if (Array.isArray (src))
    {
      o = Array.prototype.slice.call (src, 0);	// copy array cells, not properties
      for (const k in src)                      // loop over index and property keys
	{
	  let v = o[k];
	  if (v === undefined)                  // empty cell?
	    {
	      v = src[k];
	      if (v !== undefined)              // no, missing property
		o[k] = v instanceof Object && k !== '__proto__' ? copy_deep (v) : v;
	    }
	  else if (v instanceof Object)         // Object cell
	    o[k] = copy_deep (v);
	}
    }
  else if (src instanceof Object && !(src instanceof Function))
    {
      o = Object.assign ({}, src);		// reseve slots and copy properties
      for (const k in o)
	{
	  const v = o[k];
	  if (v instanceof Object &&            // Object/Array property
	      k !== '__proto__')
	    o[k] = copy_deep (v);
	}
    }
  else // primitive or Function
    o = src;
  return o;
}

/** Check if `a == b`, recursively if the arguments are of type Array or Object */
export function equals_recursively (a, b) {
  // compare primitives
  if (a === b)
    return true;
  if (typeof a != typeof b)
    return false;
  if (typeof a == 'number' && isNaN (a) && isNaN (b))
    return true;
  // check Objects, note: typeof null === 'object' && null instanceof Object === false
  if (!(a instanceof Object && b instanceof Object))
    return false;
  // functions must be exactly equal, due to scoping
  if (a instanceof Function)
    return false;
  // check inheritance
  if (a.constructor !== b.constructor ||
      !equals_recursively (Object.getPrototypeOf (a), Object.getPrototypeOf (b)))
    return false;
  // compare Array lengths
  if (Array.isArray (a) && a.length != b.length)
    return false;
  // compare RegExp
  if (a instanceof RegExp)
    return a.source == b.source && a.flags == b.flags && a.lastIndex  == b.lastIndex;
  // compare Objects by non-inherited named properties
  let ak = Object.getOwnPropertyNames (a), bk = Object.getOwnPropertyNames (b);
  if (ak.length != bk.length)
    return false;
  for (let i = 0; i < ak.length; i++)
    {
      const k = ak[i];
      if (k != bk[i])
	return false;
      const av = a[k], bv = b[k];
      if (!(av === bv || equals_recursively (av, bv)))
	return false;
    }
  // compare Objects by non-inherited symbol properties
  ak = Object.getOwnPropertySymbols (a), bk = Object.getOwnPropertySymbols (b);
  if (ak.length != bk.length)
    return false;
  for (let i = 0; i < ak.length; i++)
    {
      const k = ak[i];
      if (k != bk[i])
	return false;
      const av = a[k], bv = b[k];
      if (!(av === bv || equals_recursively (av, bv)))
	return false;
    }
  ak = null;
  bk = null;
  // compare non-Array iterables (e.g. Set)
  if (!Array.isArray (a) && typeof a[Symbol.iterator] == 'function')
    {
      const itera = a[Symbol.iterator]();
      const iterb = b[Symbol.iterator]();
      do {
	const { value: va, done: da } = itera.next();
	const { value: vb, done: db } = iterb.next();
	if (da !== db)
	  return false;
	else if (da)
	  break;
	if (!(va === vb || equals_recursively (va, vb)))
	  return false;
      } while (true);	// eslint-disable-line no-constant-condition
    }
  return true;
  // TODO: equals_recursively() busy loops for object reference cycles
}

if (__DEV__)
  {
    function eqr (a, b) { return equals_recursively (a, b) && equals_recursively (b, a); }
    let a = [ 0, 2, 3, 4, 5, 70, {a:null} ], b = [ 1, 2, 3, 4, 5, 70, {a:null} ]; console.assert (!eqr (a, b)); a[0] = 1; console.assert (eqr (a, b));
    a.x = 9; console.assert (!eqr (a, b)); b.x = 9.0; console.assert (eqr (a, b));
    const c = copy_deep (a); console.assert (eqr (a, c)); c[6].q = 'q'; console.assert (!eqr (a, c));
    const as = new Set (a), bs = new Set (b); console.assert (eqr (as, bs));
    bs.add (99); console.assert (!eqr (as, bs)); bs.delete (99); console.assert (eqr (as, bs));
    // TODO: a[999] = b; b[999] = a; console.assert (eqr (a, b));
    console.assert (eqr (/a/, /a/) && !eqr (/b/i, /b/));
    console.assert (eqr (clamp, clamp) && !eqr (clamp, eqr));
    console.assert (eqr ({ a: 1, b: 2 }, { a: 1, b: 2 }));
    console.assert (!eqr ({ a: 1, b: 2 }, { b: 2, a: 1 }));
    a = {}; b = {};
    console.assert (eqr (a, b));
    Object.defineProperty (a, '$id', { value: 1 });
    Object.defineProperty (b, '$id', { value: 1 });
    console.assert (eqr (a, b));
    b = {};
    Object.defineProperty (b, '$id', { value: 2 });
    console.assert (!eqr (a, b));
  }

/** Return @a x clamped into @a min and @a max. */
export function clamp (x, min, max) {
  return x < min ? min : x > max ? max : x;
}

/** Create a Vue component provide() function that forwards selected properties. */
export function fwdprovide (injectname, keys) {
  return function() {
    const proxy = {};
    for (let key of keys)
      Object.defineProperty (proxy, key, { enumerable: true, get: () => this[key], });
    const provide_defs = {};
    provide_defs[injectname] = proxy;
    return provide_defs;
  };
}

/** Provide `$children` (and `$vue_parent`) on every component. */
vue_mixins.vuechildren = {
  provide() {
    return { '$vue_parent': this };
  },
  inject: {
    '$vue_parent': { from: '$vue_parent', default: null, },
  },
  beforeCreate () {
    console.assert (this.$children === undefined);
    this.$children = [];
  },
  created() {
    // using $parent breaks for transitions, https://github.com/vuejs/docs-next/issues/454
    if (this.$vue_parent)
      this.$vue_parent.$children.push (this);
  },
  unmounted() {
    if (!this.$vue_parent)
      return;
    const pos = this.$vue_parent.$children.indexOf (this);
    if (pos < 0)
      throw Error ("failed to locate this in $vue_parent.$children:", this);
    this.$vue_parent.$children.splice (pos, 1);
  },
};

/** Automatically add `$attrs['data-*']` to `$el`. */
vue_mixins.autodataattrs = {
  mounted: function () {
    autodataattrs_apply.call (this);
  },
  updated: function () {
    autodataattrs_apply.call (this);
  },
};
function autodataattrs_apply () {
  for (let datakey in this.$attrs)
    if (datakey.startsWith ('data-'))
      this.$el.setAttribute (datakey, this.$attrs[datakey]);
}

function call_unwatch (unwatch_cb) {
  // Work around: https://github.com/vuejs/vue-next/issues/2381
  try {
    // Vue-3.0.0 can set instance.effects=null, *before* unwatch_cb() tries to remove itself
    // which throws an exception when removing from a `null` array.
    if (unwatch_cb)
      unwatch_cb();
  } catch (ex) {
    if (ex instanceof TypeError &&
	String (ex).match (/ null/))
      {
	// vue.js:212 Uncaught (in promise) TypeError: Cannot read property 'indexOf' of null
	// at remove (vue.js:212)
	// at Object.unwatch1 (vue.js:6489)
	// at Proxy.beforeUnmount (util.js:727)
      }
    else
      throw ex;
  }
  return null;
}

/** Generate a kebab-case ('two-words') identifier from a camelCase ('twoWords') identifier */
export function hyphenate (string) {
  const uppercase_boundary =  /\B([A-Z])/g;
  return string.replace (uppercase_boundary, '-$1').toLowerCase();
}

const nop = (() => undefined).bind (undefined);

// Call dom_create, dom_change, dom_destroy, watch dom_update and forceUpdate on changes
function dom_dispatch (before_unmount) {
  const destroying = !!before_unmount;
  // dom_create
  if (!destroying &&
      this.$dom_data.state == 0)        // before-create
    {
      this.$dom_data.state = 1;		// created
      this.dom_create?.call (this);
    }
  // dom_change
  if (!destroying &&
      this.$dom_data.state == 1)        // created
    this.dom_change?.call (this);
  // dom_update
  const valid_el = this.$el instanceof Element || this.$el instanceof Text;
  if (!destroying &&
      this.$dom_data.state == 1 &&	// created
      this.dom_update && valid_el)
    {
      this.$dom_data.unwatch1 = call_unwatch (this.$dom_data.unwatch1); // clear old $watch
      // call dom_update() and capture dependencies (upon changes $forceUpdate)
      let dom_update_watcher = () => {
	const r = this.dom_update();
	if (r instanceof Promise)
	  console.warn ('The this.dom_update() method returned a Promise, async functions are not reactive', this);
	return r;
      };
      /* The $watch *expOrFn* is called immediately, the dependencies and return value are recorded.
       * Once one of the data dependencies change, the *expOrFn* is called again, also recording the
       * dependencies and return value. If the return value changed, *callback* is invoked.
       * What we need for updating DOM elements, is:
       * A) the initial call with dependency recording which we use for (expensive) updating,
       * B) trigger $forceUpdate() once a dependency changes, without intermediate updating.
       */
      // A) first immediate dom_update() call with dependencies captured
      this.$dom_data.unwatch1 = this.$watch (() => dom_update_watcher(), nop);
      // B) later, once dependencies changed, $forceUpdate
      dom_update_watcher = this.$forceUpdate.bind (this);
    }
  // dom_destroy
  if (destroying &&
      this.$dom_data.state == 1)	// created
  {
    this.$dom_data.state = 2;		// destroyed
    this.$dom_data.destroying = true;
    this.dom_destroy?.call (this);
    this.$dom_data.unwatch1 = call_unwatch (this.$dom_data.unwatch1);
    this.$dom_data.unwatch2 = call_unwatch (this.$dom_data.unwatch2);
  }
}

/** Vue mixin to provide DOM handling hooks.
 * This mixin adds instance method callbacks to handle dynamic DOM changes
 * such as drawing into a `<canvas/>`.
 * Reactive callback methods have their data dependencies tracked, so future
 * changes to data dependencies of reactive methods will queue future updates.
 * However reactive dependency tracking only works for non-async methods.
 *
 * - dom_create() - Called after `this.$el` has been created
 * - dom_change() - Called after `this.$el` has been reassigned or changed.
 *   Note, may also be called for `v-if="false"` cases.
 * - dom_update() - Reactive callback method, called with a valid `this.$el` and
 *   after Vue component updates. Dependency changes result in `this.$forceUpdate()`.
 * - dom_draw() - Reactive callback method, called during an animation frame, requested
 *   via `dom_queue_draw()`. Dependency changes result in `this.dom_queue_draw()`.
 * - dom_queue_draw() - Cause `this.dom_draw()` to be called during the next animation frame.
 * - dom_destroy() - Callback method, called once `this.$el` is removed.
 */
vue_mixins.dom_updates = {
  beforeCreate: function () {
    console.assert (this.$dom_data == undefined);
    // install $dom_data helper on Vue instance
    this.$dom_data = {
      state: 0,
      destroying: false,
      unwatch1: null,
      unwatch2: null,
      rafid: undefined,
    };
    // Vue3 mixins are fragile, so that mixin.beforeUnmount() is *only* called if the instance
    // does not define beforeUnmount() itself. onBeforeUnmount() seems more reliable
    Vue.onMounted (() => this.$forceUpdate()); // ensure updated()
    Vue.onUpdated (() => dom_dispatch.call (this, false));
    Vue.onBeforeUnmount (() => dom_dispatch.call (this, true));
    function dom_queue_draw () {
      if (this.$dom_data.rafid !== undefined)
	return;
      function dom_draw_frame () {
	this.$dom_data.rafid = undefined;
	if (!this.$dom_data.destroying && this.$el instanceof Element)
	  {
	    this.$dom_data.unwatch2 = call_unwatch (this.$dom_data.unwatch2);
	    let once = 0;
	    const dom_draw_reactive = vm => {
	      if (once++ == 0 && this.dom_draw() instanceof Promise)
		console.warn ('dom_draw() returned Promise, async functions are not reactive', this);
	      return once;  // always change return value and guard against subsequent calls
	    };
	    this.$dom_data.unwatch2 = this.$watch (dom_draw_reactive, this.dom_queue_draw);
	  }
      }
      this.$dom_data.rafid = requestAnimationFrame (dom_draw_frame.bind (this));
    }
    this.dom_queue_draw = dom_queue_draw;
  }, // beforeCreate
  unmounted: function () {
    if (this.$dom_data.rafid !== undefined)
      {
	cancelAnimationFrame (this.$dom_data.rafid);
	this.$dom_data.rafid = undefined;
      }
  },
};

const WEAKOFFSET = 1001;
const weakmaps = { ids: new WeakMap, objs: new Array, counter: WEAKOFFSET };

/// Fetch a unique id for any object.
export function weakid (object) {
  if (!(object instanceof Object))
    return 0;
  let id = weakmaps.ids.get (object);
  if (!id) {
    id = weakmaps.counter++;
    weakmaps.ids.set (object, id);
    weakmaps.objs[id - WEAKOFFSET] = new WeakRef (object);
  }
  return id;
}

/// Find an object from its unique id.
export function weakid_lookup (id) {
  const index = id - WEAKOFFSET;
  const weakref = weakmaps.objs[index];
  if (weakref) {
    const object = weakref.deref();
    if (object)
      return object;
    weakmaps.objs.splice (index, 1);
  }
  return undefined;
}

/// Join strings and arrays of class lists from `args`.
export function join_classes (...args) {
  const cs = new Set();
  for (const arg of args)
    {
      if (Array.isArray (arg))
	{
	  for (const a of arg)
	    if (a !== undefined)
	      cs.add ('' + a);
	}
      else if (arg !== undefined)
	{
	  for (const a of arg.split (/ +/))
	    cs.add (a);
	}
    }
  return cs.size ? Array.from (cs).join (' ') : undefined;
}

/// Assign `obj[k] = v` for all `k of keys, v of values`.
export function object_zip (obj, keys, values) {
  const kl = keys.length, vl = values.length;
  for (let i = 0; i < kl; i++)
    obj[keys[i]] = i < vl ? values[i] : undefined;
  return obj;
}

/// Await and reassign all object fields.
export async function object_await_values (obj) {
  return object_zip (obj, Object.keys (obj), await Promise.all (Object.values (obj)));
}

const empty_list = Object.freeze ([]);

/** Extend Ase.Property objects with cached attributs.
 * Note, for automatic `.value_` updates, a `disconnector` function must be provided
 * as second argument, to handle disconnection of property change notifications once
 * the property is not needed anymore.
 */
export async function extend_property (prop, disconnector = undefined, augment = undefined) {
  prop = await prop;
  if (prop.update_ && prop.fetch_) {
    // already extended
    await prop.update_();
    return prop;
  }
  const xprop = {
    hints_: prop.hints(),
    ident_: prop.identifier(),
    is_numeric_: prop.is_numeric(),
    label_: prop.label(),
    nick_: prop.nick(),
    unit_: prop.unit(),
    group_: prop.group(),
    blurb_: prop.blurb(),
    description_: prop.description(),
    min_: prop.get_min(),
    max_: prop.get_max(),
    step_: prop.get_step(),
    has_choices_: false,
    value_: Vue.reactive ({ val: undefined, num: 0, text: '', choices: empty_list }),
    apply_: prop.set_value.bind (prop),
    fetch_: () => xprop.is_numeric_ ? xprop.value_.num : xprop.value_.text,
    update_: async () => {
      const val = xprop.get_value(), text = xprop.get_text();
      const choices = xprop.has_choices_ ? await xprop.choices() : empty_list;
      const value_ = { val: await val, num: undefined, text: await text, choices };
      value_.num = (value_.val - xprop.min_) / (xprop.max_ - xprop.min_);
      Object.assign (xprop.value_, value_);
      if (augment)
	await augment (xprop);
    },
    __proto__: prop,
  };
  if (disconnector)
    disconnector (xprop.on ('notify', _ => xprop.update_()));
  await object_await_values (xprop);	// ensure xprop.hints_ is valid
  xprop.has_choices_ = xprop.hints_.search (/:choice:/) >= 0;
  await xprop.update_();			// needs xprop.has_choices_, assigns xprop.value_
  return xprop;
}

/// Extract the promise `p` state as one of: 'pending', 'fulfilled', 'rejected'
export function promise_state (p) {
  const t = {}; // dummy, acting as fulfilled
  return Promise.race ([p, t])
		.then (v => v === t ? 'pending' : 'fulfilled',
		       v => 'rejected');
}

/// Turn a JS `$event` handler expression into a function.
/// This yields a factory function that binds the scope to create an expression handler.
export function compile_expression (expression, context) {
  const cache = compile_expression.cache || (compile_expression.cache = new Map());
  let mkfunc = cache.get (expression);
  if (mkfunc) return mkfunc;
  const code = `with ($·S) return function ${name} ($event) { "use strict"; const $·R = ( ${expression} ); return $·R instanceof Function ? $·R ($event) : $·R; }`;
  try {
    mkfunc = new Function ('$·S', code);
  } catch (err) {
    if (context)
      console.warn ("At:", context, "Invalid expression: {{{\n", expression, "\n}}}");
    else
      console.warn ("Invalid expression: {{{\n", expression, "\n}}}");
    console.error ("Failed to compile expression:", err);
    throw new Error (err);
  }
  cache.set (expression, mkfunc);
  return mkfunc;
}

/** VueifyObject - turn a regular object into a Vue instance.
 * The *object* passed in is used as the Vue `data` object. Properties
 * with a getter (and possibly setter) are turned into Vue `computed`
 * properties, methods are carried over as `methods` on the Vue() instance.
 */
export function VueifyObject (object = {}, vue_options = {}) {
  let voptions = Object.assign ({}, vue_options);
  voptions.methods = vue_options.methods || {};
  voptions.computed = vue_options.computed || {};
  voptions.data = voptions.data || object;
  const proto = object.__proto__;
  for (const pname of Object.getOwnPropertyNames (proto)) {
    if (typeof proto[pname] == 'function' && pname != 'constructor')
      voptions.methods[pname] = proto[pname];
    else
      {
	const pd = Object.getOwnPropertyDescriptor (proto, pname);
	if (pd.get)
	  voptions.computed[pname] = pd;
      }
  }
  return new Vue (voptions);
}

/** Copy PropertyDescriptors from source to target, optionally binding handlers against closure. */
export const clone_descriptors = (target, source, closure) => {
  // See also: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/assign completeAssign()
  const descriptors = Object.getOwnPropertyNames (source).reduce ((descriptors, pname) => {
    const pd = Object.getOwnPropertyDescriptor (source, pname);
    if (pname != 'constructor')
      {
	if (closure)
	  {
	    if (pd.get)
	      pd.get = pd.get.bind (closure);
	    if (pd.set)
	      pd.set = pd.set.bind (closure);
	    if (typeof pd.value == 'function')
	      pd.value = pd.value.bind (closure);
	  }
	descriptors[pname] = pd;
      }
    return descriptors;
  }, {});
  Object.defineProperties (target, descriptors);
  // in constrast to Object.assign, we ignore getOwnPropertySymbols here
  return target;
  // Usage: clone_descriptors (window, fakewin.__proto__, fakewin);
};

/** Produce hash code from a String, using an FNV-1a variant. */
export function fnv1a_hash (str) {
  let hash = 0x811c9dc5;
  for (let i = 0; i < str.length; ++i) {
    // Note, charCodeAt can return values > 255
    const next = 0x1000193 * (hash ^ str.charCodeAt (i));
    hash = next >>> 0; // % 4294967296, i.e. treat as 32 bit unsigned
  }
  return hash;
}

/** Split a string when encountering a comma, while preserving quoted or parenthesized segments. */
export function split_comma (str) {
  let result = [], item = '', sdepth = 0, ddepth = 0, pdepth = 0, kdepth = 0, cdepth = 0, bslash = 0;
  for (let i = 0; i < str.length; i++) {
    const c = str[i];
    if (c === ',' && 0 == (sdepth + ddepth + pdepth + kdepth + cdepth)) {
      if (item) {
	result.push (item);
	item = '';
      }
    } else {
      item += c;
      if (c === '[') kdepth++;
      if (c === ']' && kdepth) kdepth--;
      if (c === '(') pdepth++;
      if (c === ')' && pdepth) pdepth--;
      if (c === '{') cdepth++;
      if (c === '}' && cdepth) cdepth--;
      if (c === "'" && !bslash) sdepth = !sdepth;
      if (c === '"' && !bslash) ddepth = !ddepth;
    }
    bslash = !bslash && c === '\\' && (sdepth || ddepth);
  }
  if (item)
    result.push (item);
  return result;
}

/// Properly escape test into `&amp;` and related sequences.
export function escape_html (unsafe) {
  return unsafe.replaceAll ('&', '&amp;').replaceAll ('<', '&lt;').replaceAll ('>', '&gt;').replaceAll ('"', '&quot;').replaceAll("'", '&#039;');
}

/** Parse hexadecimal CSS color with 3 or 6 digits into [ R, G, B ]. */
export function parse_hex_color (colorstr) {
  if (colorstr.substr (0, 1) == '#') {
    let hex = colorstr.substr (1);
    if (hex.length == 3)
      hex = hex[0] + hex[0] + hex[1] + hex[1] + hex[2] + hex[2];
    return [ parseInt (hex.substr (0, 2), 16),
	     parseInt (hex.substr (2, 2), 16),
	     parseInt (hex.substr (4, 2), 16) ];
  }
  return undefined;
}

/** Parse hexadecimal CSS color into luminosity. */
// https://en.wikipedia.org/wiki/Relative_luminance
export function parse_hex_luminosity (colorstr) {
  const rgb = parse_hex_color (colorstr);
  return 0.2126 * rgb[0] + 0.7152 * rgb[1] + 0.0722 * rgb[2];
}

/** Parse hexadecimal CSS color into brightness. */
// https://www.w3.org/TR/AERT/#color-contrast
export function parse_hex_brightness (colorstr) {
  const rgb = parse_hex_color (colorstr);
  return 0.299 * rgb[0] + 0.587 * rgb[1] + 0.114 * rgb[2];
}

/** Parse hexadecimal CSS color into perception corrected grey. */
// http://alienryderflex.com/hsp.html
export function parse_hex_pgrey (colorstr) {
  const rgb = parse_hex_color (colorstr);
  return Math.sqrt (0.299 * rgb[0] * rgb[0] + 0.587 * rgb[1] * rgb[1] + 0.114 * rgb[2] * rgb[2]);
}

/** Parse hexadecimal CSS color into average grey. */
export function parse_hex_average (colorstr) {
  const rgb = parse_hex_color (colorstr);
  return 0.3333 * rgb[0] + 0.3333 * rgb[1] + 0.3333 * rgb[2];
}

/** Parse CSS colors (via invisible DOM element) and yield an array of rgba tuples. */
export function parse_colors (colorstr) {
  let result = [];
  if (!parse_colors.span) {
    parse_colors.span = document.createElement ('span');
    parse_colors.span.style.display = 'none';
    document.body.appendChild (parse_colors.span);
  }
  for (const c of exports.split_comma (colorstr)) {
    parse_colors.span.style.color = c;
    const style = getComputedStyle (parse_colors.span);
    let m = style.color.match (/^rgb\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)$/i);
    if (m)
      result.push ([m[1], m[2], m[3], 1]);
    else {
      m = style.color.match (/^rgba\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*([\d.]+)\s*\)$/i);
      if (m)
	result.push ([m[1], m[2], m[3], m[4]]);
    }
  }
  return result;
}

/** Retrieve a new object with the properties of `obj` resolved against the style of `el` */
export function compute_style_properties (el, obj) {
  const style = getComputedStyle (el);
  let props = {};
  for (let key in obj) {
    const result = style.getPropertyValue (obj[key]);
    if (result !== undefined)
      props[key] = result;
  }
  return props;
}

/** Check if `element` or any parentElement has `display:none` */
export function inside_display_none (element) {
  while (element)
    {
      if (getComputedStyle (element).display == "none")
	return true;
      element = element.parentElement;
    }
  return false;
}

/** Check if `element` is displayed (has width/height assigned) */
export function is_displayed (element) {
  // https://stackoverflow.com/questions/19669786/check-if-element-is-visible-in-dom/33456469#33456469
  if (element.offsetWidth || element.offsetHeight ||
      element.getClientRects().length)
    return true;
  return false;
}

function calculate_scroll_line_height()
{
  // Work around Firefox sending wheel events with ev.deltaMode == DOM_DELTA_LINE, see:
  // https://stackoverflow.com/questions/20110224/what-is-the-height-of-a-line-in-a-wheel-event-deltamode-dom-delta-line/57788612#57788612
  const el = document.createElement ('div');
  el.style.fontSize = 'initial';
  el.style.display = 'none';
  document.body.appendChild (el);
  const fontsize = window.getComputedStyle (el).fontSize;
  document.body.removeChild (el);
  return fontsize ? window.parseInt (fontsize) : 18;
}
let scroll_line_height = undefined;

/** Retrieve normalized scroll wheel event delta in CSS pixels (across Browsers)
 * This returns an object `{x,y}` with negative values pointing
 * LEFT/UP and positive values RIGHT/DOWN respectively.
 * For zoom step interpretation, the x/y pixel values should be
 * reduced via `Math.sign()`.
 * For scales the pixel values might feel more natural, because
 * browsers sometimes increase the number of events with
 * increasing wheel distance, in other cases values are accumulated
 * so fewer events with larger deltas are sent instead.
 */
export function wheel_delta (ev)
{
  const DPR = Math.max (window.devicePixelRatio || 1, 1);
  const DIV_DPR = 1 / DPR;                      // Factor to divide by DPR
  const WHEEL_DELTA = -53 / 120.0 * DIV_DPR;    // Chromium wheelDeltaY to deltaY pixel ratio
  const FIREFOX_Y = 1 / 1.8;                    // Firefox steps are ca 2 times as large as Chrome ones
  const FIREFOX_X = 3 / 1.8;                    // Firefox sets deltaX=1 and deltaY=3 per step on Linux
  const PAGESTEP = -100;                        // Chromium pixels per scroll step
  // https://stackoverflow.com/questions/5527601/normalizing-mousewheel-speed-across-browsers
  // https://groups.google.com/a/chromium.org/forum/#!topic/blink-dev/U3kH6_98BuY
  if (ev.deltaMode >= 2)                        // DOM_DELTA_PAGE
    return { x: ev.deltaX * PAGESTEP, y: ev.deltaY * PAGESTEP };
  if (ev.deltaMode >= 1)                        // DOM_DELTA_LINE - used by Firefox only
    {
      if (scroll_line_height === undefined)
        scroll_line_height = calculate_scroll_line_height();
      return { x: ev.deltaX * FIREFOX_X * scroll_line_height, y: ev.deltaY * FIREFOX_Y * scroll_line_height };
    }
  if (ev.deltaMode >= 0)                        // DOM_DELTA_PIXEL
    {
      const DFIX = 1; // * DIV_DPR;		// old Chromium included DPR
      return { x: DFIX * ev.deltaX, y: DFIX * ev.deltaY };
    }
  if (ev.wheelDeltaX !== undefined)             // Use Chromium factors for normalization
    return { x: ev.wheelDeltaX * WHEEL_DELTA, y: ev.wheelDeltaY * WHEEL_DELTA };
  if (ev.wheelDelta !== undefined)              // legacy support
    return { x: 0, y: ev.wheelDelta / -120 * 10 };
  return { x: 0, y: (ev.detail || 0) * -10 };
}

/** Use deltas from `event` to call scrollBy() on `refs[scrollbars...]`. */
export function wheel2scrollbars (event, refs, ...scrollbars)
{
  const delta = wheel_delta (event);
  for (const sb of scrollbars)
    {
      const scrollbar = refs[sb];
      if (!scrollbar)
	continue;
      if (scrollbar.clientHeight > scrollbar.clientWidth)       // vertical
        scrollbar.scrollBy ({ top: delta.y });
      else                                                      // horizontal
	scrollbar.scrollBy ({ left: delta.x });
    }
}

/** Setup Element shield for a modal containee.
 * Capture focus movements inside `containee`, call `closer(event)` for
 * pointer clicks on `shield` or when `ESCAPE` is pressed.
 */
export function setup_shield_element (shield, containee, closer, capture_escape = true)
{
  const modal_mouse_guard = event => {
    if (!event.cancelBubble && !containee.contains (event.target)) {
      event.preventDefault();
      event.stopPropagation();
      closer (event);
      /* Browsers may emit two events 'mousedown' and 'contextmenu' for button3 clicks.
       * This may cause the shield's owning widget (e.g. a menu) to reappear, because
       * in this mousedown handler we can only prevent further mousedown handling.
       * So we set up a timer that swallows the next 'contextmenu' event.
       */
      swallow_event ('contextmenu', 0);
    }
  };
  shield.addEventListener ('mousedown', modal_mouse_guard);
  Kbd.push_focus_root (containee, capture_escape ? closer : null);
  let undo_shield = () => {
    if (!undo_shield) return;
    undo_shield = null;
    shield.removeEventListener ('mousedown', modal_mouse_guard);
    Kbd.remove_focus_root (containee);
  };
  return undo_shield;
}

/** Stop (immediate) propagation and prevent default handler for an event. */
export function fullstop (event)
{
  event.preventDefault();
  event.stopPropagation();
  event.stopImmediatePropagation();
  return true;
}

/** Use capturing to swallow any `type` events until `timeout` has passed */
export function swallow_event (type, timeout = 0) {
  document.addEventListener (type, fullstop, true);
  setTimeout (() => document.removeEventListener (type, fullstop, true), timeout);
}

/// Prevent default or any propagation for a possible event.
export function prevent_event (event_or_null)
{
  if (!event_or_null || !event_or_null.preventDefault)
    return;
  const event = event_or_null;
  event.preventDefault();
  event.stopPropagation();
  event.stopImmediatePropagation();
}

/// Close dialog on backdrop clicks via hiding at mousedown
function dialog_backdrop_mousedown (ev)
{
  const dialog = this;
  if (!dialog.open || !ev.buttons)
    return;
  // detect click on backdrop area
  if (ev.offsetX < 0 || ev.offsetX > ev.target.offsetWidth ||
      ev.offsetY < 0 || ev.offsetY > ev.target.offsetHeight)
    {
      // just hide the dialog on mousedown
      dialog.__dialog_backdrop_must_close = ev.button;
      // avoid display:none, the dialog still needs to receive events
      dialog.style.setProperty ('visibility', 'hidden', 'important');
      // prevent bubbling back up to cause clicks
      prevent_event (ev);
    }
}

/// Close dialog on backdrop clicks via actual closing at mouseup
function dialog_backdrop_mouseup (ev)
{
  const dialog = this;
  // only handle "clicks" with previous backdrop mousedown
  if (dialog.__dialog_backdrop_must_close !== ev.button)
    return;
  dialog.__dialog_backdrop_must_close = undefined;
  // restore visibility to make the dialog reusable
  dialog.style.removeProperty ('visibility');
  // really close dialog due to backdrop click
  dialog.close();
  // prevent bubbling back up to cause clicks
  prevent_event (ev);
}

/// Install handlers to close a dialog on backdrop clicks.
export function dialog_backdrop_autoclose (dialog, install_or_remove)
{
  console.assert (dialog instanceof HTMLDialogElement);

  // stop events in capture phase to reliably prevent followup clicks
  const capture = { capture: true };

  // closing on mousedown tends to re-open on the following mouseup
  // so closing is deferred until mouseup
  if (install_or_remove)
    {
      dialog.addEventListener ('mousedown', dialog_backdrop_mousedown, capture);
      dialog.addEventListener ('mouseup', dialog_backdrop_mouseup, capture);
    }
  else
    {
      dialog.removeEventListener ('mousedown', dialog_backdrop_mousedown, capture);
      dialog.removeEventListener ('mouseup', dialog_backdrop_mouseup, capture);
    }
}

/** Determine position for a popup */
export function popup_position (element, opts = { origin: undefined, x: undefined, y: undefined,
						  xscale: 0, yscale: 0, })
{
  const p = 1; // padding;
  // Ignore window.innerWidth & window.innerHeight, these include space for scrollbars
  // Viewport size, https://developer.mozilla.org/en-US/docs/Web/CSS/Viewport_concepts
  const vw = document.documentElement.clientWidth, vh = document.documentElement.clientHeight;
  // Scroll offset, needed to convert viewport relative to document relative
  const sx = window.pageXOffset, sy = window.pageYOffset;
  // Element area, relative to the viewport.
  const b = element.getBoundingClientRect(), r = b;
  if (opts.xscale > 0)
    r.width *= opts.xscale;
  if (opts.yscale > 0)
    r.height *= opts.yscale;
  // Position element without an origin element
  if (!opts.origin || !opts.origin.getBoundingClientRect)
    {
      // Position element at document relative (opts.x, opts.y)
      if (opts.x >= 0 && opts.x <= 999999 && opts.y >= 0 && opts.y <= 999999)
	{
	  let vx = Math.max (0, opts.x - sx); // document coord to viewport coord
	  let vy = Math.max (0, opts.y - sy); // document coord to viewport coord
	  // Shift left if neccessary
	  if (vx + r.width + p > vw)
	    vx = vw - r.width - p;
	  // Shift up if neccessary
	  if (vy + r.height + p > vh)
	    vy = vh - r.height - p;
	  return { x: sx + Math.max (0, vx), y: sy + Math.max (0, vy) }; // viewport coords to document coords
	}
      // Center element, but keep top left onscreen
      const vx = (vw - r.width) / 2, vy = (vh - r.height) / 2;
      return { x: sx + Math.max (0, vx), y: sy + Math.max (0, vy) }; // viewport coords to document coords
    }
  // Position element relative to popup origin
  const o = opts.origin.getBoundingClientRect();
  let vx = o.x, vy = o.y + o.height;	// left aligned, below origin
  // Shift left if neccessary
  if (vx + r.width + p > vw)
    vx = vw - r.width - p;
  // Put above or shift up if neccessary
  if (vy + r.height + p > vh)
    {
      vy = o.y - b.height; // place above origin
      if (vy < 0 + p) // place below, but shift up
	vy = Math.min (vh - r.height - p, o.y);
    }
  return { x: sx + Math.max (0, vx), y: sy + Math.max (0, vy) }; // viewport coords to document coords
}

/** Resize canvas display size (CSS size) and resize backing store to match hardware pixels */
export function resize_canvas (canvas, csswidth, cssheight, fill_style = false) {
  /* Here we fixate the canvas display size at (csswidth,cssheight) and then setup the
   * backing store to match the hardware screen pixel size.
   * Note, just assigning canvas.width *without* assigning canvas.style.width may lead to
   * resizes in the absence of other constraints. So to render at screen pixel size, we
   * always have to assign canvas.style.{width|height}.
   */
  const devicepixelratio = window.devicePixelRatio;
  const cw = Math.round (csswidth), ch = Math.round (cssheight);
  const pw = Math.round (devicepixelratio * cw);
  const ph = Math.round (devicepixelratio * ch);
  canvas.style.width = cw + 'px';
  canvas.style.height = ch + 'px';
  canvas.width = pw;
  canvas.height = ph;
  const ctx = canvas.getContext ('2d');
  if (!fill_style || fill_style === true)
    ctx.clearRect (0, 0, canvas.width, canvas.height);
  else {
    ctx.fillStyle = fill_style;
    ctx.fillRect (0, 0, canvas.width, canvas.height);
  }
  return devicepixelratio;
}

/** Draw a horizontal line from (x,y) of width `w` with dashes `d` */
export function dash_xto (ctx, x, y, w, d) {
  for (let i = 0, p = x; p < x + w; p = p + d[i++ % d.length]) {
    if (i % 2)
      ctx.lineTo (p, y);
    else
      ctx.moveTo (p, y);
  }
}

/** Draw a horizontal rect `(x,y,width,height)` with pixel gaps of width `stipple` */
export function hstippleRect (ctx, x, y, width, height, stipple) {
  for (let s = x; s + stipple < x + width; s += 2 * stipple)
    ctx.fillRect (s, y, stipple, height);
}

/** Fill and stroke a canvas rectangle with rounded corners. */
export function roundRect (ctx, x, y, width, height, radius, fill = true, stroke = true) {
  if (typeof radius === 'number')
    radius = [ radius, radius, radius, radius ];
  else if (typeof radius === 'object' && radius.length == 4)
    ; // top-left, top-right, bottom-right, bottom-left
  else
    throw new Error ('invalid or missing radius');
  ctx.beginPath();
  ctx.moveTo           (x + radius[0],         y);
  ctx.lineTo           (x + width - radius[1], y);
  ctx.quadraticCurveTo (x + width,             y,                      x + width,             y + radius[1]);
  ctx.lineTo           (x + width,             y + height - radius[2]);
  ctx.quadraticCurveTo (x + width,             y + height,             x + width - radius[2], y + height);
  ctx.lineTo           (x + radius[3],         y + height);
  ctx.quadraticCurveTo (x,                     y + height,             x,                     y + height - radius[3]);
  ctx.lineTo           (x,                     y + radius[0]);
  ctx.quadraticCurveTo (x,                     y,                      x + radius[0],         y);
  ctx.closePath();
  if (fill)
    ctx.fill();
  if (stroke)
    ctx.stroke();
}

/** Add color stops from `stoparray` to `grad`, `stoparray` is an array: [(offset,color)...] */
export function gradient_apply_stops (grad, stoparray) {
  for (const g of stoparray)
    grad.addColorStop (g[0], g[1]);
}

/** Create a new linear gradient at (x1,y1,x2,y2) with color stops `stoparray` */
export function linear_gradient_from (ctx, stoparray, x1, y1, x2, y2) {
  const grad = ctx.createLinearGradient (x1, y1, x2, y2);
  gradient_apply_stops (grad, stoparray);
  return grad;
}

/** Measure ink span of a canvas text string or an array */
export function canvas_ink_vspan (font_style, textish = 'gM') {
  let canvas, ctx, cwidth = 64, cheight = 64;
  function measure_vspan (text) {
    const cache_key = font_style + ':' + text;
    let result = canvas_ink_vspan.cache[cache_key];
    if (!result)
      {
	if (canvas === undefined) {
	  canvas = document.createElement ('canvas');
	  ctx = canvas.getContext ('2d');
	}
	/* BUG: electron-1.8.3 (chrome-59) is unstable (shows canvas memory
	 * corruption) at tiny zoom levels without canvas size assignments.
	 */
	const text_em = ctx.measureText ("MW").width;
	const twidth = Math.max (text_em * 2, ctx.measureText (text).width);
	cwidth = Math.max (cwidth, 2 * twidth);
	cheight = Math.max (cheight, 3 * text_em);
	canvas.width = cwidth;
	canvas.height = cheight;
	ctx.fillStyle = '#000000';
	ctx.fillRect (0, 0, canvas.width, canvas.height);
	ctx.font = font_style;
	ctx.fillStyle = '#ffffff';
	ctx.textBaseline = 'top';
	ctx.fillText (text, 0, 0);
	const pixels = ctx.getImageData (0, 0, canvas.width, canvas.height).data;
	let start = -1, end = -1;
	for (let row = 0; row < canvas.height; row++)
	  for (let col = 0; col < canvas.width; col++) {
	    let index = (row * canvas.width + col) * 4; // RGBA
	    if (pixels[index + 0] > 0) {
	      if (start < 0)
		start = row;
	      else
		end = row;
	      break;
	    }
	  }
	result = start >= 0 && end >= 0 ? [start, end - start] : [0, 0];
	canvas_ink_vspan.cache[cache_key] = result;
      }
    return result;
  }
  return Array.isArray (textish) ? textish.map (txt => measure_vspan (txt)) : measure_vspan (textish);
}
canvas_ink_vspan.cache = [];

export const PPQN = 4838400;	// ticks per quarter note

/** Retrieve the 'C-1' .. 'G8' label for midi note numbers */
export function midi_label (numish) {
  function one_label (num) {
    const letter = [ 'C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B' ];
    const oct = Math.floor (num / letter.length) - 2;
    const key = num % letter.length;
    return letter[key] + oct;
  }
  return Array.isArray (numish) ? numish.map (n => one_label (n)) : one_label (numish);
}

export function discard_remote (aseobject) {
  console.log ("TODO: Ase: implement discard:", aseobject);
}

/// Align integer value to 8.
function align8 (int) {
  return (int / 8 |0) * 8;
}

function telemetry_field_width (telemetryfieldtype) {
  switch (telemetryfieldtype) {
    // case 'i64': return 8;
    case 'i8':	return 1;
    case 'i32':	return 4;
    case 'f32':	return 4;
    case 'f64':	return 8;
    default:    return 0;
  }
}

// Handle incoming binary data, setup by startup.js
export function jsonipc_binary_handler_ (arraybuffer) {
  if (telemetry_blocked)
    return;
  const arrays = {
    // i64:	new BigInt64Array (arraybuffer, 0, arraybuffer.byteLength / 8 |0),
    i8:		new Int8Array     (arraybuffer, 0, arraybuffer.byteLength),
    i32:	new Int32Array    (arraybuffer, 0, arraybuffer.byteLength / 4 |0),
    f32:	new Float32Array  (arraybuffer, 0, arraybuffer.byteLength / 4 |0),
    f64:	new Float64Array  (arraybuffer, 0, arraybuffer.byteLength / 8 |0),
  };
  for (const object of telemetry_objects) {
    const callback = object[".telemetry_callback"];
    callback (object, arrays);
  }
}
let telemetry_blocked = 0;

const telemetry_objects = []; // pointers into telemetry buffer
let telemetry_segments = [];  // sorted, non-overlapping request list

// re-request telemetry segments
function telemetry_reschedule() {
  const fragments = [], segments = [];
  // collect and sort fragments
  for (const object of telemetry_objects)
    for (const fragment in object)
      if (object[fragment].byteoffset >= 0)
	fragments.push (object[fragment]);
  fragments.sort (function (a, b) {
    if (a.byteoffset != b.byteoffset)
      return a.byteoffset > b.byteoffset ? +1 : -1;
    if (a.bytelength != b.bytelength)
      return a.bytelength > b.bytelength ? -1 : +1;
    return 0;
  });
  // add or extend segments
  let byteindex = 0; // number of bytes in telemetry_segments before last
  for (const f of fragments) {
    let last = segments[segments.length - 1];
    if (!segments.length || f.byteoffset >= last.offset + last.length + 8) {
      byteindex += last ? last.length : 0;
      segments.push ({ offset: align8 (f.byteoffset), length: align8 (f.bytelength + 7) });
      last = segments[segments.length - 1];
    } // due to alignment possible: offset+length < f.byteoffset+f.bytelength
    if (f.byteoffset + f.bytelength > last.offset + last.length)
      last.length = align8 (f.byteoffset + f.bytelength - last.offset + 7);
    // translate byteoffset into telemetry delivery
    f.index = (byteindex + (f.byteoffset - last.offset)) * f.factor |0;
  }
  if (!equals_recursively (telemetry_segments, segments)) {
    telemetry_segments = segments;
    (async () => {
      telemetry_blocked++;
      const result = await Ase.server.broadcast_telemetry (telemetry_segments, 32);
      telemetry_blocked--;
      if (!result)
	throw Error ("telemetry_reschedule: invalid segments: " + JSON.stringify (telemetry_segments));
    }) ();
  }
}

/// Call `fun` for telemtry updates, returns unsubscribe handler.
export function telemetry_subscribe (fun, telemetryfields) {
  if (telemetryfields.length < 1)
    return null;
  const telemetryobject = {};
  for (const field of telemetryfields)
    {
      if (!field.name)
	throw Error ("telemetry_subscribe: missing field name: " + field);
      const width = telemetry_field_width (field.type);
      if (!width)
	throw Error ("telemetry_subscribe: invalid type: " + field.type);
      const factor = 1.0 / width;
      const fragment = {
	index: -1,
	type: field.type,
	factor,
	length: field.length * factor |0,
	byteoffset: field.offset,
	bytelength: field.length,
      };
      if ((fragment.byteoffset / width |0) != fragment.byteoffset / width)
	throw Error ("telemetry_subscribe: invalid alignment: " + field.offset + "/" + width);
      if ((fragment.bytelength / width |0) != fragment.bytelength / width)
	throw Error ("telemetry_subscribe: invalid alignment: " + field.length + "/" + width);
      telemetryobject[field.name] = fragment;
    }
  Object.defineProperty (telemetryobject, ".telemetry_callback", { value: fun });
  telemetry_objects.push (telemetryobject);
  telemetry_reschedule();
  return telemetryobject;
}

/// Call `fun` for telemtry updates, returns unsubscribe handler.
export function telemetry_unsubscribe (telemetryobject) {
  if (array_remove (telemetry_objects, telemetryobject)) {
    telemetry_reschedule();
    return true;
  }
  return false;
}

// Format window titles
export function format_title (prgname, entity = undefined, infos = undefined, extras = undefined) {
  let title = prgname;
  if (entity)
    {
      let sub = entity;
      if (infos)
	{
	  sub += ' - ' + infos;
	  if (extras)
	    sub += ' (' + extras + ')';
	}
      title = sub + ' – ' + title;
    }
  return title;
}

let keyboard_click_state = { inclick: 0 };

/// Check if the current click event originates from keyboard activation.
export function in_keyboard_click()
{
  return keyboard_click_state.inclick > 0;
}

/// Retrieve event that triggers keyboard_click().
export function keyboard_click_event (fallback = undefined) {
  return keyboard_click_current_event[0] || fallback;
}
const keyboard_click_current_event = [ undefined ];

/// Trigger element click via keyboard.
export function keyboard_click (element, event, callclick = true)
{
  if (element instanceof Element)
    {
      keyboard_click_state.inclick += 1;
      if (!element.classList.contains ('active'))
	{
	  const e = element;
	  e.classList.toggle ('active', true);
	  if (!e.keyboard_click_reset_id)
	    e.keyboard_click_reset_id = setTimeout (() => {
	      e.keyboard_click_reset_id = undefined;
	      e.classList.toggle ('active', false);
	    }, 170); // match focus-activation delay
	}
      if (callclick) {
	keyboard_click_current_event.unshift (event);
	element.click (event);
	keyboard_click_current_event.shift();
      }
      keyboard_click_state.inclick -= 1;
      return true;
    }
  return false;
}

/** Check whether `element` is contained in `array` */
export function in_array (element, array)
{
  console.assert (array instanceof Array);
  return array.indexOf (element) >= 0;
}

/** Check whether `element` is found during `for (... of iteratable)` */
export function matches_forof (element, iteratable)
{
  for (let item of iteratable)
    if (element === item)
      return true;
  return false;
}

/// Extract filtered text nodes from Element.
export function element_text (element, filter)
{
  let texts = [];
  const each = e => {
    if (e.nodeType == 3) // text element
      {
	texts.push (e.data);
	return;
      }
    if (filter && filter (e) == false)
      return;
    for (const c of e.childNodes)
      each (c);
  };
  each (element);
  return texts.join ('');
}

/// Clone a menuitem icon via its `uri`.
export function clone_menu_icon (menu, uri, title = '')
{
  const menuitem = menu?.find_menuitem (uri);
  if (!menuitem)
    return {};
  App.zmove (); // pick up 'data-tip'
  return {
    ic: menuitem.ic, fa: menuitem.fa, mi: menuitem.mi, bc: menuitem.bc, uc: menuitem.uc,
    'data-kbd': menuitem.kbd,
    'data-tip': title ? title + ' ' + menuitem.slot_label() : '',
  };
}

// https://github.com/WICG/keyboard-map/blob/master/explainer.md
let keyboard_map = {
  get (key) {
    return key.startsWith ('Key') ? key.substr (3).toLowerCase() : key;
  },
};
async function refresh_keyboard_map () {
  if (navigator && navigator.keyboard && navigator.keyboard.getLayoutMap)
    keyboard_map = await navigator.keyboard.getLayoutMap();
}
(() => {
  refresh_keyboard_map();
  if (navigator && navigator.keyboard && navigator.keyboard.addEventListener)
    navigator.keyboard.addEventListener ("layoutchange", () => refresh_keyboard_map());
}) ();

/// Retrieve user-printable name for a keyboard button, useful to describe KeyboardEvent.code.
export function keyboard_map_name (keyname) {
  const name = keyboard_map.get (keyname);
  return name || keyname;
}

/// Check if `ancestor` is an ancestor of `node`, maybe including shadowRoot elements.
export function has_ancestor (node, ancestor, escape_shadowdom = true)
{
  while (node)
    {
      if (node === ancestor)
	return true;
      if (escape_shadowdom && !node.parentNode &&
	  node.nodeType == 11 && node.host) // shadowRoot fragment
	node = node.host;
      else
	node = node.parentNode;
    }
  return false;
}

/// Find the closest element or parent matching `selector`, traversing shadow DOMs.
export function closest (element, selector)
{
  while (element)
    {
      if (element.matches (selector))
	return element;
      if (!element.parentElement && element.parentNode &&
	  element.parentNode.nodeType == 11 && // shadowRoot fragment
	  element.parentNode.host)
	element = element.parentNode.host;
      else
	element = element.parentElement;
    }
  return null;
}

/** Retrieve root ancestor of `element` */
export function root_ancestor (element) {
  while (element && element.parentNode)
    element = element.parentNode;
  return element;
}

/// Find an element at `(x,y)` for which `predicate (element)` is true.
export function find_element_from_point (root, x, y, predicate, visited = new Set())
{
  for (const el of root.elementsFromPoint (x, y))
    {
      if (visited.has (el))
	continue;
      if (predicate (el))
	return el;
      visited.add (el);
      if (el.shadowRoot)
	{
	  const sub = find_element_from_point (el.shadowRoot, x, y, predicate, visited);
	  if (sub)
	    return sub;
	}
    }
  return null;
}

/** Show a notification popup, with adequate default timeout */
export function create_note (text, timeout = undefined) {
  return App.shell.create_note (text, timeout);
}

/** Generate `element.innerHTML` from `markdown_text` */
export function markdown_to_html (element, markdown_text) {
  const MarkdownIt = require ('markdown-it');
  // configure Markdown generator
  const config = { linkify: true };
  const md = new MarkdownIt (config);
  // add target=_blank to all links
  const orig_link_open = md.renderer.rules.link_open || function (tokens, idx, options, env, self) {
    return self.renderToken (tokens, idx, options); // default renderer
  };
  md.renderer.rules.link_open = function (tokens, idx, options, env, self) {
    const aIndex = tokens[idx].attrIndex ('target'); // attribute could be present already
    if (aIndex >= 0)
      tokens[idx].attrs[aIndex][1] = '_blank';       // override when present
    else
      tokens[idx].attrPush (['target', '_blank']);   // or add new attribute
    return orig_link_open (tokens, idx, options, env, self); // resume
  };
  // render HTML
  const html = md.render (markdown_text);
  element.classList.add ('b-markdown-it-outer');
  element.innerHTML = html;
}

/** Assign `map[key] = cleaner`, while awaiting and calling any previously existing cleanup function */
export function assign_async_cleanup (map, key, cleaner) {
  const oldcleaner = map[key];
  if (oldcleaner === cleaner)
    return;
  // turn each cleaner Function into a unique Function object for `===` comparisons
  map[key] = cleaner instanceof Function ? (() => cleaner()) : cleaner;
  if (cleaner instanceof Promise)
    {
      // asynchronously await and assign cleaner function
      (async () => {
	let cleanupfunc = await cleaner;
	if (cleanupfunc)
	  console.assert (cleanupfunc instanceof Function);
	else
	  cleanupfunc = undefined;
	if (map[key] === cleaner) // resolve promise
	  map[key] = () => cleanupfunc();
	else if (cleanupfunc)
	  cleanupfunc(); // promise has been discarded, invoke cleanup
      }) ();
    }
  if (oldcleaner && !(oldcleaner instanceof Promise))
    oldcleaner();
}

/** Method to be added to a `observable_from_getters()` result to force updates. */
export function observable_force_update () {
  // This method works as a tag for observable_from_getters()
}

/** Create a reactive dict from the fields in `tmpl` with async callbacks.
 *
 * Once the resolved result from `predicate()` changes and becomes true-ish, the
 * `getter()` of each field in `tmpl` is called, resolved and assigned to the
 * corresponding field in the observable binding returned from this function.
 * Optionally, fields may provide a `notify` setup handler to install a
 * notification callback that re-invokes the `getter`.
 * A destructor can be returned from `notify()` once resolved, that is executed
 * during cleanup phases.
 * The `default` of each field in `tmpl` may provide an initial value before
 * `getter` is called the first time and in case `predicate()` becomes false-ish.
 * The first argument to `getter()` is a function that can be used to register
 * cleanup code for the getter result.
 *
 * ```js
 * const data = {
 *   val: { getter: c => async_fetch(), notify: n => add_listener (n), },
 * };
 * dict = this.observable_from_getters (data, () => this.predicate());
 * // use dict.val
 * ```
 *
 * When the `n()` callback is called, a new *getter* call is scheduled.
 * A handler can be registered with `c (cleanup);` to cleanup resources
 * left over from an `async_fetch()` call.
 */
export function observable_from_getters (tmpl, predicate) {
  const monitoring_getters = [];
  const getter_cleanups = {};
  const notify_cleanups = {};
  let add_functions = false;
  let rdata; // Vue.reactive
  for (const key in tmpl)
    {
      if (tmpl[key] instanceof Function)
	{
	  add_functions = true;
	  continue;
	}
      else if (!(tmpl[key] instanceof Object))
	continue;
      const async_getter = tmpl[key].getter, async_notify = tmpl[key].notify, default_value = tmpl[key].default;
      tmpl[key] = default_value;
      if (!async_getter && !async_notify)
	continue;
      const assign_getter_cleanup = (c) => assign_async_cleanup (getter_cleanups, key, c);
      const getter = async () => {
	const had_cleanup = !!getter_cleanups[key];
	const result = !async_getter ? default_value :
		       await async_getter (assign_getter_cleanup);
	if (had_cleanup || getter_cleanups[key])
	  rdata[key] = result; // always reassign if cleanups are involved
	else if (!equals_recursively (rdata[key], result))
	  rdata[key] = result; // compare to reduce Vue updates
      };
      const getter_and_listen = (reset) => {
	if (reset) // if reset==true, getter() might not be callable
	  {
	    if (async_notify)
	      assign_async_cleanup (notify_cleanups, key, undefined);
	    if (getter_cleanups[key])
	      assign_async_cleanup (getter_cleanups, key, undefined);
	    rdata[key] = default_value;
	  }
	else
	  {
	    if (async_notify) { // sets up listener
	      let getter_pending = null;
	      const debounced_getter = async () => {
		if (getter_pending)
		  return; // debug ("debouncing async getter:", key);
		await new Promise (r => getter_pending = setTimeout (() => r(), 17));
		getter_pending = null;
		getter();
	      };
	      assign_async_cleanup (notify_cleanups, key, async_notify (debounced_getter));
	    }
	    getter ();
	  }
      };
      monitoring_getters.push (getter_and_listen);
    }
  // make all fields observable
  rdata = Vue.reactive (tmpl);
  // cleanup notifiers and getter results on `unmounted`
  const run_cleanups = () => {
    for (const key in notify_cleanups)
      assign_async_cleanup (notify_cleanups, key, undefined);
    for (const key in getter_cleanups)
      assign_async_cleanup (getter_cleanups, key, undefined);
  };
  Vue.onUnmounted (run_cleanups); // TODO: check invocation
  // create trigger for forced updates
  let ucount;
  // install tmpl functions
  if (add_functions)
    {
      let updater;
      for (const key in tmpl)
	{
	  if (tmpl[key] == observable_force_update)
	    {
	      if (!updater)
		{
		  ucount = Vue.reactive ({ c: 1 }); // reactive update counter
		  updater = function () { ucount.c += 1; }; // forces observable update
		}
	      rdata[key] = updater;      // add method to force updates
	    }
	  else if (tmpl[key] instanceof Function)
	    rdata[key] = tmpl[key];
	}
    }
  // create watch, triggering the getters if predicate turns true
  //const dummy = [undefined];
  Vue.watchEffect (async () => {
    const r = (ucount ? ucount.c : 1) && await predicate.call (this);
    const reset = !r;
    monitoring_getters.forEach (getter_and_listen => getter_and_listen (reset));
  });
  return rdata;
}

/** Join template literal arguments into a String */
export function tmplstr (a, ...e) {
  return a.map ((s, i) => s + String (e[i] || '')).join ('');
}

/** Pad `string` with `fill` until its length is `len` */
export function strpad (string, len, fill = ' ') {
  if (typeof string !== 'string')
    string = '' + string;
  if (string.length < len)
    {
      const d = len - string.length;
      while (fill.length < d)
	fill = fill + fill;
      string = fill.substr (0, d) + string;
    }
  return string;
}

export function zero_pad (string, n = 2) {
  string = String (string);
  while (string.length < n)
    string = '0' + string;
  return string;
}

/// Strip whitespace from start and end of string.
export function lrstrip (str)
{
  return str.replaceAll (/^[\s\t\f]+|[\s\t\f]+$/g, '');
}

/// Gather text content from `node_or_array`.
export function collect_text_content (node_or_array)
{
  const asarray = Array.isArray (node_or_array) ? node_or_array : node_or_array ? [ node_or_array ] : [];
  let ctext = '';
  for (const n of asarray)
    {
      const iter = document.createNodeIterator (n, NodeFilter.SHOW_TEXT);
      let textnode;
      while ( (textnode = iter.nextNode()) )
	ctext += textnode.textContent;
    }
  return ctext;
}

export function fmt_date (datelike) {
  const a = new Date (datelike);
  const y = zero_pad (a.getFullYear(), 4);
  const m = zero_pad (a.getMonth(), 2);
  const d = zero_pad (a.getDay(), 2);
  return d + '-' + m + '-' + y;
}

const default_seed = Math.random() * 4294967295 >>> 0;

/// Generate 53-Bit hash from `key`.
export function hash53 (key, seed = default_seed) {
  // https://stackoverflow.com/questions/7616461/generate-a-hash-from-string-in-javascript/52171480#52171480
  let h1 = 0xcc9e2d51 ^ seed, h2 = 0x1b873593 ^ seed;
  for (let i = 0; i < key.length; i++)
    {
      const ch = key.charCodeAt (i);
      h1 = Math.imul (h1 ^ ch, 2654435761);
      h2 = Math.imul (h2 ^ ch, 1597334677);
    }
  h1 = Math.imul (h1 ^ h2 >>> 16, 0x85ebca6b) ^ Math.imul (h2 ^ h1 >>> 13, 0xc2b2ae35);
  h2 = Math.imul (h2 ^ h1 >>> 16, 0x85ebca6b) ^ Math.imul (h1 ^ h2 >>> 13, 0xc2b2ae35);
  return 0x100000000 * (0x1fffff & h2) + (h1 >>> 0);
}

// Raster pixel line between two points
export function raster_line (x0, y0, x1, y1)
{
  x0 = x0 | 0; y0 = y0 | 0; // force integer
  x1 = x1 | 0; y1 = y1 | 0; // force integer
  // https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
  const dx = +Math.abs (x1 - x0), sx = Math.sign (x1 - x0);
  const dy = -Math.abs (y1 - y0), sy = Math.sign (y1 - y0);
  const points = [ [x0,y0] ];
  let fract = dx + dy;
  while (x0 != x1 || y0 != y1)
    {
      const f2 = 2 * fract;
      if (f2 < dx) {
	fract += dx;
	y0 += sy;
      }
      if (f2 > dy) {
	fract += dy;
	x0 += sx;
      }
      points.push ([x0,y0]);
    }
  return points;
}


const destroycallbacks = Symbol ('call_destroy_callbacks');

/// Add a `callback` to `this` to be called from `call_destroy_callbacks()`.
export function add_destroy_callback (callback)
{
  console.assert (this instanceof Object);
  console.assert (callback instanceof Function);
  const callbacks = this[destroycallbacks] || (this[destroycallbacks] = []);
  const index = callbacks.indexOf (callback);
  if (index < 0)
    callbacks.push (callback);
}

/// Remove a `callback` from `this`, previously added via `add_destroy_callback()`.
export function del_destroy_callback (callback)
{
  console.assert (this instanceof Object);
  console.assert (callback instanceof Function);
  const callbacks = this[destroycallbacks];
  return callbacks && array_remove (callbacks, callback);
}

/// Call destroy callbacks of `this`, clears the callback list.
export function call_destroy_callbacks()
{
  console.assert (this instanceof Object);
  const callbacks = this[destroycallbacks];
  if (!callbacks)
    return false;
    while (callbacks.length)
      callbacks.pop().call();
  return true;
}

// Provide keyboard handling utilities
export * from './kbd.js';

// Provide wrapper utilities
export * from './wrapper.js';
