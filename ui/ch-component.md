
# UI component implementations

The user interface components used in Anklang live in `ui/b/` and are SolidJS functions.
They are generally composed of a single file that provides:

a) A brief documentation block;
b) CSS style information extracted at build time from `Extra_css` template strings;
c) An HTML layout specified with JSX;
d) A component function that manages state and event handlers.

Component styles and DOM queries use class selectors such as `.b-trackview`.
Components that accept children render `props.children`.

## Guidelines for UI components

Guidelines capturing past experiences:

- Create a component to encapsulate possible state and specific UI/UX behavior or API functionality.

- Avoid adding components purely for layout or style reasons. Horizontal or vertical [flex](https://developer.mozilla.org/en-US/docs/Learn/CSS/CSS_layout/Flexbox) boxes, [grid](https://developer.mozilla.org/en-US/docs/Learn/CSS/CSS_layout/Flexbox) layout, dialog and menu styling are much better addressed with CSS classes and are more often subject to change than encapsulated behavior.

- Use CSS utility classes instead of custom or adhoc styles that have to be given a dedicated name. [TailwindCSS](https://tailwindcss.com/docs/utility-first#maintainability-concerns) gives good guidance here.

- Keep focusable controls in the light DOM for reliable focus styles, keyboard handling, and tab order.

- Redraw canvases after `document.fonts.ready` resolves so text uses the loaded fonts.
