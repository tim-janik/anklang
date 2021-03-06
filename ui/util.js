// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
'use strict';

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

class PointerDrag {
  constructor (vuecomponent, event) {
    this.vm = vuecomponent;
    this.el = event.target;
    this.pointermove = this.pointermove.bind (this);
    this.el.addEventListener ('pointermove', this.pointermove);
    this.pointerup = this.pointerup.bind (this);
    this.el.addEventListener ('pointerup', this.pointerup);
    this.pointercancel = this.pointercancel.bind (this);
    this.el.addEventListener ('pointercancel', this.pointercancel);
    this.el.addEventListener ('lostpointercapture', this.pointercancel);
    this.start_stamp = event.timeStamp;
    try {
      this.el.setPointerCapture (event.pointerId);
      this._pointerid = event.pointerId;
    } catch (e) {
      // something went wrong, bail out the drag
      this._disconnect (event);
    }
    if (this.el)
      this.vm.drag_event (event, START);
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
	this.vm.drag_event (event, CANCEL);
	this.el = null;
      }
    this.vm = null;
  }
  pointermove (event) {
    if (this.el)
      this.vm.drag_event (event, MOVE);
    event.preventDefault();
    event.stopPropagation();
    this.seen_move = true;
  }
  pointerup (event) {
    if (!this.el)
      return;
    this._disconnect (event);
    this.vm.drag_event (event, STOP);
    this.destroy (event);
    event.preventDefault();
    event.stopPropagation();
  }
  pointercancel (event) {
    if (!this.el)
      return;
    this._disconnect (event);
    this.vm.drag_event (event, CANCEL);
    this.destroy (event);
    event.preventDefault();
    event.stopPropagation();
  }
}

/** Start `drag_event (event)` handling on a Vue component's element, use `@pointerdown="Util.drag_event"` */
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

