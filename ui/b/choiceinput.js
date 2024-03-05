// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class BChoiceInput
 * @description
 * The <b-choiceinput> element provides a choice popup to choose from a set of options.
 * It supports the Vue
 * [v-model](https://vuejs.org/v2/guide/components-custom-events.html#Customizing-Component-v-model)
 * protocol by emitting an `input` event on value changes and accepting inputs via the `value` prop.
 * ### Props:
 * *value*
 * : The choice value to be displayed.
 * *choices*
 * : List of choices: `[ { icon, label, blurb }... ]`
 * *title*
 * : Optional title for the popup menu.
 * *label*
 * : A label used to extend the tip attached to the choice component.
 * *small*
 * : Reduce padding and use small layout.
 * *prop*
 * : If `label` is unspecified, it can be fetched from `prop->label` instead.
 * ### Events:
 * *valuechange*
 * : Event emitted whenever the value changes, which is provided as `event.target.value`.
 */

import { LitComponent, html, nothing, JsExtract, live, docs, ref } from '../little.js';
import * as Util from '../util.js';
import { get_uri } from '../dom.js';

// <STYLE/>
JsExtract.scss`
b-choiceinput {
  display: flex;
  flex-basis: auto;
  flex-flow: row nowrap;
  align-items: stretch;
  align-content: stretch;
  position: relative;
  margin: 0;
  white-space: nowrap;
  user-select: none;
  &.b-choice-big {
    justify-content: left; text-align: left;
    padding: .1em 0;
  }
  &.b-choice-small {
    justify-content: center; text-align: center;
    padding: 0;
  }
  b-objecteditor &.b-choice {
    text-align: left;
    justify-content: left;
    padding: 0;
    flex: 1 1 auto;
  }
  .-nick {
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    width: 1em; flex: 1 1 auto;
  }
  .-arrow {
    flex: 0 0 auto; width: 1em;
    margin: 0 0 0 .3em;
  }
  &.b-choice-small .-arrow {
    display: none;
  }
  .b-choice-current {
    align-self: center;
    padding-left: 0.3em;
    width: 100%;
    margin: 0;
    white-space: nowrap; overflow: hidden;
    .b-choice-big & {
      flex-grow: 1;
      justify-content: space-between;
      padding: $b-button-radius 0 $b-button-radius .5em;
    }
    .b-choice-small & {
      width: 100%; height: 1.33em;
      justify-content: center;
      padding: 2px;
    }
    @include b-style-outset();
  }
}
.b-choiceinput-contextmenu {
  .b-choice-label { display: block; white-space: pre-line; }
  .b-choice-line1,
  .b-choice-line2 { display: block; white-space: pre-line; font-size: 90%; color: $b-style-fg-secondary; }
  .b-choice-line3 { display: block; white-space: pre-line; font-size: 90%; color: $b-style-fg-notice; }
  .b-choice-line4 { display: block; white-space: pre-line; font-size: 90%; color: $b-style-fg-warning; }
  button {
    &:focus, &.active, &:active {
      .b-choice-line1, .b-choice-line2, .b-choice-line3,
      .b-choice-line4 { filter: $b-style-fg-filter; } //* adjust to inverted menuitem */
    } }
  button {
    white-space: pre-line;
  }
}
`;


// <HTML/>
const HTML = (t, d) =>  html`
  <h-flex class="b-choice-current" ${ref (h => t.pophere = h)} tabindex="0" >
    <span class="-nick">${t.nick()}</span>
    <span class="-arrow" > ⬍ <!-- ▼ ▽ ▾ ▿ ⇕ ⬍ ⇳ --> </span>
  </h-flex>
`;
const CONTEXTMENU_HTML = (t) =>  html`
  <b-contextmenu class="b-choiceinput-contextmenu" ${ref (h => t.cmenu = h)}
    @activate=${e => t.activate (get_uri (e.detail))} @close=${e => t.need_cmenu = false} >
    <b-menutitle style=${!t.title ? 'display:none' : ''}> ${t.title} </b-menutitle>
    ${ t.mchoices().map ((c, index) => CONTEXTMENU_ITEM (t, c) )}
  </b-contextmenu>
`;
const CONTEXTMENU_ITEM = (t, c) => html`
  <button uri=${c.ident} ic=${c.icon} >
    <span class="b-choice-label ${c.labelclass}" > ${ c.label } </span>
    <span class="b-choice-line1 ${c.line1class}" > ${ c.blurb } </span>
    <span class="b-choice-line2 ${c.line2class}" > ${ c.line2 } </span>
    <span class="b-choice-line3 ${c.line3class}" > ${ c.notice } </span>
    <span class="b-choice-line4 ${c.line4class}" > ${ c.warning } </span>
  </button>
`;

// <SCRIPT/>
class BChoiceInput extends LitComponent {
  createRenderRoot() { return this; }
  render()
  {
    return [
      HTML (this),
      !this.need_cmenu ? nothing : CONTEXTMENU_HTML (this),
    ];
  }
  static properties = {
    value:	{ type: String, },
    title:	{ type: String, },
    label:	{ type: String, },
    small:	{ type: Boolean, },
    prop:	{ type: Object, },
    choices:	{ type: Array },
    need_cmenu: { state: true }, // internal
  };
  constructor() {
    super();
    Util.define_reactive (this, {
      value_: { value: '' },	//     this.value_ = '';
    });
    this.value = '';
    this.small = false;
    this.prop = null;
    this.label = '';
    this.choices = [];
    this.need_cmenu = false;
    this.cmenu = null;
    this.pophere = null;
    const this_popup_menu = this.popup_menu.bind (this);
    this.addEventListener ('click', this_popup_menu);
    this.addEventListener ('mousedown', this_popup_menu);
    this.addEventListener ('keydown', this.keydown.bind (this));
  }
  updated (changed_props)
  {
    if (changed_props.has ('small')) {
      this.classList.remove (this.small ? 'b-choice-big' : 'b-choice-small');
      this.classList.add (!this.small ? 'b-choice-big' : 'b-choice-small');
    }
    if (changed_props.has ('value'))
      this.value_ = this.value; // may cause re-render
    this.setAttribute ('data-tip', this.data_tip());
  }
  mchoices()
  {
    const mchoices = [];
    for (let i = 0; i < this.choices.length; i++) {
      const c = Object.assign ({}, this.choices[i]);
      mchoices.push (c);
    }
    return mchoices;
  }
  current()
  {
    const mchoices = this.mchoices();
    for (let i = 0; i < mchoices.length; i++)
      if (mchoices[i].ident == this.value_)
	return mchoices[i];
    return {};
  }
  data_tip()
  {
    const choice = this.current();
    let tip = "**CLICK** Select Choice";
    const plabel = this.label || this.prop?.label_;
    if (!plabel || !choice.label)
      return tip;
    let val = "**" + plabel + "** ";
    val += choice.label;
    return val + " " + tip;
  }
  nick()
  {
    const choice = this.current();
    return choice.label || "";
  }
  activate (uri)
  {
    if (this.cmenu) {
      // close popup to remove focus guards
      this.cmenu.close();
      this.need_cmenu = false;
    }
    this.value_ = uri;
    this.value = this.value_; // becomes Event.target.value
    this.dispatchEvent (new Event ('valuechange', { composed: true }));
  }
  popup_menu (event)
  {
    if (!this.cmenu) { // force synchronous rendering to create this.cmenu
      this.need_cmenu = true; // does this.requestUpdate();
      this.performUpdate();
    }
    this.pophere.focus();
    this.cmenu.popup (event, { origin: this.pophere, focus_uri: this.value });
  }
  keydown (event)
  {
    if (this.cmenu?.open)
      return;
    // allow selection changes with UP/DOWN while menu is closed
    if (event.keyCode == Util.KeyCode.DOWN || event.keyCode == Util.KeyCode.UP)
      {
	event.preventDefault();
	event.stopPropagation();
	const mchoices = this.mchoices();
	const choice = this.current();
	if (choice.ident)
	  for (let i = 0; i < mchoices.length; i++)
	    if (mchoices[i].ident == choice.ident) {
	      const index = i + (event.keyCode == Util.KeyCode.DOWN ? +1 : -1);
	      if (index >= 0 && index < mchoices.length)
		this.activate (mchoices[index].ident);
	      break;
	    }
      }
    else if (event.keyCode == Util.KeyCode.ENTER)
      this.popup_menu (event);
  }
}
customElements.define ('b-choiceinput', BChoiceInput);
