# UI component implementations

Anklang's UI uses SolidJS components in `ui/b/`, written in JSX or TSX.
A component file usually contains a documentation comment, an `Extra_css`
style block, and an exported function that returns JSX.
See [ButtonBar](jsdocsmd/b/buttonbar.md) for a small example.

Vite builds the UI. The plugin in `ui/extra-css.ts` extracts `Extra_css`
template strings and passes the CSS through the build's style processing.
Keep these style blocks at module scope and use CSS variables for values
that change at runtime.

## Component guidelines

- Create components for shared behavior, state, or a useful UI control.
  Use CSS classes for plain layout and spacing.
- Use native HTML elements where possible. Keep keyboard controls and
  focusable elements in the light DOM.
- Read changing props inside JSX or reactive computations. Use `splitProps`
  when separating local props from attributes passed to an HTML element.
- Use signals for local state and effects for work that follows state changes.
  Release event listeners, timers, and subscriptions with `onCleanup`.
- Pass nested content through `props.children`.

The older Lit helpers remain in `ui/little.js`, but the components in `ui/b/`
use SolidJS. New components should follow those JSX/TSX files. Vue templates,
Envue wrappers, and `JsExtract` style blocks are no longer the component pattern.

## Documentation comments

`doc/jsdoc2md.js` reads `.js`, `.jsx`, `.ts`, and `.tsx` files. It combines
Markdown blocks with API documentation from JSDoc. Both `/** ... */` and
consecutive `///` lines are supported.

A comment starting with a Markdown heading supplies handbook text:

```js
/** ## Clip list
 * The clip list starts playback of individual clips.
 */
```

For a component reference section, use `@class` and `@description`:

```js
/** @class Example
 * @description
 * Displays a value.
 *
 * ### Props
 * *value*
 * : The value to display.
 */
```

Put API descriptions directly before the functions, classes, methods, or
exported constants they describe. TypeScript types and JSX are converted to
JavaScript for JSDoc; the code is not executed.

To generate a reference page or extract handbook text:

```sh
node doc/jsdoc2md.js ui/b/buttonbar.tsx
node doc/jsdoc2md.js --markdown-only ui/b/cliplist.tsx
```

By default the command writes to stdout. Use `-O DIRECTORY` to write one
`.md` file per input, `-d DEPTH` to set API heading levels, and `-e NAME` to
prefix exported API names. Markdown headings keep their original levels.

`doc/Makefile.mk` generates reference pages under `out/doc/jsdocsmd/` and
handbook excerpts under `out/gen/`. `make mkdocs-site` builds the documentation
site; `make check-jsdoc` runs the extraction tests.
