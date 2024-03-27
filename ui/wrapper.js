// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import * as Util from './util.js';

const asecachingwrapper_cleanup_registry = new FinalizationRegistry (acw => acw.__cleanup__());

/// Caching wrapper for ASE classes
class AseCachingWrapper {
  constructor (aseobj) {
    // each non '__*' property is an aseobj property wrapper
    /**@type{null|WeakRef}*/
    this.__wref__ = new WeakRef (aseobj);
    this.__cleanups__ = [];
    this.__promise__ = null;
    asecachingwrapper_cleanup_registry.register (aseobj, this, this);
  }
  /// Add property to cache
  __add__ (prop, defaultvalue = undefined, callback = undefined) {
    const aseobj = this.__wref__?.deref();
    if (!aseobj)
      return this.__cleanup__();
    let propwrapper = this[prop];
    if (!propwrapper) {
      let notify_set = undefined;
      const wrapper = { count: 0, value: defaultvalue,
			/**@type{null|function}*/ get_value: null,
			unwatch_notify: null,
			/**@type{Array<function>}*/ callbacks: [] };
      // debug helper
      if (typeof aseobj[prop] !== 'function')
	throw new TypeError (`property getter not callable: ${aseobj.constructor.name}($id=${aseobj.$id})['${prop}']==` + aseobj[prop]);
      // add reactive value getter
      wrapper.get_value = () =>
	{
          if (reactive_tracking())
            reactive_track (notify_set || (notify_set = new Set));
	  return wrapper.value;
	};
      // fetch value from Ase object, notify reactive watchers
      const async_refetch = async (prop, old_promise) =>
	{
	  const aseobj = this.__wref__?.deref();
	  if (!aseobj)
	    return old_promise;
	  const value_promise = aseobj[prop] ();
	  await old_promise;
	  const val = await value_promise;
	  if (this.__wref__ && val !== wrapper.value) {
	    const old = wrapper.value;
	    wrapper.value = Util.freeze_deep (val);
	    for (const cb of wrapper.callbacks)
	      cb.call (null, prop, old, val);
	    reactive_notify (notify_set);
	  }
	};
      // maintain this.__promise__ while refetch is in progress
      const promise_refetch = async () =>
	{
	  const old_promise = this.__promise__;
	  const new_promise = async_refetch (prop, old_promise);
	  this.__promise__ = new_promise;
	  await new_promise;
	  if (this.__promise__ === new_promise)
	    this.__promise__ = null; // reset if this was the last one pending
	};
      // refetch on Ase chamnges
      wrapper.unwatch_notify = aseobj.on ("notify:" + prop, promise_refetch);
      // Setup, initial refetch
      this[prop] = wrapper;
      promise_refetch();
      propwrapper = wrapper;
    }
    propwrapper.count += 1;
    if (callback)
      propwrapper.callbacks.push (callback);
  }
  /// Remove property caching request
  __del__ (prop, callback = undefined) {
    const wrapper = this[prop];
    console.assert (wrapper && wrapper.count >= 1);
    if (!wrapper?.count)
      return;
    wrapper.count -= 1;
    if (callback) {
      const cb_index = wrapper.callbacks.indexOf (callback);
      cb_index >= 0 && wrapper.callbacks.splice (cb_index, 1);
    }
    if (!wrapper.count) {
      this[prop] = undefined;
      wrapper.callbacks.length = 0; // allow GC
      const unwatch_notify = wrapper.unwatch_notify;
      for (const k in wrapper)
	wrapper[k] = undefined; // allow GC of former contents
      if (unwatch_notify)
	unwatch_notify();
    }
  }
  /// Remove all references
  __cleanup__() {
    asecachingwrapper_cleanup_registry.unregister (this);
    this.__wref__ = null;
    this.__promise__ = null;
    for (const prop of Object.keys (this))
      if (!prop.startsWith ('__') && this[prop]?.count) {
	this[prop].count = 1;
	this.__del__ (prop);
      }
    while (this.__cleanups__.length)
      this.__cleanups__.shift() ();
  }
}

const aseobj_weakmap = new WeakMap();

/** Wrap AseObject to cache properties and support __add__, __cleanup__ and auto cleanup.
 * @param {function|undefined} callback
 */
