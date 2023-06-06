<!-- This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0 -->

<docs>
  # B-NOTEBOARD
  Noteboard to post notifications for end users.
</docs>

<style lang="scss">
@import 'mixins.scss';

.b-noteboard {
  position: fixed; top: 0; left: 0; right: 0; bottom: 0;
  display: flex; flex-direction: column; flex-wrap: wrap-reverse;
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
    transition: all 0.233s ease-in-out; // see Util.NoteBoard.FADING
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
      max-height: 0; min-height: 0; // vertical shrinking, to allow siblings to flow up
      color: transparent;           // hide text reflow artefacts
      z-index: -1;                  // transition *behind* siblings
      transform: translateY(-33vh); // visual slide-up effect
      transition: all 0.283s ease-out 0.05s, transform 0.233s ease-in; // see Util.NoteBoard.FADING
    }
    // style close button
    .note-board-note-close {
      position: absolute; top: var(--note-board-padding); right: var(--note-board-padding);
      display: flex; margin-top: -0.2em;
      &:hover { color: #f88; }
    }
    // make room for .note-board-note-close
    &::before { float: right; content: ' '; padding: 1px; }
  }
  // markdown styling for notes
  .note-board-markdown {
    @include b-markdown-it-inlined;
    h1 { color: #a00; } //* Error */
    h2 { color: #a80; } //* Warning */
    h3 { color: #090; } //* Info */
    h4 { color: #09b; } //* Debug */
    p { margin-top: 0.5em; }
  }
}

</style>

<template>
  <v-flex class="b-noteboard"></v-flex>
</template>

<script>
import * as Envue from './envue.js';
import * as Util from "../util.js";

class Noteboard extends Envue.Component {
  TIMEOUT = 15 * 1000;	// time for note to last
  FADING = 233;		// fade in/out in milliseconds, see app.scss
  constructor (vm) {
    super (vm);
  }
  create_note (text, timeout) {
    const h53 = Util.hash53 (text);
    const dupselector = ".note-board-note[data-hash53='" + h53 + "']";
    for (const dup of this.$vm.$el.querySelectorAll (dupselector))
      if (dup && dup.__popdown) // deduplicate existing messages
	dup.__popdown();
    // create note with FADEIN
    const note = document.createElement ('div');
    note.setAttribute ('data-hash53', h53);
    note.classList.add ('note-board-note');
    note.classList.add ('note-board-fadein');
    // setup content
    if (Util.markdown_to_html)
      {
	note.classList.add ('note-board-markdown');
	Util.markdown_to_html (note, text);
      }
    else
      {
	note.classList.add ('note-board-plaintext');
	note.innerText = text;
      }
    // setup close button
    const close = document.createElement ('span');
    close.classList.add ('note-board-note-close');
    close.innerText = "✖";
    note.insertBefore (close, note.firstChild);
    const popdown = () => {
      if (!note.parentNode)
	return;
      note.classList.add ('note-board-fadeout');
      setTimeout (() => {
	if (note.parentNode)
	  note.parentNode.removeChild (note);
      }, this.FADING + 1);
    };
    note.__popdown = popdown;
    close.onclick = popdown;
    // show note with delay and throttling
    const popup = () => {
      note.setAttribute ('data-timestamp', Util.now());
      this.$vm.$el.appendChild (note);
      setTimeout (() => {
	note.classList.remove ('note-board-fadein');
	if (!(timeout < 0))
	  setTimeout (popdown, timeout ? timeout : this.TIMEOUT);
      }, this.FADING);
    };
    popup();
    if (this.$vm.$el.nextSibling) // raise noteboard
      this.$vm.$el.parentNode.insertBefore (this.$vm.$el, null);
    return popdown;
  }
}
export default Noteboard.vue_export ({ sfc_template });

</script>