// Maintain pending_pointer_lock state
function pointer_lock_changed (ev) {
  if (document.pointerLockElement !== pending_pointer_lock)
    {
      // grabbing the lock did not work
      pending_pointer_lock = null;
      // exit erroneous pointer lock
      if (document.pointerLockElement &&
	  document.pointerLockElement.__unrequest_pointer_lock)
	document.pointerLockElement.__unrequest_pointer_lock();
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
  // this API only operates on elements that have the __unrequest_pointer_lock member set
  if (element.__unrequest_pointer_lock)
    {
      if (pending_pointer_lock === element)
	pending_pointer_lock = null;
      if (document.pointerLockElement === element)
	document.exitPointerLock();
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
  if (has_pointer_lock (element) && element.__unrequest_pointer_lock)
    return element.__unrequest_pointer_lock;
  // this API only operates on elements that have the __unrequest_pointer_lock member set
  element.__unrequest_pointer_lock = () => unrequest_pointer_lock (element);
  pending_pointer_lock = element;
  if (document.pointerLockElement != element)
    {
      if (document.pointerLockElement)
	document.exitPointerLock();
      pending_pointer_lock.requestPointerLock();
    }
  return element.__unrequest_pointer_lock;
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

/** Remove element `item` from `array` */
export function array_remove (array, item) {
  for (let i = 0; i < array.length; i++)
    if (item === array[i]) {
      array.splice (i, 1);
      break;
    }
  return array;
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

/** Create a new object that has the same properties and Array values as `src` */
export function copy_recursively (src = {}) {
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
		o[k] = v instanceof Object && k !== '__proto__' ? copy_recursively (v) : v;
	    }
	  else if (v instanceof Object)         // Object cell
	    o[k] = copy_recursively (v);
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
	    o[k] = copy_recursively (v);
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
    const c = copy_recursively (a); console.assert (eqr (a, c)); c[6].q = 'q'; console.assert (!eqr (a, c));
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

/** The V-INLINEBLUR directive guides focus for inline editing.
 * A Vue directive to notify and force blur on Enter or Escape.
 * The directive value must evaluate to a callable function for notifications.
 * For inputs that use `onchange` handlers, the edited value should be
 * discarded if the `cancelled` property is true.
 */
vue_directives['inlineblur'] = {
  created (el, binding, vnode) {
    //debug ("inlineblur", "created", el, binding, vnode);
  },
  beforeMount (el, binding, vnode) {
    //debug ("inlineblur", "beforeMount");
    if (binding.value && typeof binding.value !== 'function')
      console.warn ('[Vue-v-inlineblur:] wrong type argument, function required:', binding.expression);
    el.cancelled = false;
  },
  mounted (el, binding, vnode) {
    // const vm = binding.instance;
    console.assert (document.body.contains (el));
    el.focus();
    if (el == document.activeElement)
      {
	if (binding.value)
	  {
	    el._v_inlineblur_watch_blur = function (event) {
	      binding.value.call (this, event);
	    };
	    el.addEventListener ('blur', el._v_inlineblur_watch_blur, true);
	  }
	const ignorekeys = 'ignorekeys' in binding.modifiers;
	if (!ignorekeys)
	  {
	    el._v_inlineblur_watch_keydown = function (event) {
	      const esc = Util.match_key_event (event, 'Escape');
	      if (esc)
		el.cancelled = true;
	      if (esc || Util.match_key_event (event, 'Enter'))
		el.blur();
	    };
	    el.addEventListener ('keydown', el._v_inlineblur_watch_keydown);
	  }
      }
    else if (binding.value)
      {
	el.cancelled = true;
	binding.value.call (this, new CustomEvent ('cancel', { detail: 'blur' }));
      }
  },
  beforeUpdate (el, binding, vnode, prevVnode) {
    //debug ("inlineblur", "beforeUpdate");
  },
  updated (el, binding, vnode, prevVnode) {
    //debug ("inlineblur", "updated");
  },
  beforeUnmount (el, binding, vnode) {
    //debug ("inlineblur", "beforeUnmount");
  },
  unmounted (el, binding, vnode) {
    //debug ("inlineblur", "unmounted");
    if (el._v_inlineblur_watch_blur)
      {
	el.removeEventListener ('blur', el._v_inlineblur_watch_blur, true);
	el._v_inlineblur_watch_blur = undefined;
      }
    if (el._v_inlineblur_watch_keydown)
      {
	el.removeEventListener ('keydown', el._v_inlineblur_watch_keydown);
	el._v_inlineblur_watch_keydown = undefined;
      }
  }
};

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

// Call dom_create, dom_destroy, watch dom_update, then forceUpdate on changes.
function dom_dispatch() {
  const el_valid = this.$el instanceof Element || this.$el instanceof Text;
  const destroying = this.$dom_data.destroying;
  // dom_create
  if (el_valid && this.$dom_data.state == 0 && !destroying) {
    this.$dom_data.state = 1;
    this.dom_create?.call (this);
  }
  /* A note on $watch. Its `expOrFn` is called immediately, the dependencies and return value
   * are recorded. Later, once a dependency changes, its `expOrFn` is called again, also
   * recording the dependencies and return value. If the return value changes, `callback`
   * is invoked. What we need for updating DOM elements, is:
   * a: the initial call with dependency recording which we use for (expensive) updating,
   * b: trigger $forceUpdate() once a dependency changes, without intermediate updating.
   */
  // dom_update
  if (el_valid && this.$dom_data.state == 1 && !destroying) {
    this.$dom_data.unwatch1 = call_unwatch (this.$dom_data.unwatch1); // clear old $watch
    if (this.dom_update) {
      let domupdate_watcher = () => {
	const r = this.dom_update(); // capture dependencies
	if (r instanceof Promise)
	  console.warn ('The this.dom_update() method returned a Promise, async functions are not reactive', this);
	return r;
      };
      this.$dom_data.unwatch1 = this.$watch (() => domupdate_watcher(), nop);
      domupdate_watcher = this.$forceUpdate.bind (this); // dependencies changed
    }
  }
  // dom_destroy
  if (this.$dom_data.state == 1 && !el_valid) {
    this.$dom_data.unwatch1 = call_unwatch (this.$dom_data.unwatch1);
    this.$dom_data.state = 0;
    this.dom_destroy?.call (this);
  }
}

/** Vue mixin to provide DOM handling hooks.
 * This mixin adds instance method callbacks to handle dynamic DOM changes
 * such as drawing into a `<canvas/>`.
 * Reactive callback methods have their data dependencies tracked, so future
 * changes to data dependencies of reactive methods will queue future updates.
 * However reactive dependency tracking only works for non-async methods.
 *
 * - dom_create() - Reactive callback method, called after `this.$el` has been created
 * - dom_update() - Reactive callback method, called after `this.$el` has been created
 *   and after Vue component updates. Dependency changes result in `this.$forceUpdate()`.
 *   Note, this is also called for `v-if="false"` cases, where `Vue.updated()` is not called.
 * - dom_draw() - Reactive callback method, called during an animation frame, requested
 *   via `dom_queue_draw()`. Dependency changes result in `this.dom_queue_draw()`.
 * - dom_queue_draw() - Cause `this.dom_draw()` to be called during the next animation frame.
 * - dom_destroy() - Callback method, called once `this.$el` was removed.
 */
vue_mixins.dom_updates = {
  beforeCreate: function () {
    console.assert (this.$dom_data == undefined);
    // install $dom_data helper on Vue instance
    this.$dom_data = {
      state: 0,
      destroying: false,
      // animateplaybackclear: undefined,
      unwatch1: null,
      unwatch2: null,
      rafid: undefined,
    };
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
  mounted: function () {
    if (this.$dom_data)
      this.$forceUpdate();	// ensure updated()
  },
  updated: function () {
    if (this.$dom_data)
      dom_dispatch.call (this);
  },
  beforeUnmount: function () {
    if (!this.$dom_data)
      return;
    this.$dom_data.destroying = true;
    this.$dom_data.unwatch1 = call_unwatch (this.$dom_data.unwatch1);
    this.$dom_data.unwatch2 = call_unwatch (this.$dom_data.unwatch2);
    this.dom_trigger_animate_playback (false);
    dom_dispatch.call (this);
  },
  unmounted: function () {
    if (this.$dom_data.rafid !== undefined)
      {
	cancelAnimationFrame (this.$dom_data.rafid);
	this.$dom_data.rafid = undefined;
      }
  },
  methods: {
    dom_trigger_animate_playback (flag) {
      if (flag === undefined)
	return !!this.$dom_data.animateplaybackclear;
      if (flag && !this.$dom_data.animateplaybackclear)
	{
	  console.assert (this.dom_animate_playback);
	  this.$dom_data.animateplaybackclear = Util.add_frame_handler (this.dom_animate_playback.bind (this));
	}
      else if (!flag && this.$dom_data.animateplaybackclear)
	{
	  this.$dom_data.animateplaybackclear();
	  this.$dom_data.animateplaybackclear = undefined;
	}
    },
  },
};

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

/// Extract the promise `p` state as one of: 'pending', 'fulfilled', 'rejected'
export function promise_state (p) {
  const t = {}; // dummy, acting as fulfilled
  return Promise.race ([p, t])
		.then (v => v === t ? 'pending' : 'fulfilled',
		       v => 'rejected');
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
 * while Firefox tends to increase the number of events with
 * increasing wheel distance, Chromium tends to accumulate and
 * send fewer events with higher values instead.
 */
export function wheel_delta (ev)
{
  const DPR = Math.max (window.devicePixelRatio || 1, 1);
  const DIV_DPR = 1 / DPR;                      // Factor to divide by DPR
  const WHEEL_DELTA = -53 / 120.0 * DIV_DPR;    // Chromium wheelDeltaY to deltaY pixel ratio
  const FIREFOX_X = 3;                          // Firefox sets deltaX=1 and deltaY=3 per step on Linux
  const PAGESTEP = -100;                        // Chromium pixels per scroll step
  // https://stackoverflow.com/questions/5527601/normalizing-mousewheel-speed-across-browsers
  // https://groups.google.com/a/chromium.org/forum/#!topic/blink-dev/U3kH6_98BuY
  if (ev.deltaMode >= 2)                        // DOM_DELTA_PAGE
    return { x: ev.deltaX * PAGESTEP, y: ev.deltaY * PAGESTEP };
  if (ev.deltaMode >= 1)                        // DOM_DELTA_LINE - only Firefox is known to send this
    {
      if (scroll_line_height === undefined)
        scroll_line_height = calculate_scroll_line_height();
      return { x: ev.deltaX * FIREFOX_X * scroll_line_height, y: ev.deltaY * scroll_line_height };
    }
  if (ev.deltaMode >= 0)                        // DOM_DELTA_PIXEL - Chromium includes DPR
    return { x: ev.deltaX * DIV_DPR, y: ev.deltaY * DIV_DPR };
  if (ev.wheelDeltaX !== undefined)             // Use Chromium factors for normalization
    return { x: ev.wheelDeltaX * WHEEL_DELTA, y: ev.wheelDeltaY * WHEEL_DELTA };
  if (ev.wheelDelta !== undefined)              // legacy support
    return { x: 0, y: ev.wheelDelta / -120 * 10 };
  return { x: 0, y: (ev.detail || 0) * -10 };
}

/** Use deltas from `event` to call scrollBy() on `refs[scrollbars...]`. */
export function wheel2scrollbars (event, refs, ...scrollbars)
{
  const delta = Util.wheel_delta (event);
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

/** List all elements that can take focus and are descendants of `element` or the document. */
export function list_focusables (element)
{
  if (!element)
    element = document.body;
  const candidates = [
    'a[href]',
    'audio[controls]',
    'button',
    'input:not([type="radio"]):not([type="hidden"])',
    'select',
    'textarea',
    'video[controls]',
    '[contenteditable]:not([contenteditable="false"])',
    '[tabindex]',
  ];
  const excludes = ':not([disabled]):not([tabindex="-1"])';
  const candidate_selector = candidates.map (e => e + excludes).join (', ');
  const nodes = element.querySelectorAll (candidate_selector); // selector for focusable elements
  const array1 = [].slice.call (nodes);
  // filter out non-tabable elements
  const array = array1.filter (element => {
    if (element.offsetWidth <= 0 &&     // browsers can focus 0x0 sized elements
	element.offsetHeight <= 0 &&
	inside_display_none (element))  // but not within display:none
      return false;
    return true;
  });
  return array;
}

/** Check if `element` has inner input navigation */
export function is_nav_input (element) {
  const nav_elements = [
    "AUDIO",
    "SELECT",
    "TEXTAREA",
    "VIDEO",
    "EMBED",
    "IFRAME",
    "OBJECT",
    "APPLET",
  ];
  if (in_array (element.tagName, nav_elements))
    return true;
  const nav_input_types = [
    'date',
    'datetime-local',
    'email',
    'month',
    'number',
    'password',
    'range',
    'search',
    'tel',
    'text',
    'time',
    'url',
    'week',
  ];
  if (element.tagName == "INPUT" && in_array (element.type, nav_input_types))
    return true;
  return false;
}

/** Check if `element` is button-like input */
export function is_button_input (element) {
  const click_elements = [
    "BUTTON",
    "SUMMARY",
  ];
  if (in_array (element.tagName, click_elements))
    return true;
  const button_input_types = [
    'button',
    'checkbox',
    'color',
    'file',
    // 'hidden',
    'image',
    'radio',
    'reset',
    'submit',
  ];
  if (element.tagName == "INPUT" && in_array (element.type, button_input_types))
    return true;
  return false;
}

/** Compute global midpoint position of element, e.g. for focus movement */
export function element_midpoint (element) {
  const r = element.getBoundingClientRect();
  return { y: 0.5 * (r.top + r.bottom),
	   x: 0.5 * (r.left + r.right) };
}

/** Install a FocusGuard to allow only a restricted set of elements to get focus. */
class FocusGuard {
  defaults() { return {
    updown_focus: true,
    updown_cycling: false,
    focus_root_list: [],
    last_focus: undefined,
  }; }
  constructor () {
    Object.assign (this, this.defaults());
    window.addEventListener ('focusin', this.focusin_handler.bind (this), true);
    window.addEventListener ('keydown', this.keydown_handler.bind (this));
    if (document.activeElement && document.activeElement != document.body)
      this.last_focus = document.activeElement;
    // Related: https://developer.mozilla.org/en-US/docs/Web/Accessibility/Keyboard-navigable_JavaScript_widgets
  }
  push_focus_root (element, escapecb) {
    const current_focus = document.activeElement && document.activeElement != document.body ? document.activeElement : undefined;
    this.focus_root_list.unshift ([ element, current_focus, escapecb ]);
    if (current_focus)
      this.focus_changed (current_focus, false);
  }
  remove_focus_root (element) {
    if (this.last_focus && !this.last_focus.parentElement)
      this.last_focus = undefined;	// cleanup to allow GC
    for (let i = 0; i < this.focus_root_list.length; i++)
      if (this.focus_root_list[i][0] === element)
	{
	  const saved_focus = this.focus_root_list[i][1];
	  this.focus_root_list.splice (i, 1);
	  if (saved_focus)
	    saved_focus.focus(); // try restoring focus
	  return true;
	}
    return false;
  }
  focusin_handler (event) {
    return this.focus_changed (event.target);
  }
  focus_changed (target, refocus = true) {
    if (this.focus_root_list.length == 0 || !document.activeElement ||
	document.activeElement == document.body)
      return false; // not interfering
    const focuslist = list_focusables (this.focus_root_list[0][0]);
    const idx = focuslist.indexOf (target);
    if (idx < 0) // invalid element gaining focus
      {
	document.activeElement.blur();
	if (refocus && focuslist.length)
	  {
	    const lastidx = focuslist.indexOf (this.last_focus);
	    let newidx = 0;
	    if (lastidx >= 0 && lastidx < focuslist.length / 2)
	      newidx = focuslist.length - 1;
	    focuslist[newidx].focus();
	  }
	return true;
      }
    else
      this.last_focus = document.activeElement;
    return false; // not interfering
  }
  keydown_handler (event) {
    if (document.activeElement == document.body) // no focus
      return keydown_move_focus (event);
    else
      return false; // not interfering
  }
}
const the_focus_guard = new FocusGuard();

/** Constrain focus to `element` and its descendants */
export function push_focus_root (element, escapecb) {
  the_focus_guard.push_focus_root (element, escapecb);
}

/** Remove an `element` previously installed via push_focus_root() */
export function remove_focus_root (element) {
  the_focus_guard.remove_focus_root (element);
}

/** Move focus on UP/DOWN/HOME/END `keydown` events */
export function keydown_move_focus (event) {
  const root = the_focus_guard?.focus_root_list?.[0]?.[0] || document.body;
  let subfocus = null, left_right = false;
  // constrain focus movements within data-subfocus=1 container
  for (let element = document.activeElement; element; element = element.parentElement) {
    if (element === root)
      break;
    const d = element.getAttribute ('data-subfocus');
    if (d)
      {
	subfocus = element;
	if (d === "*")
	  left_right = true;
	break;
      }
  }
  let dir;
  if (event.keyCode == KeyCode.HOME)
    dir = 'HOME';
  else if (event.keyCode == KeyCode.END)
    dir = 'END';
  else if (event.keyCode == KeyCode.UP)
    dir = -1;
  else if (event.keyCode == KeyCode.DOWN)
    dir = +1;
  else if (left_right && event.keyCode == KeyCode.LEFT)
    dir = 'LEFT';
  else if (left_right && event.keyCode == KeyCode.RIGHT)
    dir = 'RIGHT';
  return move_focus (dir, subfocus);
}

/** Move focus to prev or next focus widget */
export function move_focus (dir = 0, subfocus = false) {
  const home = dir == 'HOME', end = dir == 'END';
  const up = dir == -1, down = dir == +1;
  const left = dir == 'LEFT', right = dir == 'RIGHT';
  const updown_focus = the_focus_guard.updown_focus;
  const updown_cycling = the_focus_guard.updown_cycling;
  const last_focus = the_focus_guard.last_focus;
  if (!(home || up || down || end || left || right))
    return false; // nothing to move

  if (the_focus_guard.focus_root_list.length == 0 || !updown_focus ||
      // is_nav_input (document.activeElement) || (document.activeElement.tagName == "INPUT" && !is_button_input (document.activeElement)) ||
      !(up || down || home || end || left || right))
    return false; // not interfering
  const root = subfocus || the_focus_guard.focus_root_list[0][0];
  const focuslist = list_focusables (root);
  if (!focuslist)
    return false; // not interfering
  let idx = focuslist.indexOf (document.activeElement);
  if (idx < 0 && (up || down))
    { // re-focus last element if possible
      idx = focuslist.indexOf (last_focus);
      if (idx >= 0)
	{
	  focuslist[idx].focus();
	  event.preventDefault();
	  return true;
	}
    }
  let next; // position to move new focus to
  if (idx < 0)
    next = (down || home) ? 0 : focuslist.length - 1;
  else if (home || end)
    next = home ? 0 : focuslist.length - 1;
  else if (left || right)
    {
      const idx_pos = element_midpoint (focuslist[idx]);
      let dist = { x: 9e99, y: 9e99 };
      for (let i = 0; i < focuslist.length; i++)
	if (i != idx)
	  {
	    const pos = element_midpoint (focuslist[i]);
	    const d = { x: pos.x - idx_pos.x, y: pos.y - idx_pos.y };
	    if ((right && d.x > 0) || (left && d.x < 0))
	      {
		d.x = Math.abs (d.x);
		d.y = Math.abs (d.y);
		if (d.x < dist.x || (d.x == dist.x && d.y < dist.y))
		  {
		    dist = d;
		    next = i;
		  }
	      }
	  }
    }
  else // up || down
    {
      next = idx + (up ? -1 : +1);
      if (updown_cycling)
	{
	  if (next < 0)
	    next += focuslist.length;
	  else if (next >= focuslist.length)
	    next -= focuslist.length;
	}
      else if (next < 0 || next >= focuslist.length)
	next = undefined;
    }
  if (next >= 0 && next < focuslist.length)
    {
      focuslist[next].focus();
      event.preventDefault();
      return true;
    }
  return false;
}

/** Forget the last focus elemtn inside `element` */
export function forget_focus (element) {
  if (!the_focus_guard.last_focus)
    return;
  let l = the_focus_guard.last_focus;
  while (l)
    {
      if (l == element)
	{
	  the_focus_guard.last_focus = undefined;
	  return;
	}
      l = l.parentNode;
    }
}

/** Setup Element shield for a modal containee.
 * Capture focus movements inside `containee`, call `closer(event)` for
 * pointer clicks on `shield` or when `ESCAPE` is pressed.
 */
export function setup_shield_element (shield, containee, closer)
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
      Util.swallow_event ('contextmenu', 0);
    }
  };
  shield.addEventListener ('mousedown', modal_mouse_guard);
  Util.push_focus_root (containee, closer);
  let undo_shield = () => {
    if (!undo_shield) return;
    undo_shield = null;
    shield.removeEventListener ('mousedown', modal_mouse_guard);
    Util.remove_focus_root (containee);
  };
  return undo_shield;
}

/** Use capturing to swallow any `type` events until `timeout` has passed */
export function swallow_event (type, timeout = 0) {
  const preventandstop = function (event) {
    event.preventDefault();
    event.stopPropagation();
    event.stopImmediatePropagation();
    return true;
  };
  document.addEventListener (type, preventandstop, true);
  setTimeout (() => document.removeEventListener (type, preventandstop, true), timeout);
}

/** Determine position for a popup */
export function popup_position (element, opts = { origin: undefined, x: undefined, y: undefined,
						  xscale: 0, yscale: 0, }) {
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

let frame_handler_id = 0x200000,
    frame_handler_active = false,
    frame_handler_callback_id = undefined,
    frame_handler_cur = 0,
    frame_handler_max = 0,
    frame_handler_array = undefined;

function call_frame_handlers () {
  frame_handler_callback_id = undefined;
  const active = frame_handler_active && shm_array_active;
  frame_handler_max = frame_handler_array.length;
  for (frame_handler_cur = 0; frame_handler_cur < frame_handler_max; frame_handler_cur++) {
    const handler_id = frame_handler_array[frame_handler_cur][1];
    const handler_result = frame_handler_array[frame_handler_cur][0] (active);
    if (handler_result !== undefined && !handler_result)
      remove_frame_handler (handler_id);
  }
  if (frame_handler_active)
    frame_handler_callback_id = window.requestAnimationFrame (call_frame_handlers);
}

function reinstall_frame_handler() {
  if (frame_handler_callback_id !== undefined) {
    window.cancelAnimationFrame (frame_handler_callback_id);
    frame_handler_callback_id = undefined;
  }
  if (frame_handler_active)
    frame_handler_callback_id = window.requestAnimationFrame (call_frame_handlers);
  else
    call_frame_handlers(); // call one time for cleanups with frame_handler_active == false
}

function remove_frame_handler (handler_id) {
  for (let i = frame_handler_array.length - 1; i >= 0; i--)
    if (frame_handler_array[i][1] == handler_id) {
      frame_handler_array.splice (i, 1);
      if (i < frame_handler_cur)
	frame_handler_cur--;
      frame_handler_max--;
      return;
    }
  console.error ("remove_frame_handler(" + handler_id + "): invalid id");
}

/// Install a permanent redraw handler, to run as long as the DSP engine is active.
export function add_frame_handler (handlerfunc) {
  if (frame_handler_array === undefined) { // must initialize
    frame_handler_array = [];
    (async function() {
      const check_active_promise = Ase.server.engine_active();
      const onenginechange_promise = Ase.server.on ('enginechange', (ev) => {
	frame_handler_active = Boolean (ev.active);
	shm_reschedule();
	reinstall_frame_handler();
      } );
      frame_handler_active = Boolean (await check_active_promise);
      shm_reschedule();
      reinstall_frame_handler();
      await onenginechange_promise;
    }) ();
  }
  const handler_id = frame_handler_id++;
  frame_handler_array.push ([handlerfunc, handler_id]);
  if (!frame_handler_callback_id) // ensure handlerfunc is called at least once
    frame_handler_callback_id = window.requestAnimationFrame (call_frame_handlers);
  return function() { remove_frame_handler (handler_id); };
}

export function discard_remote (aseobject) {
  console.log ("TODO: Ase: implement discard:", aseobject);
}

let shm_array_active = false;
let shm_array_entries = []; // { bpos, blength, shmoffset, usecount, }
let shm_array_binary_size = 8;
let shm_array_buffer = new ArrayBuffer (0);
export let shm_array_int32   = undefined;	// assigned by shm_receive
export let shm_array_float32 = undefined;	// assigned by shm_receive
export let shm_array_float64 = undefined;	// assigned by shm_receive
shm_receive (null);

export function shm_receive (arraybuffer) {
  shm_array_active = frame_handler_active && arraybuffer && shm_array_binary_size <= arraybuffer.byteLength;
  if (shm_array_active)
    shm_array_buffer = arraybuffer;
  else
    shm_array_buffer = new ArrayBuffer (shm_array_binary_size);
  shm_array_int32   = new Int32Array (shm_array_buffer, 0, shm_array_buffer.byteLength / 4 ^0);
  shm_array_float32 = new Float32Array (shm_array_buffer, 0, shm_array_buffer.byteLength / 4 ^0);
  shm_array_float64 = new Float64Array (shm_array_buffer, 0, shm_array_buffer.byteLength / 8 ^0);
}

export function shm_subscribe (byteoffset, bytelength) {
  const lalignment = 4;
  bytelength = (((bytelength + lalignment-1) / lalignment) ^0) * lalignment;
  // reuse existing region
  for (let i = 0; i < shm_array_entries.length; ++i)
    {
      const e = shm_array_entries[i];
      if (e.shmoffset <= byteoffset && e.shmoffset + e.blength >= byteoffset + bytelength)
	{
	  const pos = e.bpos + byteoffset - e.shmoffset;
	  e.usecount += 1;
	  return [ pos, i ];
	}
    }
  // reallocate freed region
  for (let i = 0; i < shm_array_entries.length; ++i)
    {
      const e = shm_array_entries[i];
      if (e.usecount == 0 && e.blength == bytelength)
	{
	  e.shmoffset = byteoffset;
	  e.usecount = 1;
	  shm_reschedule();
	  return [ e.bpos, i ];
	}
    }
  // allocate new pos, if length exceeds 4, pos needs larger alignment (e.g. for double)
  let nextpos = shm_array_binary_size;
  if (bytelength > 4)
    nextpos = (((nextpos + 8-1) / 8) ^0) * 8;
  // allocate new region
  let e = {
    bpos: nextpos,
    blength: bytelength,
    shmoffset: byteoffset,
    usecount: 1,
  };
  shm_array_binary_size = nextpos + bytelength;
  shm_array_entries.push (e);
  shm_reschedule();
  return [ e.bpos, shm_array_entries.length - 1 ];
}

export function shm_unsubscribe (subscription_tuple) {
  const e = shm_array_entries[subscription_tuple[1]];
  console.assert (e.usecount > 0);
  if (e.usecount)
    {
      e.usecount--;
      if (e.usecount == 0)
	{
	  e.shmoffset = -1;
	  // entry is disabled but stays around for future allocations and stable indexing
	  shm_reschedule();
	}
      return true;
    }
  return false;
}

function shm_reschedule() {
  if (pending_shm_reschedule_promise)
    return;
  pending_shm_reschedule_promise = delay (-1); // Vue.nextTick();
  pending_shm_reschedule_promise.then (async () => {
    pending_shm_reschedule_promise = null; // allow new shm_reschedule() calls
    if (frame_handler_active)
      {
	const entries = copy_recursively (shm_array_entries);
	for (let i = entries.length - 1; i >= 0; i--)
	  {
	    if (entries[i].usecount == 0)
	      entries.splice (i, 1);
	    else
	      delete entries[i].usecount;
	  }
	await Ase.server.broadcast_shm_fragments (entries, 33);
      }
    else
      {
	const promise = Ase.server.broadcast_shm_fragments ([], 0);
	shm_receive (null);
	await promise;
      }
  });
}
let pending_shm_reschedule_promise = null;

/// Create a promise that resolves after the given `timeout` with `value`.
function delay (timeout, value) {
  return new Promise (resolve => {
    if (timeout >= 0)
      setTimeout (resolve.bind (null, value), timeout);
    else if (window.setImmediate)
      setImmediate (resolve.bind (null, value));
    else
      setTimeout (resolve.bind (null, value), 0);
  });
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

/// Trigger element click via keyboard.
export function keyboard_click (element, callclick = true)
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
      if (callclick)
	element.click();
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

/// Popup `menu` using `event.currentTarget` as origin.
export function dropdown (menu, event, options = {})
{
  if (!options.origin)
    {
      // use the event handler element for positioning
      options.origin = event.currentTarget;
    }
  return menu?.popup (event, options);
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
    'data-tip': title ? title + ' ' + menuitem.get_text() : '',
  };
}

/// Symbolic names for key codes
export const KeyCode = {
  BACKSPACE: 8, TAB: 9, LINEFEED: 10, ENTER: 13, RETURN: 13, CAPITAL: 20, CAPSLOCK: 20, ESC: 27, ESCAPE: 27, SPACE: 32,
  PAGEUP: 33, PAGEDOWN: 34, END: 35, HOME: 36, LEFT: 37, UP: 38, RIGHT: 39, DOWN: 40, PRINTSCREEN: 44,
  INSERT: 45, DELETE: 46, SELECT: 93,
  F1: 112, F2: 113, F3: 114, F4: 115, F5: 116, F6: 117, F7: 118, F8: 119, F9: 120, F10: 121, F11: 122, F12: 123,
  F13: 124, F14: 125, F15: 126, F16: 127, F17: 128, F18: 129, F19: 130, F20: 131, F21: 132, F22: 133, F23: 134, F24: 135,
  BROWSERBACK: 166, BROWSERFORWARD: 167, PLUS: 187/*?*/, MINUS: 189/*?*/, PAUSE: 230, ALTGR: 255,
  VOLUMEMUTE: 173, VOLUMEDOWN: 174, VOLUMEUP: 175, MEDIANEXTTRACK: 176, MEDIAPREVIOUSTRACK: 177, MEDIASTOP: 178, MEDIAPLAYPAUSE: 179,
};

/// Check if a key code is used of rnavigaiton (and non alphanumeric).
export function is_navigation_key_code (keycode)
{
  const navigation_keys = [
    KeyCode.UP, KeyCode.RIGHT, KeyCode.DOWN, KeyCode.LEFT,
    KeyCode.PAGE_UP, KeyCode.PAGE_DOWN, KeyCode.HOME, KeyCode.END,
    KeyCode.ESCAPE, KeyCode.TAB, KeyCode.SELECT /*contextmenu*/,
    KeyCode.BROWSERBACK, KeyCode.BROWSERFORWARD,
    KeyCode.BACKSPACE, KeyCode.DELETE,
    KeyCode.SPACE, KeyCode.ENTER /*13*/,
  ];
  return in_array (keycode, navigation_keys);
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

/// Create display name from KeyEvent.code names.
export function display_keyname (keyname)
{
  // Replace KeyX with 'X'
  keyname = keyname.replace (/\bKey([A-Z])/g, "$1");
  // Replace Digit7 with '7'
  keyname = keyname.replace (/\bDigit([0-9])/g, "$1");
  return keyname;
}

/// Match an event's key code, considering modifiers.
export function match_key_event (event, keyname)
{
  // SEE: http://unixpapa.com/js/key.html & https://developer.mozilla.org/en-US/docs/Mozilla/Gecko/Gecko_keypress_event
  // split_hotkey (hotkey)
  const rex = new RegExp (/\s*[+]\s*/); // Split 'Shift+Ctrl+Alt+Meta+SPACE'
  const parts = keyname.split (rex);
  const keycode = event.code; // fallback: search KeyCode[] for value()==event.keyCode
  const keychar = event.key || String.fromCharCode (event.which || event.keyCode);
  let need_meta = 0, need_alt = 0, need_ctrl = 0, need_shift = 0;
  for (let i = 0; i < parts.length; i++)
    {
      // collect meta keys
      switch (parts[i].toLowerCase())
      {
	case 'cmd': case 'command':
	case 'super': case 'meta':	need_meta  = 1; continue;
	case 'option': case 'alt':	need_alt   = 1; continue;
	case 'control': case 'ctrl':	need_ctrl  = 1; continue;
	case 'shift':		  	need_shift = 1; continue;
      }
      // match named keys
      if (parts[i] == keycode)
	continue;
      // match character keys
      //if (keychar.toLowerCase() == parts[i].toLowerCase())
      //  continue;
      // failed to match
      return false;
    }
  // ignore shift for case insensitive characters (except for navigations)
  if (keychar.toLowerCase() == keychar.toUpperCase() &&
      !is_navigation_key_code (event.keyCode))
    need_shift = -1;
  // match meta keys
  if (need_meta   != 0 + event.metaKey ||
      need_alt    != 0 + event.altKey ||
      need_ctrl   != 0 + event.ctrlKey ||
      (need_shift != 0 + event.shiftKey && need_shift >= 0))
    return false;
  return true;
}

const hotkey_list = [];

function hotkey_handler (event) {
  let kdebug = () => undefined; // kdebug = debug;
  const focus_root = the_focus_guard?.focus_root_list?.[0];
  // avoid composition events, https://developer.cdn.mozilla.net/en-US/docs/Web/API/Element/keydown_event
  if (event.isComposing || event.keyCode === 229)
    {
      kdebug ("hotkey_handler: ignore-composition: " + event.code + ' (' + document.activeElement.tagName + ')');
      return false;
    }
  // allow ESCAPE callback for focus_root
  if (focus_root && focus_root[2] && Util.match_key_event (event, 'Escape'))
    {
      const escapecb = focus_root[2];
      if (false === escapecb (event))
	event.preventDefault();
      return true;
    }
  // give precedence to navigatable element with focus
  if (is_nav_input (document.activeElement) ||
      (document.activeElement.tagName == "INPUT" && !is_button_input (document.activeElement)))
    {
      kdebug ("hotkey_handler: ignore-nav: " + event.code + ' (' + document.activeElement.tagName + ')');
      return false;
    }
  // activate focus via Enter
  if (Util.match_key_event (event, 'Enter') && document.activeElement != document.body)
    {
      kdebug ("hotkey_handler: button-activation: " + ' (' + document.activeElement.tagName + ')');
      // allow `onclick` activation via `Enter` on <button/> and <a href/> with event.isTrusted
      const click_pending = document.activeElement.nodeName == 'BUTTON' || (document.activeElement.nodeName == 'A' &&
									    document.activeElement.href);
      Util.keyboard_click (document.activeElement, !click_pending);
      if (!click_pending)
	event.preventDefault();
      return true;
    }
  // restrict global hotkeys during modal dialogs
  const modal_element = !!(document._b_modal_shields?.[0]?.root || focus_root);
  // activate global hotkeys
  const array = hotkey_list;
  for (let i = 0; i < array.length; i++)
    if (match_key_event (event, array[i][0]))
      {
	const subtree_element = array[i][2];
	if (modal_element && !has_ancestor (subtree_element, modal_element))
	  continue;
	const callback = array[i][1];
	event.preventDefault();
	kdebug ("hotkey_handler: hotkey-callback: '" + array[i][0] + "'", callback.name);
	callback.call (null, event);
	return true;
      }
  // activate elements with data-hotkey=""
  const hotkey_elements = document.querySelectorAll ('[data-hotkey]:not([disabled])');
  for (const el of hotkey_elements)
    if (match_key_event (event, el.getAttribute ('data-hotkey')))
      {
	if (modal_element && !has_ancestor (el, modal_element))
	  continue;
	event.preventDefault();
	kdebug ("hotkey_handler: keyboard-click2: '" + el.getAttribute ('data-hotkey') + "'", el);
	Util.keyboard_click (el);
	return true;
      }
  kdebug ('hotkey_handler: unused-key: ' + event.code + ' ' + event.which + ' ' + event.charCode + ' (' + document.activeElement.tagName + ')');
  return false;
}
window.addEventListener ('keydown', hotkey_handler, { capture: true });

/// Add a global hotkey handler.
export function add_hotkey (hotkey, callback, subtree_element = undefined) {
  hotkey_list.push ([ hotkey, callback, subtree_element ]);
}

/// Remove a global hotkey handler.
export function remove_hotkey (hotkey, callback) {
  const array = hotkey_list;
  for (let i = 0; i < array.length; i++)
    if (hotkey === array[i][0] && callback === array[i][1])
      {
	array.splice (i, 1);
	return true;
      }
  return false;
}

/** Check if `ancestor` is an ancestor or `element` */
export function has_ancestor (element, ancestor) {
  while (element)
    {
      if (element === ancestor)
	return true;
      element = element.parentNode;
    }
  return false;
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

/** Method to be added to a `vue_observable_from_getters()` result to force updates. */
export function observable_force_update () {
  // This method works as a tag for vue_observable_from_getters()
}

/** Create an observable binding for the fields in `tmpl` with async callbacks.
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
 */
export function vue_observable_from_getters (tmpl, predicate) { // `this` is Vue instance
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
	    if (async_notify) // sets up listener
	      assign_async_cleanup (notify_cleanups, key, async_notify (getter));
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
