// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import { LitComponent, html, JsExtract, live, docs, ref } from '../little.js';
import * as Util from '../util.js';

/** @class BObjectEditor
 * @description
 * The <b-objecteditor> element is a field-editor for object input.
 * A copy of the input value is edited, update notifications are provided via
 * an `input` event.
 * ### Properties:
 * *value*
 * : Object with properties to be edited.
 * *readonly*
 * : Make this component non editable for the user.
 * ### Events:
 * *input*
 * : This event is emitted whenever the value changes through user input or needs to be constrained.
 */

// <STYLE/>
JsExtract.scss`
b-objecteditor {
  display: grid;
  grid-gap: 0.6em 0.5em;
  .b-objecteditor-clear {
    -font-size: 1.1em; // @include b-font-weight-bolder();
    color: #888; background: none; padding: 0 0 0 0.5em; margin: 0;
    outline: none; border: 1px solid rgba(0,0,0,0); border-radius: $b-button-radius;
    &:hover			{ color: #eb4; }
    &:active			{ color: #3bf; }
  }
  > * {
    /* avoid visible overflow for worst-case resizing */
    min-width: 0;
    overflow-wrap: break-word;
  }
  .b-objecteditor-group {
    align-items: center;
    margin-top: 0.5em;
    &:first-child { margin-top: 0; }
    white-space: nowrap;
    .b-objecteditor-label { text-overflow: ellipsis; overflow: hidden; @include b-font-weight-bold(); }
  }
  .b-objecteditor-flabel {
    padding: 0 0.1em 0 0;
    min-width: 3em;
    white-space: nowrap;
    text-overflow: ellipsis;
    overflow: hidden;
  }
  .b-objecteditor-field {
    justify-self: flex-end;
    justify-content: space-between;
    white-space: nowrap;
    max-width: 32em;
    width: 50vw;
  }
  .b-objecteditor-value {
    width: 100%;
  }
}`;

// <HTML/>
const GROUP_HTML = (t, group) =>  html`
<h-flex class="b-objecteditor-group" style="grid-column: 1 / span 3" >
  <span class="b-objecteditor-label" style="flex-grow: 0;" >${group.name}</span>
  <hr style="flex-grow: 1; margin-left: 0.5em; min-width: 5em"></hr>
</h-flex>
`;
const PROP_HTML = (t, prop, INPUT_TAG) =>  html`
<span class="b-objecteditor-flabel" style="grid-column: 1" data-bubble=${prop.descr_ || prop.blurb_} >${prop.label_}</span>
<h-flex class="b-objecteditor-field" style="grid-column: 2 / span 2" >
  <span class="b-objecteditor-value" data-bubble=${prop.blurb_ || prop.descr_} style="text-align: right" >
    ${INPUT_TAG}
  </span>
  <span><span class="b-objecteditor-clear" @click=${e => prop.reset()} data-bubble=${"Reset " + prop.label_} > ⊗  </span></span>
</h-flex>
`;
const NUMBER_HTML = (t, prop) => html`
  <b-numberinput class=${'b-objecteditor--' + prop.ident_}
    value=${prop.value_.val} @valuechange=${e => prop.apply_ (e.target.value)}
    min=${prop.min_} max=${prop.max_}
    ></b-numberinput>`;
const TEXT_HTML = (t, prop) => html`
  <b-textinput class=${'b-objecteditor--' + prop.ident_}
    .prop=${prop} ></b-textinput>`;
const SWITCH_HTML = (t, prop) => html`
  <b-switchinput class=${'b-objecteditor--' + prop.ident_}
    ?value=${prop.value_.val} @valuechange=${e => prop.apply_ (e.target.value)}
    ></b-switchinput>
`;
const CHOICE_HTML = (t, prop) => html`
  <b-choiceinput class=${'b-objecteditor--' + prop.ident_}
    value=${prop.value_.val} @valuechange=${e => prop.apply_ (e.target.value)}
    title=${prop.title_} .choices=${prop.value_.choices}
    ></b-choiceinput>`;

// <SCRIPT/>
class BObjectEditor extends LitComponent {
  createRenderRoot() { return this; }
  render()
  {
    const content = [];
    for (const group of this.gprops) {
      content.push (GROUP_HTML (this, group));
      for (const prop of group.props) {
	const component_html = prop.b_objecteditor_component_html_ (this, prop);
	content.push (PROP_HTML (this, prop, component_html));
      }
    }
    return content;
  }
  static properties = {
    readonly:	{ type: Boolean, },
    augment:    { type: Function, },
    value:	{ type: Array, },
    gprops:     { state: true }, // internal
  };
  constructor() {
    super();
    this.readonly = false;
    this.augment = null;
    this.value = [];
    this.gprops = [];
  }
  replace_disconnectors (new_disconnectors)
  {
    if (this.disconnectors)
      while (this.disconnectors.length)
	this.disconnectors.pop().call();
    this.disconnectors = new_disconnectors;
  }
  updated (changed_props)
  {
    if (changed_props.has ('value')) {
      const async_fetch = async () => {
	this.gprops = await this.list_fields_ (this.value);
      };
      async_fetch();
    }
  }
  async list_fields_ (proplist)
  {
    const disconnectors = [];
    const groups = {};
    const pending_xprops = [];
    for (const prop of proplist)
      {
	const augment = async xprop => {
	  if (this.augment)
	    await this.augment (xprop);
	};
	pending_xprops.push (Util.extend_property (prop, cb => disconnectors.push (cb), augment));
      }
    for (const pending_prop of pending_xprops)
      {
	const xprop = await pending_prop;
	if (!groups[xprop.group_])
	  groups[xprop.group_] = [];
	groups[xprop.group_].push (xprop);
      }
    const grouplist = []; // [ { name, props: [richprop, ...] }, ... ]
    for (const k of Object.keys (groups))
      {
	grouplist.push ({ name: k, props: groups[k] });
	for (const xprop of groups[k])
	  {
	    let component_html;	// component type
	    if (xprop.hints_.search (/:range:/) >= 0)
	      component_html = NUMBER_HTML;
	    else if (xprop.hints_.search (/:bool:/) >= 0)
	      component_html = SWITCH_HTML;
	    else if (xprop.has_choices_)
	      component_html = CHOICE_HTML;
	    else
	      component_html = TEXT_HTML;
	    xprop.b_objecteditor_component_html_ = component_html;
	  }
	Object.freeze (groups[k]);
      }
    this.replace_disconnectors (disconnectors);
    return grouplist;
  }
  disconnectedCallback()
  {
    super.disconnectedCallback();
    // release property change notifiers
    this.replace_disconnectors();
  }
}
customElements.define ('b-objecteditor', BObjectEditor);
