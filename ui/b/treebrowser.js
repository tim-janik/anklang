// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import { LitComponent, html, repeat, JsExtract, docs } from '../little.js';
import * as ContextMenu from './contextmenu.js';
import * as Util from '../util.js';
import * as Kbd from '../kbd.js';
import { get_uri } from '../dom.js';

// == STYLE ==
Extra_css`
@reference "../tailwind.css";

b-treebrowser {
  margin: 0 var(--b-menu-hpad);
  @apply inline-flex flex-col;
  --b-treebrowser-indent: 1.5rem;
  user-select: none;
}

b-treebrowser details {
  @apply inline-flex flex-col;
  padding-left: var(--b-treebrowser-indent);
  &[disabled], &[disabled] * { color: var(--b-menu-disabled); }
}

b-treebrowser details > summary {
  @apply font-bold relative;
  list-style: none;
}
b-treebrowser details > summary::before {
  @apply absolute inline;
  left: calc(-1 * var(--b-treebrowser-indent) + 2px);
  content: '►'; /* ▸ */
  transition: all .1s ease;
}
b-treebrowser details[open] > summary::before {
  content: '▷'; /* ▹ ▼ */
  transform: rotate(90deg);
}

b-treebrowser button {
  @apply flex text-left;
}
`;

// == HTML ==
const HTML_ENTRY = (tree, e) => [
  e.entries  ? null : html`
    <button uri="${e.uri}">
      ${e.label}
    </button>`,
  !e.entries ? null : HTML_BRANCH (tree, e)
];
const HTML_BRANCH = (tree, e) =>
  html`
    <details ?open=${tree.expandall}>
      <summary>
	${e.label}
      </summary>
      ${repeat (e.entries, x => x.uri, x => HTML_ENTRY (tree, x))}
    </details>
  `;

// == SCRIPT ==
/** @class BTreeBrowser
 * @description
 * This container can render tree structures with collapsible branches.
 * ## Props:
 * *expandall*
 * : Expand all entries by default.
 * ## Events:
 * *close*
 * : A *close* event is emitted once the "Close" button activated.
 */
class BTreeBrowser extends LitComponent {
  createRenderRoot() { return this; }
  render()
  {
    return this.entries_.map (e => HTML_ENTRY (this, e));
  }
  static properties = {
    // https://lit.dev/docs/components/properties/#property-options
    expandall:       { type: Boolean, reflect: true }, // attribute: true
    tree:            { attribute: false, }, // assign as .tree={...}
  };
  constructor()
  {
    super();
    this.expandall = true;
    this.tree = [];
    this.addEventListener ("keydown", this.focus_updown.bind (this));
  }
  get entries_()
  {
    if (Array.isArray (this.tree)) return this.tree;
    if (this.tree) return [ this.tree ];
    return  [];
  }
  focus_updown (event)
  {
    const target = event.target;
    // RIGHT opens branch
    if (target.tagName === 'SUMMARY' &&
	target.parentElement.open === false &&
	Kbd.match_key_event (event, 'ArrowRight')) {
      target.parentElement.open = true;
      return Util.prevent_event (event);
    }
    // LEFT closes branch
    if (target.tagName === 'SUMMARY' &&
	target.parentElement.open === true &&
	Kbd.match_key_event (event, 'ArrowLeft')) {
      target.parentElement.open = false;
      return Util.prevent_event (event);
    }
    // focus RIGHT or DOWN
    if (Kbd.match_key_event (event, ['ArrowRight', 'ArrowDown'])) {
      Util.prevent_event (event);
      return Kbd.move_focus_next();
    }
    // focus LEFT or UP
    if (Kbd.match_key_event (event, ['ArrowLeft', 'ArrowUp'])) {
      Util.prevent_event (event);
      return Kbd.move_focus_prev();
    }
  }
}
customElements.define ('b-treebrowser', BTreeBrowser);

export const example_data = {
  // Example data
  label: 'Tree Root',
  entries: [
    { label: 'Hello-1' },
    { label: 'Second Choice' },
    {
      label: 'Expandable Children',
      entries: [
	{
	  label: 'Subfolder Stuff',
	  entries: [
	    { label: 'A - One' },
	    { label: 'B - Two' },
	    { label: 'C - Three' },
	  ]
	},
	{ label: 'Ying' },
	{
	  label: '| More Things...',
	  entries: [
	    { label: '| Abcdefgh' },
	    { label: '| ijklmnopq' },
	    { label: '| rstuvwxyz' },
	  ]
	},
	{ label: 'Yang' },
      ]
    }
  ]
};
