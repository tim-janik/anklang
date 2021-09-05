// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
'use strict';

import * as Vue from '/vue.js';
export * from '/vue.js';

/// Component base class for wrapping Vue components.
export class Component {
  /// Let `this.$vm` point to the Vue component, and `$vm.$object` point to `this`.
  constructor (vm) {
    console.assert (vm);
    Object.defineProperty (this, '$vm', { value: vm });
    Object.defineProperty (vm, '$object', { value: this });
  }
  /// Force a Vue component update.
  update() {
    this.$vm?.$forceUpdate();
  }
  /// Wrapper for [Util.vue_observable_from_getters()](#Util.vue_observable_from_getters).
  observable_from_getters (...tmpl) { return Util.vue_observable_from_getters.call (this, ...tmpl); }
  /// Wrapper for [Vue.$watch](https://v3.vuejs.org/api/instance-methods.html#watch)
  $watch (...args) { return this.$vm.$watch (...args); }
  /// Create a Vue options API object from *vue_object* for SFC exports.
  static vue_export (vue_object = {})
  {
    const Class = this;
    return vue_export_from_class (Class, vue_object);
  }
}

/// Forward all accesses to fields on `vm` to access fields on `classinstance`.
export function forward_access (vm, classinstance, ignores = []) {
  const hasOwnProperty = (o, n) => Object.prototype.hasOwnProperty.call (o, n);
  const get_bindthis = (instance, name) => {
    let v = instance[name];
    if ('function' === typeof v)
      v = v.bind (instance); // get ahead of .bind (Vue.Proxy)
    return v;
  };
  // forward Class fields
  for (const name of Object.getOwnPropertyNames (classinstance))
    {
      if (ignores.indexOf (name) >= 0 || hasOwnProperty (vm, name))
	continue;
      const pd = Object.getOwnPropertyDescriptor (classinstance, name);
      Object.defineProperty (vm, name, {
	enumerable: pd.enumerable,
	set: vv => { classinstance[name] = vv; },
	get: () => get_bindthis (classinstance, name),
      });
    }
  // forward inherited members of the whole prototype chain
  for (let proto = classinstance.__proto__; proto != Object.prototype; proto = Object.getPrototypeOf (proto))
    for (const name of Object.getOwnPropertyNames (proto))
      {
	if (ignores.indexOf (name) >= 0 || hasOwnProperty (vm, name))
	  continue;
        const pd = Object.getOwnPropertyDescriptor (proto, name);
	Object.defineProperty (vm, name, {
	  enumerable: pd.enumerable,
	  set: vv => { classinstance[name] = vv; },
	  get: () => get_bindthis (classinstance, name),
	});
      }
}

/// Create a Vue options API object that proxies access to a newly created `Class` instance.
export function vue_export_from_class (Class, vue_object = {}) {
  const ignores = [ 'constructor', '$vm' ];
  vue_object.methods || (vue_object.methods = {});
  // hook into setup() and Options API lifecycle hooks
  const setup = vue_object.setup;
  vue_object.setup = function (props, context) {
    if (Class.prototype.beforeMount)     Vue.onBeforeMount     (_ => Vue.getCurrentInstance().ctx.$object.beforeMount());
    if (Class.prototype.mounted)         Vue.onMounted         (_ => Vue.getCurrentInstance().ctx.$object.mounted());
    if (Class.prototype.beforeUpdate)    Vue.onBeforeUpdate    (_ => Vue.getCurrentInstance().ctx.$object.beforeUpdate());
    if (Class.prototype.updated)         Vue.onUpdated         (_ => Vue.getCurrentInstance().ctx.$object.updated());
    if (Class.prototype.beforeUnmount)   Vue.onBeforeUnmount   (_ => Vue.getCurrentInstance().ctx.$object.beforeUnmount());
    if (Class.prototype.unmounted)       Vue.onUnmounted       (_ => Vue.getCurrentInstance().ctx.$object.unmounted());
    if (Class.prototype.errorCaptured)   Vue.onErrorCaptured   (_ => Vue.getCurrentInstance().ctx.$object.errorCaptured());
    if (Class.prototype.activated)       Vue.onActivated       (_ => Vue.getCurrentInstance().ctx.$object.activated());
    if (Class.prototype.deactivated)     Vue.onDeactivated     (_ => Vue.getCurrentInstance().ctx.$object.deactivated());
    if (Class.prototype.serverPrefetch)  Vue.onServerPrefetch  (_ => Vue.getCurrentInstance().ctx.$object.serverPrefetch());
    if (Class.prototype.renderTracked)   Vue.onRenderTracked   (_ => Vue.getCurrentInstance().ctx.$object.renderTracked());
    if (Class.prototype.renderTriggered) Vue.onRenderTriggered (_ => Vue.getCurrentInstance().ctx.$object.renderTriggered());
    return setup ? setup.call (this, props, context) : undefined;
  };
  // hook into beforeCreate()
  const beforeCreate = vue_object.beforeCreate;
  vue_object.beforeCreate = function () {
    const ctx = this._.ctx; // `this` is the Vue Proxy for ctx
    console.assert (ctx);
    const classinstance = new Class (this);
    console.assert (classinstance.$vm === this);
    console.assert (ctx.$object === classinstance);
    console.assert (this.$object === classinstance);
    forward_access (ctx, classinstance, ignores);
    // ^^^ Vue template compiler only checks hasOwnProperty accesses,
    // so all base class functions and fields have to be forwarded
    Class.prototype.beforeCreate?.call (classinstance);
    beforeCreate?.call (this);
  };
  // hook into created()
  if (Class.prototype.created || vue_object.created) {
    const created = vue_object.created;
    vue_object.created = function () {
      this.$object.created?.();
      created?.call (this);
    };
  }
  // copy static members
  window.c = window.c || [];
  window.c.push(Class);
  const statics = [ 'emits', 'data', 'props', 'computed', 'watch', 'provide', 'inject', 'components', 'directives', 'methods', ];
  for (const name of statics)
    if (Class[name] !== undefined &&
	!Object.prototype.hasOwnProperty.call (vue_object, name))
      {
	const pd = Object.getOwnPropertyDescriptor (Class, name);
	Object.defineProperty (vue_object, name, {
	  enumerable: pd.enumerable,
	  get: () => Class[name],
	});
      }
  return vue_object;
}
