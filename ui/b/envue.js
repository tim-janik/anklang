// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
'use strict';

import * as Vue from '/vue.js';
export * from '/vue.js';

/// Component base class for wrapping Vue components.
export class Component {
  /// Setup `this.$vm` and initialize a `Vue.reactive` member `this.m`.
  constructor (vm) {
    if (vm)
      this.$vm = vm;
    if (!this.m)
      this.m = Vue.reactive ({});
  }
  /// Force a Vue component update.
  update() {
    this.$vm?.$forceUpdate();
  }
  $watch (...a) { return this.$vm.$watch (...a); }
  observable_from_getters (...a) { return this.$vm.observable_from_getters (...a); }
  /// Create a Vue options API object for SFC exports.
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
	get: () => classinstance[name],
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
    if (Class.prototype.renderTracked)   Vue.onRenderTracked   (_ => Vue.getCurrentInstance().ctx.$object.renderTracked());
    if (Class.prototype.renderTriggered) Vue.onRenderTriggered (_ => Vue.getCurrentInstance().ctx.$object.renderTriggered());
    return setup ? setup.call (this, props, context) : undefined;
  };
  // hook into beforeCreate()
  const beforeCreate = vue_object.beforeCreate;
  vue_object.beforeCreate = function () {
    const ctx = this._.ctx;
    console.assert (ctx);
    const classinstance = new Class (this);
    Object.defineProperty (classinstance, '$vm', { value: this });
    Object.defineProperty (ctx, '$object', { value: classinstance });
    forward_access (ctx, classinstance, ignores);
    if (Class.prototype.beforeCreate)
      Class.prototype.beforeCreate.call (classinstance);
    if (beforeCreate)
      beforeCreate.call (this);
  };
  // hook into created()
  if (Class.prototype.created || vue_object.created) {
    const created = vue_object.created;
    vue_object.created = function () {
      const ctx = this._.ctx;
      console.assert (ctx && ctx.$object);
      if (Class.prototype.created)
	Class.prototype.created.call (ctx.$object);
      if (created)
	created.call (this);
    };
  }
  return vue_object;
}
