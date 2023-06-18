// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { LitElement, html, css, postcss, docs, ref } from '../little.js';
import * as Util from '../util.js';

/** ## Part-List
 * The Part-List allows to arrange Clip objects for playback.
 */

// == STYLE ==
const STYLE = await postcss`
@import 'mixins.scss';
:host {
  display: flex;
  position: relative;
}
b-clipview {
  margin: 0 1px;
  width: calc(5 * $b-clipthumb-width);
  flex-shrink: 0; flex-grow: 0;
}
`;

// == HTML ==
const HTML = (t, d) => [
  t.wtrack.arranger_parts.map ((clip, index) =>
    html`  <b-clipview .clip=${clip} index=${index} .track=${t.track} trackindex=${t.trackindex} ></b-clipview>`
  ),
];

// == SCRIPT ==
import * as Ase from '../aseapi.js';

const OBJECT_PROPERTY = { attribute: false };
const NUMBER_ATTRIBUTE = { type: Number, reflect: true }; // sync attribute with property

class BPartList extends LitElement {
  static styles = [ STYLE ];
  static properties = {
    track:	OBJECT_PROPERTY,
    trackindex:	NUMBER_ATTRIBUTE,
  };
  render()
  {
    const d = {};
    return HTML (this, d);
  }
  constructor()
  {
    super();
    this.track = null;
    this.wtrack = { arranger_parts: [] };	// dummy
    this.trackindex = -1;
    this.setAttribute ('data-f1', "#part-list");
    this.addEventListener ('dblclick', this.dblclick.bind (this));
  }
  updated (changed_props)
  {
    if (changed_props.has ('track'))
      {
	const weakthis = new WeakRef (this); // avoid strong wtrack->this refs for automatic cleanup
	this.wtrack = Util.wrap_ase_object (this.track, { arranger_parts: [] }, () => weakthis.deref()?.requestUpdate());
      }
  }
  dblclick (event)
  {
    this.track.create_part (0);
  }
}

customElements.define ('b-partlist', BPartList);
