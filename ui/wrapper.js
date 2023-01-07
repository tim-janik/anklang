// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
'use strict';

const asecachingwrapper_cleanup_registry = new FinalizationRegistry (acw => acw.__cleanup__());

/// Caching wrapper for ASE classes
class AseCachingWrapper {
  constructor (aseobj) {
    // each non '__*' property is an aseobj property wrapper
    this.__wref__ = new WeakRef (aseobj);
    this.__cleanups__ = [];
    this.__promise__ = null;
    asecachingwrapper_cleanup_registry.register (aseobj, this, this);
  }
  async __ase_getter__ (prop, old_promise)
  {
    const aseobj = this.__wref__?.deref();
    const wrapper = this[prop];
    if (!aseobj || !wrapper)
      return old_promise;
    const value_promise = aseobj[prop] ();
    await old_promise;
    const val = await value_promise;
    if (this.__wref__ && val !== wrapper.value) {
      const old = wrapper.value;
      wrapper.value = Util.freeze_deep (val);
      for (const cb of wrapper.callbacks)
	cb.call (null, prop, old, val);
    }
  }
  /// Add property to cache
  __add__ (prop, defaultvalue = undefined, callback = undefined) {
    const aseobj = this.__wref__?.deref();
    if (!aseobj)
      return this.__cleanup__();
    let wrapper = this[prop];
    if (!wrapper) {
      const run_getter = async () => {
	const old_promise = this.__promise__;
	const new_promise = this.__ase_getter__ (prop, old_promise);
	this.__promise__ = new_promise;
	await new_promise;
	if (this.__promise__ === new_promise)
	  this.__promise__ = null; // reset if this was the last one pending
      };
      const del_notify = aseobj.on ("notify:" + prop, run_getter);
      this[prop] = wrapper = { count: 0, value: defaultvalue, del_notify, callbacks: [] };
      run_getter();
    }
    wrapper.count += 1;
    if (callback)
      wrapper.callbacks.push (callback);
  }
  /// Remove property caching request
  __del__ (prop, callback = undefined) {
    const aseobj = this.__wref__?.deref();
    if (!aseobj)
      return this.__cleanup__();
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
      wrapper.value = undefined; // allow GC of former contents
      wrapper.callbacks.length = 0; // allow GC
      const del_notify = wrapper.del_notify;
      wrapper.del_notify = undefined; // allows GC after this scope
      if (del_notify)
	del_notify();
    }
  }
  /// Remove all references
  __cleanup__() {
    asecachingwrapper_cleanup_registry.unregister (this);
    this.__wref__ = null;
    for (const prop of Object.getOwnPropertyNames (this))
      if (!prop.startsWith ('__'))
	{
	  const wrapper = this[prop];
	  if (!wrapper)
	    continue;
	  this[prop] = undefined;
	  wrapper.value = undefined; // allow GC of former contents
	  wrapper.callbacks.length = 0; // allow GC
	  const del_notify = wrapper.del_notify;
	  wrapper.del_notify = undefined; // allows GC after this scope
	  if (del_notify)
	    del_notify();
	}
    while (this.__cleanups__.length)
      this.__cleanups__.shift() ();
  }
}

const aseobj_weakmap = new WeakMap();

/// Wrap AseObject to cache properties and support __add__, __cleanup__ and auto cleanup.
export function wrap_ase_object (aseobj, fields = {}, callback = undefined)
{
  if (!aseobj) return aseobj;
  // make AseCachingWrapper
  if (!aseobj_weakmap.has (aseobj))
    aseobj_weakmap.set (aseobj, new AseCachingWrapper (aseobj));
  let cwrapper = aseobj_weakmap.get (aseobj);
  // make facade with aseobj prototype for automatic cleanup
  const __cleanup__ = () => {
    facade_cleanup_registry.unregister (__cleanup__);
    while (__cleanup__.cleanups.length)
      __cleanup__.cleanups.shift() ();
    cwrapper = null;
  };
  __cleanup__.cleanups = [];
  const __add__ = function (prop, defaultvalue, callback) {
    if (!cwrapper) return;
    cwrapper.__add__ (prop, defaultvalue, callback);
    __cleanup__.cleanups.push (() => cwrapper.__del__ (prop, callback));
    Object.defineProperty (this, prop, {
      get: function() { return cwrapper?.[prop]?.value; }
    });
  };
  const facade = { __aseobj__: aseobj,
		   __cleanup__,
		   __add__,
		   get __promise__() { return cwrapper?.__promise__; },
  };
  facade_cleanup_registry.register (facade, __cleanup__, __cleanup__);
  // add property caching
  for (const prop of Object.getOwnPropertyNames (fields))
    facade.__add__ (prop, fields[prop], callback);
  return facade;
}
const facade_cleanup_registry = new FinalizationRegistry (cleanupfunc => cleanupfunc());
