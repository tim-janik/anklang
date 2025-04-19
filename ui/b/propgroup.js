// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

/** @class B-PROPGROUP
 * A property group contains a group title, several rows and each row contains a number of properties.
 * @property {string} name - Group name.
 * @property {Array<any>} props - List of properties with cached information and layout rows.
 * @property {boolean} readonly - Make this component non editable for the user.
 */

import { LitComponent, html, JsExtract, docs, repeat } from '../little.js';
import * as Util from '../util.js';

// == STYLE ==
Extra_css`
@reference "../tailwind.css";
b-propgroup {
  @apply vflex;
  padding: 5px;
  justify-content: space-evenly;
  border-radius: var(--b-button-radius);
  background: var(--b-device-area1);
  &:nth-child(2n) {
    background: var(--b-device-area2);
  }
  .b-propgroup-title {
    @apply flex grow justify-center text-center;
  }
  --b-prop-width: 3rem;
  --b-prop-height: 2.5rem;
  --b-prop-gap: calc(2 * 0.125rem);
  .b-propgroup-row > * { @apply justify-center; }
  .b-propgroup-row > * + * { margin-left: var(--b-prop-gap); }
  .b-propgroup-row:not(:last-child) { margin-bottom: var(--b-prop-gap); }
}
.b-propgroup-row b-textinput {
  width: calc(var(--b-prop-gap) * 4 + 5 * var(--b-prop-width));
}
.b-propgroup-row {
  > * { width: var(--b-prop-width); }
  > span { margin-top: calc(2 * 0.125rem); }
}`;

// == HTML ==
const GROUP_HTML = (t, prop_rows) => [
  html`<span class="b-propgroup-title"> ${t.name} </span>`,
  repeat (prop_rows, (row_props, index) =>
    html`
      <div class="b-propgroup-row grid justify-evenly" style="grid-auto-flow: column dense">
	${repeat (row_props, (prop, i) => PROP_HTML (t, prop, i))}
      </div>`)
];
const PROP_HTML = (t, prop) => {
  let p;
  switch (prop_case (prop)) {
    case 'B': p = html` <b-toggle ?disabled=${prop.readonly} .value=${prop.value_.num}
			  label="" @valuechange=${e => prop.set_normalized (!!e.target.value)} ></b-toggle> `; break;
    case 'C': p = html` <b-choiceinput small="1" indexed="1" ?disabled=${t.readonly} .prop="${prop}"
			  label=${prop.label_} title=${prop.title_}
			  value=${prop.value_.val} @valuechange=${e => prop.apply_ (e.target.value)} ></b-choiceinput> `; break;
    case 'K': p = html` <b-knob ?disabled=${prop.readonly} .prop="${prop}" ></b-knob> `; break;
    case 'T': p = html` <b-textinput ?disabled=${prop.readonly} .prop=${prop}
			  label=${prop.label_} title=${prop.title_} ></b-textinput> `; break;
    default:  p = html` <span>${prop.nick_}</span> `; break;
  }
  const p_label = html` <span class="text-center text-[90%]" style="grid-row: 2/3"> ${prop.nick_} </span> `;
  return html` ${p} \n ${p_label} `;
};
function prop_case (prop)
{
  const hints = ':' + prop.hints_ + ':';
  if (hints.search (/:choice:/) >= 0)
    return 'C';	// choice
  if (hints.search (/:toggle:/) >= 0)
    return 'B';	// toggle
  if (hints.search (/:text:/) >= 0)
    return 'T';	// text
  if (prop.is_numeric_)
    return 'K';	// knob
  return '?';
}

// == SCRIPT ==
class BPropGroup extends LitComponent {
  createRenderRoot() { return this; }
  render()
  {
    const prop_rows = this.assign_layout_rows (this.props);
    return GROUP_HTML (this, prop_rows);
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
      ; // prop_rows are generated on the fly
  }
  prop_class (prop)
  {
    const hints = ':' + prop.hints_ + ':';
    let c = '';
    return ' ' + c + ' ';
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
