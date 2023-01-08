// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
import { LitElement, html, css, postcss, docs, ref, repeat } from '../little.js';

/** == B-TRACKLIST ==
 * A container for vertical display of Ase.Track instances.
 * ## Props:
 * *project*
 * : The *Ase.Project* containing playback tracks.
 */

// == STYLE ==
const STYLE = await postcss`
@import 'mixins.scss';
$scroll-shadow-inset: 7px;
:host {
  display: flex; flex-flow: row nowrap;
  //* Layout: Tracks Spacer Clips Spacer Parts Scrollbar */
  // grid-template-columns: min-content 3px 17fr 3px 10px 20px;
  grid-template-columns: min-content 2fr 1fr;
  //* Layout: Header Spacer Main Spacer Footer */
  grid-template-rows: repeat(999,min-content) 1fr; // min-content 3px 1fr 3px 20px;
  background-color: $b-tracklist-bg;
  overflow: hidden;
  // for :before box-shadow
  position: relative; padding: 0 calc(0.5 * $scroll-shadow-inset);
}
:host::before { // add inner box-shadow to indicate scrolling borders
  content: "";
  position: absolute; top: 0; left: 0; width: 100%; height: 100%;
  z-index: 999; pointer-events: none; user-select: none;
  box-shadow: black 0 0 $scroll-shadow-inset 0px inset;
}
.trackviews {
  display: flex; flex-flow: column nowrap;
  grid-column: 1/2; grid-row: 1/2;
  > * { height: 46px; box-sizing: border-box; overflow: hidden; flex-grow: 0; flex-shrink: 0; }
  padding: $scroll-shadow-inset 0;
  overflow: hidden;
}
.cliplists {
  display: flex; flex-flow: column nowrap;
  grid-column: 2/3; grid-row: 1/2;
  > * { height: 46px; box-sizing: border-box; overflow: hidden; flex-grow: 0; flex-shrink: 0; }
  padding: $scroll-shadow-inset 0;
  overflow: hidden scroll;
}
`;

// == HTML ==
const TRACKVIEW_HTML = (t, track, idx, len) => html`
  <b-trackview .track=${track} trackindex=${idx} ></b-trackview>
`;
const CLIPLIST_HTML = (t, track, idx, len) => html`
  <b-cliplist .track=${track} trackindex=${idx} ></b-cliplist>
`;
const HTML = (t, d) => html`
  <div class="trackviews" ${ref (h => t.trackviews = h)} >
    ${repeat (t.wproject_.all_tracks, track => track.$id, (track, idx) => TRACKVIEW_HTML (t, track, idx, t.wproject_.all_tracks.length))}
  </div>
  <div class="cliplists" ${ref (h => t.cliplists = h)} >
    ${repeat (t.wproject_.all_tracks, track => track.$id, (track, idx) => CLIPLIST_HTML (t, track, idx, t.wproject_.all_tracks.length))}
  </div>
  <div class="cliplists" ${ref (h => t.partlists = h)} >
    ${repeat (t.wproject_.all_tracks, track => track.$id, (track, idx) => CLIPLIST_HTML (t, track, idx, t.wproject_.all_tracks.length))}
  </div>
`;

// == SCRIPT ==
import * as Ase from '../aseapi.js';
import * as Util from '../util.js';

const OBJECT_PROPERTY = { attribute: false };

class BTrackList extends LitElement {
  static styles = [ STYLE ];
  static properties = {
    project: OBJECT_PROPERTY,
  };
  render()
  {
    return HTML (this);
  }
  constructor()
  {
    super();
    this.project = null;
    this.wproject_ = { all_tracks: [] };
    this.addEventListener ('dblclick', this.list_dblclick.bind (this));
  }
  updated (changed_props)
  {
    if (changed_props.has ('project'))
      {
	const weakthis = new WeakRef (this); // avoid strong wproject_->this refs for automatic cleanup
	this.wproject_ = Util.wrap_ase_object (this.project, { all_tracks: [] }, () => weakthis.deref()?.requestUpdate());
      }
    if (!this.cliplists.onscroll)
      this.cliplists.onscroll = event => this.trackviews.scrollTop = this.partlists.scrollTop = this.cliplists.scrollTop;
    if (!this.partlists.onscroll)
      this.partlists.onscroll = event => this.trackviews.scrollTop = this.cliplists.scrollTop = this.partlists.scrollTop;
  }
  list_dblclick (event)
  {
    debug (event);
    if (event.path[0].classList.contains ('trackviews'))
      this.project.create_track();
  }
}
customElements.define ('b-tracklist', BTrackList);
