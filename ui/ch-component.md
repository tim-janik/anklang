
# Web Component Implementations

The user interface components used in Anklang are implemented as
[custom HTML elements](https://developer.mozilla.org/en-US/docs/Web/Web_Components/Using_custom_elements)
and are generally composed of a single file that provides:

a) A brief documentation block;
b) CSS style information, that is extracted at build time via [JsExtract](https://tim-janik.github.io/docs/anklang/jsextract_8js.html);
c) An HTML layout specified with [lit-html expressions](https://lit.dev/docs/templates/expressions/);
d) An assorted JavaScript class that defines a new custom HTML element, possibly via [Lit](https://lit.dev/docs/).

Simple components that have no or at most one child element and do not require complex HTML layouts with lit-html
can be implemented directly via [customElements.define()](https://developer.mozilla.org/en-US/docs/Web/API/CustomElementRegistry/define).

Components with complex layouts that need lit-html or that act as containers with several [HTMLSlotElement](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/slot)s
(for multiple types of children) which require a
[ShadowRoot](https://developer.mozilla.org/en-US/docs/Web/API/ShadowRoot),
should be implemented as LitElements by extending
[LitComponent](https://tim-janik.github.io/docs/anklang/little_8js.html#LitComponent) (our convenience wrapper around
[LitElement](https://lit.dev/docs/api/LitElement/).

Note that [a Lit component is an HTML element](https://lit.dev/docs/components/defining/#a-lit-component-is-an-html-element),
it extends [ReactiveElement](https://lit.dev/docs/api/ReactiveElement/)
which always extends [HTMLElement](https://developer.mozilla.org/en-US/docs/Web/API/HTMLElement) and none of the other
[HTML element interfaces](https://developer.mozilla.org/en-US/docs/Web/API/HTML_DOM_API#html_element_interfaces_2).


## Guidelines for Web Components

Guidelines capturing past experiences:

- Create a component to encapsulate possible state and specific UI/UX behavior or API functionality.

- Avoid adding components purely for layout or style reasons. Horizontal or vertical [flex](https://developer.mozilla.org/en-US/docs/Learn/CSS/CSS_layout/Flexbox) boxes, [grid](https://developer.mozilla.org/en-US/docs/Learn/CSS/CSS_layout/Flexbox) layout, dialog and menu styling are much better addressed with CSS classes and are more often subject to change than encapsulated behavior.

- Use CSS utility classes instead of custom or adhoc styles that have to be given a dedicated name. [TailwindCSS](https://tailwindcss.com/docs/utility-first#maintainability-concerns) gives good guidance here.

- Use simple custom element components if that suffices, components that do not expect external children can get away without a shadow DOM.

- Components with external children need [`<slot/>`](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/slot) elements, these require shadow DOM handling as provided by LitElement, use it via our [LitComponent](https://tim-janik.github.io/docs/anklang/little_8js.html#LitComponent) class.

- Focussable elements **MUST NOT** be buried inside the shadow DOM tree. It is perfectly fine to add focussable elements as (external) children to a ready-made web component that has a shadow DOM with `<slot/>` elements, but proper focus handling, focus related styling, keyboard and focus chain handling is not reliably possible when focussable elements are **inside the shadow DOM**, instead of staying part of the light DOM by adding them into `<slot/>` elements.

## Legacy Vue Components

Some components are still implemented via Vue and are slowly phased out.
We often use `<canvas>` elements for Anklang specific displays, and Vue canvas handling comes with certain caveats:

1) Use of the `Util.vue_mixins.dom_updates` mixin (now default) allows to trigger the `dom_update()`
   component method for `$forceUpdate()` invocations and related events.
2) A `methods: { dom_update() {}, }` component entry should be provided that triggers the
   actual canvas rendering logic.
3) Using a `document.fonts.ready` promise, Anklang re-renders all Vue components via
   `$forceUpdate()` once all webfonts have been loaded, `<canvas>` elements containing text
   usually need to re-render themselves in this case.

**Envue components:**

Envue components are created to simplify some of the oddities of dealing with Vue-3 components.
The function `Envue.Component.vue_export` creates a Vue component definition, so that the
Vue component instance (`$vm`) is tied to an `Envue.Component` instance (`$object`).
Notes:
- The Vue lifetime component can be accessed as `$object.$vm`.
- The Envue component can be accessed as `$vm.$object`.
- Accesses to `$vm.*` fields e.g. from within a `<template/>` definition are forwarded to access `$object.*` fields.
- Vue3 components are Proxy objects, *but* assignments to these Proxy objects is *not* reactive.
- To construct reactive instance data with async functions, use `observable_from_getters()`.

Vue uses a template compiler to construct a [render()](https://v3.vuejs.org/guide/render-function.html#complete-example)
function from [HTML](https://v3.vuejs.org/guide/template-syntax.html#raw-html) `<template/>` strings.
The [Javascript expressions](https://v3.vuejs.org/guide/template-syntax.html#javascript-expressions)
in templates are sandboxed and limited in scope, but may refer to Vue component properties
that are exposed through `hasOwnProperty()`.
In order to support Envue instance methods and fields in template expressions,
all members present after Envue construction are forwarded into the Vue component.
