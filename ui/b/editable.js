// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import { LitComponent, html, JsExtract, docs, ref } from '../little.js';
import * as Util from "../util.js";

/** == B-EDITABLE ==
 * Display an editable span.
 * ### Methods:
 * - **activate()** - Show input field and start editing.
 * ### Props:
 * - **clicks** - Set to 1 or 2 to activate for single or double click.
 * ### Events:
 * - **change** - Emitted when an edit ends successfully, text is in `event.detail.value`.
 */

// == STYLE ==
JsExtract.scss`
b-editable {
  @apply inline-flex;
}`;


// == SCRIPT ==
import * as Ase from '../aseapi.js';

const NUMBER_ATTRIBUTE = { type: Number, reflect: true }; // sync attribute with property
const STRING_ATTRIBUTE = { type: String, reflect: true }; // sync attribute with property
const PRIVATE_PROPERTY = { state: true };

class BEditable extends LitComponent {
  static properties = {
    clicks:	NUMBER_ATTRIBUTE,
    editable_:	PRIVATE_PROPERTY,
    value: 	STRING_ATTRIBUTE,
  };
  createRenderRoot() { return this; }
  render()
  {
    // Since this.input_ is focussable, we avoid putting it into a shadowRoot
    return this.input_;
  }
  constructor()
  {
    super();
    this.clicks = 1;
    this.value = '';
    this.editable_ = false;
    const input_click = this.input_click.bind (this);
    this.addEventListener ("click", input_click);
    this.addEventListener ("dblclick", input_click);
    this.input_ = document.createElement ('input');
    this.input_.className = "focus:via-dim-950 m-[2px] box-content h-min w-full p-0 leading-none text-inherit [&[inert]]:bg-transparent";
    this.input_.inert = true;
    this.input_.onkeydown = this.input_keydown.bind (this);
    this.input_.onchange = e => Util.prevent_event (e);
    this.input_.onblur = this.input_blur.bind (this);
  }
  updated (changed_props)
  {
    if (changed_props.has ('value'))
      this.input_.value = this.value;
  }
  activate()
  {
    this.editable_ = true;
    this.input_.inert = false;
    if (document.activeElement !== this.input_) {
      this.input_.focus();
      this.input_.selectionEnd = this.input_.selectionStart = 0;
      this.input_.setSelectionRange (2**31, 2**31); // move cursor to end
    }
  }
  input_click (event)
  {
    const clicks = 0 | this.clicks;
    if ((clicks === 1 && event.type === "click") ||
	(clicks === 2 && event.type === "dblclick")) {
      Util.prevent_event (event);
      this.activate();
    }
  }
  input_keydown (event)
  {
    event.stopPropagation();
    const esc = Util.match_key_event (event, 'Escape');
    const enter = Util.match_key_event (event, 'Enter');
    if (esc || enter)
      this.input_blur (null, enter);
  }
  input_blur (event_, confirmed = false)
  {
    this.input_.selectionEnd = this.input_.selectionStart = 0;
    this.input_.inert = true;
    this.input_.scrollLeft = 0;
    if (!confirmed)
      this.input_.value = this.value;
    else {
      this.dispatchEvent (new CustomEvent ('change', {
	detail: { value: this.input_.value }
      }));
    }
  }
}
customElements.define ('b-editable', BEditable);
