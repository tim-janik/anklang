// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class B-Noticeboard
 * SolidJS component that displays notification notes for end users.
 *
 * ### API:
 * *create_note(text, timeout?)*
 * : Post a notification note. Returns a popdown function.
 */

import { onCleanup } from 'solid-js';
import * as Util from "../util.js";
import * as Dom from "../dom.js";

// == STYLE ==
Extra_css`
b-noticeboard {
  position: fixed; inset: 0;
  display: flex; flex-flow: column wrap-reverse;
  align-items: flex-end; align-content: end;
  pointer-events: none; user-select: none;
  --note-board-padding: 12px;
  padding: var(--note-board-padding) var(--note-board-padding) 0 0;
  transition: all 0.2s ease;
  .note-board-note {
    position: relative; width: 25em; min-height: 5em;
    color: #111; background: #eef; opacity: 0.95;
    pointer-events: all;
    padding: var(--note-board-padding); margin: 0 0 var(--note-board-padding) var(--note-board-padding);
    border-radius: calc(var(--note-board-padding) / 2);
    transition: all 0.233s ease-in-out; /* see Util.Noticeboard.FADING */
    overflow: hidden; max-height: 100vh;
    &.note-board-fadein {
      transform: translateY(100vh);
    }
    &.note-board-fadeout {
      /* CSS cannot fade from height:auto -> height:0...
       * But we can animate absolute dimensions of max-height and somewhat
       * cover up artefacts with transition and color fading.
       */
      padding-top: 0; padding-bottom: 0; margin-top: 0;
      max-height: 0; min-height: 0; /* vertical shrinking, to allow siblings to flow up */
      color: transparent;           /* hide text reflow artefacts */
      z-index: -1;                  /* transition *behind* siblings */
      transform: translateY(-33vh); /* visual slide-up effect */
      transition: all 0.283s ease-out 0.05s, transform 0.233s ease-in; /* see Util.Noticeboard.FADING */
    }
    /* style close button */
    .note-board-note-close {
      position: absolute; top: var(--note-board-padding); right: var(--note-board-padding);
      display: flex; margin-top: -0.2em;
      &:hover { color: #f88; }
    }
    /* make room for .note-board-note-close */
    &::before { float: right; content: ' '; padding: 1px; }
  }
  /* markdown styling for notes */
  .note-board-markdown {
    @include b-markdown-it-inlined;
    h1 { color: #a00; } /* Error */
    h2 { color: #a80; } /* Warning */
    h3 { color: #090; } /* Info */
    h4 { color: #09b; } /* Debug */
    p { margin-top: 0.5em; }
  }
}`;

// == Module-level state ==
const POPDOWN = Symbol.for('b_noticeboard_POPDOWN');
const TIMEOUT = 15 * 1000; // time for note to last
const FADING = 233;        // fade in/out in milliseconds, see app.scss
let container;
const timeouts = new Map();

/**
 * Create a notification note.
 * @param {string} text - Note content (supports markdown)
 * @param {number} [timeout] - Auto-dismiss timeout in ms (default: 15s)
 * @returns {Function} popdown function to dismiss the note
 */
export function create_note (text, timeout)
{
  const h53 = Util.hash53 (text);
  const dupselector = ".note-board-note[data-hash53='" + h53 + "']";
  for (const dup of container?.querySelectorAll (dupselector) || [])
    if (dup && dup[POPDOWN]) // deduplicate existing messages
      dup[POPDOWN]();

  // create note with FADEIN
  const note = document.createElement ('div');
  note.setAttribute ('data-hash53', '' + h53);
  note.setAttribute ('role', 'status');
  note.classList.add ('note-board-note');
  note.classList.add ('note-board-fadein');

  // setup content
  if (Dom.markdown_to_html) {
    note.classList.add ('note-board-markdown');
    Dom.markdown_to_html (note, text);
  } else {
    note.classList.add ('note-board-plaintext');
    note.innerText = text;
  }

  // setup close button
  const close = document.createElement ('span');
  close.classList.add ('note-board-note-close');
  close.innerText = "\u2716";
  note.insertBefore (close, note.firstChild);

  const popdown = () => {
    if (!note.parentNode) return;
    note.classList.add ('note-board-fadeout');
    setTimeout (() => {
      if (note.parentNode)
        note.parentNode.removeChild (note);
    }, FADING + 1);
  };
  note[POPDOWN] = popdown;
  close.onclick = popdown;

  // show note with delay and throttling
  const popup = () => {
    note.setAttribute ('data-timestamp', '' + Util.now());
    container?.appendChild (note);
    const fadeInTimer = setTimeout (() => {
      note.classList.remove ('note-board-fadein');
      if (!(timeout < 0)) {
        const autoHideTimer = setTimeout (popdown, timeout ? timeout : TIMEOUT);
        timeouts.set (h53, autoHideTimer);
      }
    }, FADING);
    timeouts.set (h53 + '_fadein', fadeInTimer);
  };
  popup();

  if (container?.nextSibling) // raise noticeboard
    container.parentNode?.insertBefore (container, null);

  return popdown;
}

// == Component ==
export function Noticeboard (props)
{
  const setContainer = (el) => { container = el; };

  onCleanup (() => {
    for (const [, tid] of timeouts)
      clearTimeout (tid);
    timeouts.clear();
    while (container?.firstChild)
      container.removeChild (container.firstChild);
  });

  return (
    <b-noticeboard ref={setContainer} style={props.style}></b-noticeboard>
  );
}
