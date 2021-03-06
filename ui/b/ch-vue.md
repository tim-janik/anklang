
# Vue Component Reference

The Vue components used in Anklang are generally composed of a single file that provides: \
a) Brief documentation;
b) CSS style information;
c) Html layout;
d) Assorted JS logic.

The Vue component object is implemented as part of (d).
We often use `<canvas>` elements for Anklang specific displays, and Vue canvas handling comes with certain caveats:

1) Use of the `Util.vue_mixins.dom_updates` mixin (now default) allows to trigger the `dom_update()`
   component method for `$forceUpdate()` invocations and related events.

2) A `methods: { dom_update() {}, }` component entry should be provided that triggers the
   actual canvas rendering logic.

3) Using a `document.fonts.ready` promise, Anklang re-renders all Vue components via
   `$forceUpdate()` once all webfonts have been loaded, `<canvas>` elements containing text
   usually need to re-render themselves in this case.

## Envue components

Envue components are created to simplify some of the oddities of dealing with Vue-3 components.
The function `Envue.Component.vue_export` creates a Vue component definition, so that the
Vue component instance (`$vm`) is tied to an `Envue.Component` instance (`$object`).
Notes:
- The Vue lifetime component can be accessed as `$object.$vm`.
- The Envue component can be accessed as `$vm.$object`.
- Accesses to `$vm.*` fields e.g. from within a `<template/>` definition are forwarded to access `$object.*` fields.
- Vue3 components are Proxy objects, *but* assignments to these Proxy objects is *not* reactive.
- We need a way to construct reactive instance data via async functions, `observable_from_getters()` allows this.
