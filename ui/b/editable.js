// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
import { LitComponent, html, JsExtract, docs, ref } from '../little.js';
import * as Util from "../util.js";

/** == B-EDITABLE ==
 * Display an editable span.
 * ### Methods:
 * - **activate()** - Show input field and start editing.
 * ### Props:
 * - **selectall** - Set to `true` to automatically select all editable text.
 * - **clicks** - Set to 1 or 2 to activate for single or double click.
 * ### Events:
 * - **change** - Emitted when an edit ends successfully, text is in `event.detail.value`.
 */

// == STYLE ==
const STYLE = await JsExtract.fetch_css (import.meta);
JsExtract.scss`
@import 'mixins.scss';
:host {
  display: flex;
  position: relative;
}
input {
  position: absolute;
  display: inline-block;
  top: 0; left: 0; right: 0; bottom: 0;
  width: 100%; height: 100%;
  margin: 0; padding: 0; border: 0;
  width: 100%; height: 100%; line-height: 1;
  letter-spacing: inherit; text-align: inherit;
  background: #000; color: inherit; font: inherit;
  z-index: 2;
  outline: 0;
  box-sizing: content-box;
  vertical-align: middle;
  white-space: nowrap;
  overflow: hidden;
}
slot          { color: inherit; }
slot[editing] { color: #0000; }
`;

// == HTML ==
const HTML = (t, d) => html`
  <slot ?editing=${t.editable_} ></slot>${INPUT_HTML (t, d)}
`;
const INPUT_HTML = (t, d) =>
  !t.editable_ ? '' :
  html`
    <input class="b-editable-overlay"
	${ref (h => t.input_ = h)}
	@keydown=${t.input_keydown}
	@change=${t.input_change}
	@blur=${t.input_blur}
    /></span>
  `;

// == SCRIPT ==
import * as Ase from '../aseapi.js';

const BOOLEAN_ATTRIBUTE = { type: Boolean, reflect: true }; // sync attribute with property
const NUMBER_ATTRIBUTE = { type: Number, reflect: true }; // sync attribute with property
const PRIVATE_PROPERTY = { state: true };

class BEditable extends LitComponent {
  static styles = [ STYLE ];
  static properties = {
    selectall:	BOOLEAN_ATTRIBUTE,
    clicks:	NUMBER_ATTRIBUTE,
    editable_:	PRIVATE_PROPERTY,
  };
  render()
  {
    return HTML (this);
  }
  constructor()
  {
    super();
    this.selectall = false;
    this.clicks = 1;
    this.editable_ = false;
    this.input_ = null;
    const input_click = this.input_click.bind (this);
    this.addEventListener ("click", input_click);
    this.addEventListener ("dblclick", input_click);
  }
  updated (changed_props)
  {
    const input = this.input_;
    if (input && input.valid_change === undefined)
      {
	input.value = this.innerText;
	if (this.selectall)
	  input.select();
	input.focus();
	if (input === Util.activeElement())
	  {
	    // allow input
	    input.valid_change = 1;
	  }
      }
  }
  activate()
  {
    this.editable_ = true;
  }
  input_click (event)
  {
    const clicks = 0 | this.clicks;
    if ((clicks === 1 && event.type === "click") ||
	(clicks === 2 && event.type === "dblclick")) {
      event.stopPropagation();
      this.activate();
    }
  }
  input_keydown (event)
  {
    event.stopPropagation();
    const input = this.input_;
    const esc = Util.match_key_event (event, 'Escape');
    if (esc)
      input.valid_change = 0;
    if (esc || Util.match_key_event (event, 'Enter'))
      input.blur();
  }
  input_change (event)
  {
    Util.prevent_event (event);
    const input = this.input_;
    if (input.valid_change)
      input.valid_change = 2;
  }
  input_blur (event)
  {
    const input = this.input_;
    if (input.valid_change == 2)
      this.dispatchEvent (new CustomEvent ('change', {
	detail: { value: input.value }
      }));
    input.valid_change = 0;
    input.disabled = true;
    // give async 'change' handlers some time for updates
    setTimeout (() => this.editable_ = false, 67);
  }
}

customElements.define ('b-editable', BEditable);
