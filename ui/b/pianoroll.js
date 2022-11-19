// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

/** # B-PIANOROLL
 * A pianoroll element.
 * ## Properties:
 * *.isactive*: bool (uri)
 * : Property callback used to check if a particular piano roll should stay active or be disabled.
 * ## Attributes:
 * *clip*
 * : The clip with notes to be edited.
 */

import { LitElement, html, css, postcss, docs } from '../little.js';
import * as Kbd from '../kbd.js';

// == STYLE ==
const STYLE = await postcss`
@import 'shadow.scss';
@import 'mixins.scss';
:host {
  display: flex; flex-direction: column; align-rolls: stretch;
}
button {
  display: flex; flex-direction: column; align-rolls: stretch;
  flex: 0 0 auto;
  flex-wrap: nowrap;
  flex-direction: row;
  margin: 0; padding: $b-piano-vpad $b-piano-hpad; text-align: left;
  // button-reset
  background: transparent; color: $b-piano-foreground; border: 1px solid transparent;
  cursor: pointer; user-select: none; outline: none;
  kbd { color: zmod($b-piano-foreground, Jz-=15); }
  b-icon { color: $b-piano-fill; height: 1em; width: 1em; }
  > b-icon:first-child { margin: 0 $b-piano-spacing 0 0; }
}
`;

// == HTML ==
const HTML = (t, d) => html`
  <c-grid>
    Piano Roll
  </c-grid>
`;

// == SCRIPT ==
const BOOL_ATTRIBUTE = { type: Boolean, reflect: true }; // sync attribute with property
const STRING_ATTRIBUTE = { type: String, reflect: true }; // sync attribute with property
const PRIVATE_PROPERTY = { state: true };

class BPianoRoll extends LitElement {
  static styles = [ STYLE ];
  static shadowRootOptions = { ...LitElement.shadowRootOptions, delegatesFocus: true };
  render()
  {
    const d = {};
    return HTML (this, d);
  }
  static properties = {
    clip: STRING_ATTRIBUTE,
    disabled: BOOL_ATTRIBUTE,
    iconclass: STRING_ATTRIBUTE,
    isactive: PRIVATE_PROPERTY,
  };
  constructor()
  {
    super();
    this.clip = null;
    this.disabled = false;
    this.iconclass = '';
    this.isactive = true;
  }
  updated (changed_props)
  {
    // FIXME: this.setAttribute ('aria-label', this.slot_label());
  }
}

customElements.define ('b-pianoroll', BPianoRoll);
