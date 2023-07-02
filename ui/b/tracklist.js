// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import { LitComponent, html, JsExtract, docs, ref, repeat } from '../little.js';

/** == B-TRACKLIST ==
 * A container for vertical display of Ase.Track instances.
 * ## Props:
 * *project*
 * : The *Ase.Project* containing playback tracks.
 */

// == STYLE ==
const STYLE = await JsExtract.fetch_css (import.meta);
JsExtract.scss`
@import 'mixins.scss';
$scroll-shadow-inset: 7px;
:host {
  display: flex; flex-direction: column; align-items: stretch;
  position: relative;
}
.grid {
  display: grid; flex-grow: 1;
  position: relative; inset: 0;
  padding: 0 calc(0.5 * $scroll-shadow-inset);
  align-items: stretch;
  grid-template-columns: min-content 3fr 2fr;
  grid-template-rows: min-content 1fr min-content;
  background-color: $b-tracklist-bg;
  overflow: hidden;
  // for :before box-shadow
}
.trackviews,
.partlists,
.cliplists {
  display: flex; flex-flow: column nowrap;
  align-items: flex-start; // needed for scroll-x
  padding: $scroll-shadow-inset 0;
  min-height: 0;
  > * { height: 46px; box-sizing: border-box; overflow: hidden; flex-grow: 0; flex-shrink: 0; }
}
.trackviews {
  grid-area: 2/1 / 3/2;
  overflow: scroll hidden;
}
.cliplists {
  grid-area: 2/2 / 3/3;
  overflow: scroll;
}
.partlists {
  grid-area: 2/3 / 3/4;
  overflow: scroll;
}
.scrollshadow {
  grid-area: 2/1 / 3/4;
  position: absolute; top: 0; left: 0; height: 200%;
  width: calc(100% + 200px); margin-left: -100px;
  // add inner box-shadow to indicate scrolling borders
  z-index: 1; pointer-events: none; user-select: none;
  box-shadow: black 0 0 $scroll-shadow-inset 0px inset;
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
  <div class="grid" >
    <div style="grid-area: 1/1 / 2/4;"> <!--HEADER--> </div>
    <div class="trackviews" ${ref (h => t.trackviews = h)} >
      ${repeat (t.wproject_.all_tracks, track => track.$id, (track, idx) => TRACKVIEW_HTML (t, track, idx, t.wproject_.all_tracks.length))}
    </div>
    <div class="cliplists" ${ref (h => t.cliplists = h)} >
      ${repeat (t.wproject_.all_tracks, track => track.$id, (track, idx) => CLIPLIST_HTML (t, track, idx, t.wproject_.all_tracks.length))}
    </div>
    <div class="partlists" ${ref (h => t.partlists = h)} >
      ${repeat (t.wproject_.all_tracks, track => track.$id, (track, idx) => CLIPLIST_HTML (t, track, idx, t.wproject_.all_tracks.length))}
    </div>
    <div style="grid-area: 3/1 / 4/4;"> <!--FOOTER--> </div>
    <div class="scrollshadow"></div>
  </div>
`;

// == SCRIPT ==
import * as Ase from '../aseapi.js';
import * as Util from '../util.js';

const OBJECT_PROPERTY = { attribute: false };

class BTrackList extends LitComponent {
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
    this.trackviews = null;
    this.cliplists = null;
    this.partlists = null;
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
    if (event.path[0] === this ||
	event.path[0].classList.contains ('trackviews'))
      this.project.create_track();
  }
}
customElements.define ('b-tracklist', BTrackList);
