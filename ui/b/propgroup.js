// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

/** @class B-PROPGROUP
 * A property group is a collection of properties.
 * @property {string} name - Group name.
 * @property {Array<any>} props - List of properties with cached information and layout rows.
 * @property {boolean} readonly - Make this component non editable for the user.
 */

import { LitComponent, html, JsExtract, docs } from '../little.js';
import * as Util from '../util.js';

// == STYLE ==
JsExtract.scss`
b-propgroup {
  @include v-flex();
  padding: 5px;
  justify-content: space-evenly;
  border-radius: $b-button-radius;
  background: $b-device-area1;
  &:nth-child(2n) {
    background: $b-device-area2;
  }
  .b-propgroup-title {
    display: flex;
    justify-content: center;
    text-align: center;
    flex-grow: 1;
  }
  .b-propgroup-input {
    flex-grow: 1;
  }
  .b-propgroup-row {
    justify-content: space-evenly;
    .b-propgroup-vprop {
      align-items: center;
      .b-propgroup-nick {
	font-size: 90%;
      }
    }
    //* emulate: .b-propgroup-row { gap: ...; } */
    & > *:not(:last-child) { margin-right: 7px; }
  }
  //* emulate: .b-propgroup { gap: ...; } */
  & > *:not(:last-child) { margin-bottom: 5px; }
  .b-propgroup-big {
    /*max-height: 2em;*/
  }
}`;

// == HTML ==
const GROUP_HTML = (t, proprows) => [
  html`
    <span class="b-propgroup-title"> ${t.name} </span> `,
  proprows.map ((row, index) => html`
    <h-flex class="b-propgroup-row" key="${index}">
      ${row.map (prop => PROP_HTML (t, prop))}
    </h-flex>`),
];
const PROP_HTML = (t, prop) => html`
  <v-flex class=${t.prop_class (prop) + " b-propgroup-vprop"}>
    <b-propinput class="b-propgroup-input" .prop="${prop}" ?readonly=${t.readonly}></b-propinput>
    <span class="b-propgroup-nick"> ${prop.nick_} </span>
  </v-flex>`;

// == SCRIPT ==
class BPropGroup extends LitComponent {
  createRenderRoot() { return this; }
  render()
  {
    const proprows = this.assign_layout_rows (this.props);
    return GROUP_HTML (this, proprows);
  }
  static properties = {
    name:     { type: String, reflect: true },
    props:    { type: Array, reflect: true },
    readonly: { type: Boolean, reflect: true },
  };
  constructor()
  {
    super();
    this.name = '';
    this.props = [];
    this.readonly = false;
  }
  updated (changed_properties)
  {
    if (changed_properties.has ('props'))
      ; // proprows are generated on the fly
  }
  prop_class (prop)
  {
    const hints = ':' + prop.hints_ + ':';
    let c = '';
    if (prop.is_numeric_ && hints.search (/:big:/) < 0) // FIXME: test "big" hint
      c += 'b-propgroup-big';
    return c;
  }
  assign_layout_rows (props)
  {
    // split properties into rows, according to lrow_
    const rows = [];
    for (const prop of props) {
      console.assert ('number' == typeof prop.lrow_);
      if (!rows[prop.lrow_]) {
        rows[prop.lrow_] = [];
        rows[prop.lrow_].index = prop.lrow_;
      }
      rows[prop.lrow_].push (prop);
    }
    // freezing avoids watchers
    return Object.freeze (rows);
  }
}
customElements.define ('b-propgroup', BPropGroup);
