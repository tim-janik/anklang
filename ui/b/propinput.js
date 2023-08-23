// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import { LitComponent, html, render, noChange, JsExtract, docs, ref } from '../little.js';

/** @class BPropInput
 * @description
 * A property input element, usually for numbers, toggles or menus.
 * ### Props:
 * *labeled*
 * : Display property name next to its editing field.
 * *prop*
 * : Contains the extended property being edited.
 * *readonly*
 * : Make this component non editable for the user.
 */

// == STYLE ==
JsExtract.scss`
:host(b-propinput) {
  display: flex; justify-content: center;
  .b-propinput-ldiv[class]::before { content: "\200b"; /* zero width character to force line height */ }
  .b-propinput-toggle { height: 2em; }
  .b-propinput-choice { height: 2em; width: 2.3em; }
  .b-propinput-span {
    pointer-events: none; user-select: none;
    max-width: 100%;
    white-space: nowrap;
    text-align: center;
    margin-right: -999em; /* avoid widening parent */
    overflow: hidden;
    z-index: 9;
  }
  &.b-propinput:hover .b-propinput-span { overflow: visible; }
}`;

// == HTML ==
const HTML = (t, d) => [
  t.istype ('toggle') &&
  html`
    <b-toggle class="b-propinput-toggle" label=""
      .value=${t.prop.value_.num} @valuechange=${e => t.set_num (e.target.value)} ></b-toggle>`,
  t.istype ('choice') &&
  html`
    <b-choiceinput class="b-propinput-choice" small="1" indexed="1"
      value=${t.prop.value_.val} @valuechange=${e => t.prop.apply_ (e.target.value)}
      label=${t.prop.label_} title=${t.prop.title_} .choices=${t.prop.value_.choices} ></b-choiceinput>`,
  !t.labeled || !t.prop.nick_ ? '' :
  html`
    <span class="b-propinput-span">${t.prop.nick_}</span> `,
];

// == SCRIPT ==
const OBJ_ATTRIBUTE = { type: Object, reflect: true };    // sync attribute with property
const BOOL_ATTRIBUTE = { type: Boolean, reflect: true };  // sync attribute with property

class BPropInput extends LitComponent {
  createRenderRoot() { return this; }
  render()
  {
    const d = {};
    return HTML (this, d);
  }
  static properties = {
    prop:	OBJ_ATTRIBUTE,
    labeled:	BOOL_ATTRIBUTE,
    readonly:	BOOL_ATTRIBUTE,
  };
  constructor()
  {
    super();
    this.labeled = true;
    this.readonly = false;
    this.prop = null;
  }
  updated (changed_props)
  {
    if (changed_props.has ('prop')) {
      changed_props['prop'] && changed_props['prop'].delnotify_ (this.request_update_);
      this.prop && this.prop.addnotify_ (this.request_update_);
    }
    this.value_changed();
  }
  disconnectedCallback()
  {
    this.prop && this.prop.delnotify_ (this.request_update_);
  }
  value_changed()
  {
    if (!this.prop) {
      this.setAttribute ('data-bubble', null);
      return;
    }
    let b = '';
    if (this.prop.label_ && this.prop.description_)
      b += '###### '; // label to title
    if (this.prop.label_)
      b += this.prop.label_;
    if (this.prop.unit_)
      b += '  (**' + this.prop.unit_ + '**)';
    if (this.prop.label_ || this.prop.unit_)
      b += '\n';
    if (this.prop.description_)
      b += this.prop.description_;
    this.setAttribute ('data-bubble', b);
    App.zmove(); // force bubble changes to be picked up
  }
  istype (proptyppe)
  {
    if (this.prop?.is_numeric_)
      {
	const hints = ':' + this.prop.hints_ + ':';
	if (proptyppe === 'toggle' && hints.search (/:toggle:/) >= 0)
	  return true;
	if (proptyppe === 'choice' && hints.search (/:choice:/) >= 0)
	  return true;
      }
    return '';
  }
  set_num (nv)
  {
    if (this.readonly)
      return;
    this.prop.set_normalized (nv);
  }
}
customElements.define ('b-propinput', BPropInput);
