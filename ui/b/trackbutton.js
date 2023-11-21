// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import { LitComponent, html, JsExtract, live, docs, ref } from '../little.js';
import * as Util from '../util.js';

/** @class BTrackButton
 * @description
 * The <b-trackvolume> element is an editor for the track volume, implemented as thin wrapper
 * arount <b-toggle>.
 * ### Properties:
 * *label*
 * : Either "M" for mute property or "S" for solo property
 * *track*
 * : The track
 * *value*
 * : Current value
 */

// <STYLE/>
JsExtract.scss`
`;

// <HTML/>
const HTML = t => [
html`
<b-toggle label="${t.label}" .value="${t.value}" @valuechange="${event => t.set_value (event.target.value)}"></b-toggle>
`
];

const OBJ_ATTRIBUTE = { type: Object, reflect: true };    // sync attribute with property
const STRING_ATTRIBUTE = { type: String, reflect: true }; // sync attribute with property
const BOOL_ATTRIBUTE = { type: Boolean, reflect: true }; // sync attribute with property

// <SCRIPT/>
class BTrackButton extends LitComponent {
  createRenderRoot() { return this; }
  render() { return HTML (this); }
  static properties = {
    track:      OBJ_ATTRIBUTE,
    label:      STRING_ATTRIBUTE,
    value:      BOOL_ATTRIBUTE
  };
  constructor() {
    super();
    this.label = "";
    this.track = null;
    this.prop = null;
  }
  set_value (value)
  {
    this.prop.set_value (value);
  }
  updated (changed_props)
  {
    this.update_value();
  }
  async update_value()
  {
    if (!this.prop)
      {
        let property;
        if (this.label == "M")
          property = "mute";
        if (this.label == "S")
          property = "solo";
        let prop = await this.track.access_property (property);
        this.prop = prop;
        this.prop.on ("notify", args => {
          if (args.detail == property)
            this.update_value();
        });
      }
    this.value = await this.prop.get_value();
  }
}
customElements.define ('b-trackbutton', BTrackButton);
