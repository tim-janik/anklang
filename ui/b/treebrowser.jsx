// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class B-TreeBrowser
 * SolidJS component that renders tree structures with collapsible branches.
 *
 * ### Props:
 * *tree*
 * : Array or single entry object to render.
 * *expandall*
 * : Expand all entries by default (default: true).
 */

import { createSignal, For, onMount, splitProps } from 'solid-js';
import * as Util from '../util.js';
import * as Kbd from '../kbd.js';

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

// == Recursive Tree Node Component ==
function TreeNode (props)
{
  const has_entries = () => props.entry.entries?.length > 0;

  return (
    <>
      {has_entries() ? (
        <details open={props.expandall}>
          <summary>
            {props.entry.label}
          </summary>
          <For each={props.entry.entries}>
            {(entry) => <TreeNode entry={entry} expandall={props.expandall} />}
          </For>
        </details>
      ) : (
        <button uri={props.entry.uri}>{props.entry.label}</button>
      )}
    </>
  );
}

// == Component ==
export function TreeBrowser (props)
{
  const [local, rest] = splitProps (props, ['class', 'tree', 'expandall']);
  const [expandall, setExpandall] = createSignal (props.expandall ?? true);
  /** @type {HTMLElement | undefined} */
  let container;

  onMount (() => {
    container?.addEventListener ('keydown', focus_updown);
  });

  const focus_updown = (event) => {
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
  };

  const entries = () => {
    const tree = props.tree;
    if (Array.isArray (tree)) return tree;
    if (tree) return [ tree ];
    return [];
  };

  return (
    <b-treebrowser {...rest} ref={container} class={'b-treebrowser' + (local.class ? ' ' + local.class : '')}>
      <For each={entries()}>
        {(entry) => <TreeNode entry={entry} expandall={expandall()} />}
      </For>
    </b-treebrowser>
  );
}

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