export function wrap_ase_object (aseobj, fields = {}, callback = undefined)
{
  if (!aseobj) return aseobj;
  // make AseCachingWrapper
  if (!aseobj_weakmap.has (aseobj))
    aseobj_weakmap.set (aseobj, new AseCachingWrapper (aseobj));
  let cwrapper = aseobj_weakmap.get (aseobj);
  // make facade with aseobj prototype for automatic cleanup
  const __cleanup__ = () => {
    finalization_cleanup_registry.unregister (__cleanup__);
    while (__cleanup__.cleanups.length)
      __cleanup__.cleanups.shift() ();
    cwrapper = null;
  };
  __cleanup__.cleanups = [];
  /**@this any*/
  const __add__ = function (prop, defaultvalue, callback) {
    if (!cwrapper) return;
    cwrapper.__add__ (prop, defaultvalue, callback);
    __cleanup__.cleanups.push (() => cwrapper.__del__ (prop, callback));
    Object.defineProperty (this, prop, {
      enumerable: true,
      configurable: true,
      get: function() { return cwrapper?.[prop]?.get_value?.(); }
    });
  };
  const facade = { __aseobj__: aseobj,
		   __cleanup__,
		   __add__,
		   get __promise__() { return cwrapper?.__promise__; },
  };
  finalization_cleanup_registry.register (facade, __cleanup__, __cleanup__);
  // add property caching
  for (const prop of Object.getOwnPropertyNames (fields))
    facade.__add__ (prop, fields[prop], callback);
  return facade;
}

/// FinalizationRegistry to call cleanup callback upon object destruction.
const finalization_cleanup_registry = new FinalizationRegistry (cleanupfunc => cleanupfunc());

/// Define reactive properties on `object`, to be used with reactive_wrapper().
/// See also [Object.defineProperties](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/defineProperties).
export function define_reactive (object, properties_object)
{
  const data = {};
  for (const key of Object.keys (properties_object))
    {
      const descriptor = properties_object[key] instanceof Object ? properties_object[key] : {};
      const enumerable = descriptor.enumerable !== undefined ? !!descriptor.enumerable : true;
      const configurable = descriptor.configurable !== undefined ? !!descriptor.configurable : false;
      const writable = descriptor.writable !== undefined ? !!descriptor.writable : true;
      const value = 'value' in descriptor ? descriptor.value : properties_object[key];
      if (!writable) {
	Object.defineProperty (object, key, { enumerable, configurable, value });
	continue;
      }
      data[key] = value;
      let notify_set = undefined;
      Object.defineProperty (object, key, {
	enumerable,
	get () {
	  if (reactive_tracking())
	    reactive_track (notify_set || (notify_set = new Set));
	  return data[key];
	},
	set (v) {
	  if (v === data[key]) return;
	  data[key] = v;
	  reactive_notify (notify_set);
	},
	configurable });
    }
}

function reactive_tracking()
{
  return !!reactive_effect_handle;
}
let reactive_effect_handle = null;

function reactive_track (notify_set)
{
  const handle = reactive_effect_handle && reactive_effect_handle.deref();
  handle?.track (notify_set);
}

function reactive_notify (notify_set)
{
  if (!notify_set) return;
  const call_handle_notify = handle_wref => {
    const handle = handle_wref.deref();
    if (handle)
      handle.notify();
    else // stale WeakRef
      notify_set.delete (handle_wref);
  };
  notify_set.forEach (call_handle_notify);
}

// Internal handle for reactive_wrapper()
class ReactiveHandle {
  wref;
  notifier;
  notify_sets;
  effect;
  once = false;
  running = 0;
  notify ()
  {
    if (this.once)
      this.untrack();
    this.notifier();
  }
  run ()
  {
    if (!this.running) // recursion
      this.untrack();
    const last_handle = reactive_effect_handle;
    reactive_effect_handle = this.wref;
    this.running += 1;
    const result = this.effect (...arguments);
    this.running -= 1;
    reactive_effect_handle = last_handle;
    return result;
  }
  untrack ()
  {
    if (!this.notify_sets) return;
    this.notify_sets.forEach (nset => nset.delete (this.wref));
    this.notify_sets.clear();
  }
  track (nset)
  {
    this.notify_sets || (this.notify_sets = new Set);
    this.notify_sets.add (nset);
    nset.add (this.wref);
  }
}

/// Make `effect()` wrapper to watch reactive properties, on changes run `notifier()`.
export function reactive_wrapper (effect, notifier, keepwatching = false)
{
  const handle = new ReactiveHandle;
  handle.wref = new WeakRef (handle);
  handle.notifier = notifier;
  handle.effect = effect;
  handle.once = !keepwatching;
  handle.run = handle.run.bind (handle);
  return handle.run;
}
