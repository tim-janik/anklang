// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import { LitComponent, html, JsExtract, live, docs, ref } from '../little.js';
import * as Util from '../util.js';

/** @class BTextInput
 * @description
 * The <b-textinput> element is a field-editor for text input.
 *
 * ### Properties:
 * *value*
 * : Contains the text string being edited.
 * *readonly*
 * : Make this component non editable for the user.
 *
 * ### Events:
 * *valuechange*
 * : Event emitted whenever the value changes, which is provided as `event.target.value`.
 */

// == STYLE ==
Extra_css`
@reference "../tailwind.css";
b-textinput input {
  outline-width: 0; border: none;
  text-align: left;
  padding-left: var(--b-button-radius); padding-right: var(--b-button-radius);
  @apply b-style-inset;
}
`;

// <HTML/>
const HTML = t =>
html`
<label>
  <input ${ref (h => t.input_element = h)} type="text" ?readonly=${t.readonly}
	 style="width: 100%; min-width: 2.5em" @input=${t.handle_input}
         @click=${t.textinput_click}
	 placeholder=${t.placeholder} .value=${live (t.value)} >
</label>
`;

// <SCRIPT/>
function prop_info (prop, key) {
  const md = prop?.metadata;
  if (!md || !key) return "";
  const eq = key.length;
  for (let kv of md) {
    if (kv[eq] === '=' && kv.startsWith (key))
      return kv.substring (eq + 1);
  }
  return "";
}

class BTextInput extends LitComponent {
  createRenderRoot() { return this; }
  render() { return HTML (this); }
  input_element = null;
  static properties = {
    prop:	 { type: Object, reflect: true },
    placeholder: { type: String, },
    readonly:	 { type: Boolean, },
  };
  constructor()
  {
    super();
    this.value = '';
    this.prop = null;
    this.placeholder = '';
    this.readonly = false;
  }
  updated (changed_props)
  {
    if (changed_props.has ('prop')) {
      if (this.prop) {
	this.prop.name; // access field, we need it later on.
	this.prop.value; // access field, we need it later on.
	this.prop.metadata; // access field, we need it later on.
      }
    }
    const value = this.prop ? this.prop.value_.val : "";
    if (value !== this.value) {
      this.value = value;
      this.request_update ('value');
    }
  }
  disconnectedCallback()
  {
    super.disconnectedCallback();
    this.prop && this.prop.delnotify_ (this.request_update);
  }
  handle_input (event)  	// emit 'input' with constrained value
  {
    const constrainedvalue = this.constrain (this.input_element.value);
    if (constrainedvalue !== this.value) {
      this.value = constrainedvalue;
      this.prop?.apply_ (this.value);
    }
  }
  constrain (txt)
  {
    return '' + txt;
  }
  async textinput_click (event)
  {
    if (!prop_info (this.prop, "extensions"))
      return;
    const opt = {
      title:  _('Select File'),
      button: _('Open File'),
      cwd:    "~MUSIC",
      // TODO: filter by extensions
    };
    const filename = await Shell.select_file (opt);
    if (!filename)
      return;
    this.prop.value = filename;
  }
}
customElements.define ('b-textinput', BTextInput);
