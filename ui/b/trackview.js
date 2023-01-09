// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
import { LitElement, html, css, postcss, docs, ref } from '../little.js';

/** == B-TRACKVIEW ==
 * A Vue template to display a project's Ase.Track.
 * ## Props:
 * *project*
 * : The *Ase.project* containing playback tracks.
 * *track*
 * : The *Ase.Track* to display.
 */

// == STYLE ==
const STYLE = await postcss`
@import 'mixins.scss';
$b-trackview-level-height: 5px;
$b-trackview-level-space: 1px;
:host {
  display: flex;
  align-items: stretch;
  background-color: $b-button-border;
  border: 1px solid $b-button-border;
  border-top-left-radius: $b-button-radius;
  border-bottom-left-radius: $b-button-radius;
}
.-lvm-main { // level meter
  height: calc($b-trackview-level-height + $b-trackview-level-space + $b-trackview-level-height);
  position: relative;
  // push element onto own compositing layer to reduce rendering overhead
  will-change: auto;
}
.-lvm-levelbg {
  height: 100%;
  --db-zpc: 66.66%;
  background: linear-gradient(to right, #0b0, #bb0 var(--db-zpc), #b00);
}
.-lvm-covertip0, .-lvm-covermid0, .-lvm-covertip1, .-lvm-covermid1,
.-lvm-levelbg, .-lvm-coverspace      { position: absolute; width: 100%; }
.-lvm-covertip0, .-lvm-covermid0     { top: 0px; }
.-lvm-coverspace                     { top: calc($b-trackview-level-height - 0.25px); height: calc($b-trackview-level-space + 0.5px); }
.-lvm-covertip1, .-lvm-covermid1     { top: calc($b-trackview-level-height + $b-trackview-level-space); }
.-lvm-coverspace {
  background-color: rgba( 0, 0, 0, .80);
}
.-lvm-covertip0, .-lvm-covermid0, .-lvm-covertip1, .-lvm-covermid1 {
  height: $b-trackview-level-height;
  background-color: rgba( 0, 0, 0, .75);
  transform-origin: center right;
  will-change: transform;
  transform: scaleX(1);
}
.-lvm-covertip1, .-lvm-covermid1 {
  height: calc($b-trackview-level-height + 1px);
  // add 1px to cover for rounded coords
}
.b-trackview-control {
  display: flex;
  flex-direction: column;
  justify-content: space-between;
  margin-right: 5px;
  overflow: hidden;
}
:host([current-track]) .b-trackview-control { background-color: zmod($b-button-border, Jz+=25%); }
.-track-name {
  display: inline-flex; position: relative; width: 7em; overflow: hidden;
}
`;

// == HTML ==
const HTML = (t, d) => html`
  <div class="b-trackview-control" @click=${t.track_click0} @contextmenu=${t.menu_open}
       data-tip="**CLICK** Select Track **RIGHTCLICK** Track Menu" >
    <span class="-track-name" >
      <b-editable ${ref (h => t.trackname_ = h)} clicks="2" style="min-width: 4em; width: 100%"
        selectall @change=${event => t.track.name (event.detail.value.trim())}
        >${t.wtrack_.name}</b-editable>
    </span>
    <div class="-lvm-main">
      <div class="-lvm-levelbg" ${ref (h => t.levelbg_ = h)}></div>
      <div class="-lvm-covermid0" ${ref (h => t.covermid0_ = h)}></div>
      <div class="-lvm-covertip0" ${ref (h => t.covertip0_ = h)}></div>
      <div class="-lvm-coverspace" ></div>
      <div class="-lvm-covermid1" ${ref (h => t.covermid1_ = h)}></div>
      <div class="-lvm-covertip1" ${ref (h => t.covertip1_ = h)}></div>
    </div>
  </div>

  <b-contextmenu ${ref (h => t.trackviewcmenu_ = h)} id="g-trackviewcmenu" @activate=${t.menu_click} .isactive=${t.menu_check} >
    <b-menutitle> Track </b-menutitle>
    <b-menuitem ic="fa-plus-circle"        uri="add-track" >      Add Track             </b-menuitem>
    <b-menuitem ic="fa-i-cursor"           uri="rename-track" >   Rename Track          </b-menuitem>
    <b-menuitem ic="fa-toggle-down"        uri="bounce-track" >   Bounce Track          </b-menuitem>
    <b-menuitem ic="mi-visibility_off"     uri="track-details" >  Show / Hide Track Details </b-menuitem>
    <b-menuseparator style="margin: 7px" ></b-menuseparator>
    <b-menurow>		<!-- ic="fa-clone" uri="clone-track" >    Dupl.                 -->
      <b-menuitem ic="fa-times-circle"     uri="delete-track" >   Delete                </b-menuitem>
      <b-menuitem ic="fa-scissors"         uri="cut-track" >      Cut                   </b-menuitem>
      <b-menuitem ic="fa-files-o"          uri="copy-track" >     Copy                  </b-menuitem>
      <b-menuitem ic="fa-clipboard"        uri="paste-track" >    Paste                 </b-menuitem>
    </b-menurow>
    <b-menuseparator style="margin: 7px" ></b-menuseparator>
    <b-menutitle> Playback </b-menutitle>
    <b-menuitem ic="uc-Ｍ"                 uri="mute-track" >     Mute Track            </b-menuitem>
    <b-menuitem ic="uc-Ｓ"                 uri="solo-track" >     Solo Track            </b-menuitem>
    <b-menuseparator style="margin: 7px" ></b-menuseparator>
    <b-menutitle> MIDI Channel </b-menutitle>
    <b-menuitem                  uri="mc-0"  ic=${t.mcc (0)}  > Internal Channel </b-menuitem>
    <b-menurow noturn>
      <b-menuitem class="-nokbd" uri="mc-1"  ic=${t.mcc (1)}  >  1 </b-menuitem>
      <b-menuitem class="-nokbd" uri="mc-2"  ic=${t.mcc (2)}  >  2 </b-menuitem>
      <b-menuitem class="-nokbd" uri="mc-3"  ic=${t.mcc (3)}  >  3 </b-menuitem>
      <b-menuitem class="-nokbd" uri="mc-4"  ic=${t.mcc (4)}  >  4 </b-menuitem>
    </b-menurow> <b-menurow noturn>
      <b-menuitem class="-nokbd" uri="mc-5"  ic=${t.mcc (5)}  >  5 </b-menuitem>
      <b-menuitem class="-nokbd" uri="mc-6"  ic=${t.mcc (6)}  >  6 </b-menuitem>
      <b-menuitem class="-nokbd" uri="mc-7"  ic=${t.mcc (7)}  >  7 </b-menuitem>
      <b-menuitem class="-nokbd" uri="mc-8"  ic=${t.mcc (8)}  >  8 </b-menuitem>
    </b-menurow> <b-menurow noturn>
      <b-menuitem class="-nokbd" uri="mc-9"  ic=${t.mcc (9)}  >  9 </b-menuitem>
      <b-menuitem class="-nokbd" uri="mc-10" ic=${t.mcc (10)} > 10 </b-menuitem>
      <b-menuitem class="-nokbd" uri="mc-11" ic=${t.mcc (11)} > 11 </b-menuitem>
      <b-menuitem class="-nokbd" uri="mc-12" ic=${t.mcc (12)} > 12 </b-menuitem>
    </b-menurow> <b-menurow noturn>
      <b-menuitem class="-nokbd" uri="mc-13" ic=${t.mcc (13)} > 13 </b-menuitem>
      <b-menuitem class="-nokbd" uri="mc-14" ic=${t.mcc (14)} > 14 </b-menuitem>
      <b-menuitem class="-nokbd" uri="mc-15" ic=${t.mcc (15)} > 15 </b-menuitem>
      <b-menuitem class="-nokbd" uri="mc-16" ic=${t.mcc (16)} > 16 </b-menuitem>
    </b-menurow>
  </b-contextmenu>
`;

// == SCRIPT ==
import * as Ase from '../aseapi.js';
import * as Util from '../util.js';
import { clamp } from '../util.js';

const MINDB = -72.0; // -96.0;
const MAXDB =  +6.0; // +12.0;
const DBOFFSET = Math.abs (MINDB) * 1.5;
const DIV_DBRANGE = 1.0 / (MAXDB - MINDB);
const NUMBER_ATTRIBUTE = { type: Number, reflect: true }; // sync attribute with property
const OBJECT_PROPERTY = { attribute: false };

class BTrackView extends LitElement {
  static styles = [ STYLE ];
  static properties = {
    track: OBJECT_PROPERTY,
    trackindex: NUMBER_ATTRIBUTE,
  };
  render()
  {
    this.notify_current_track();
    return HTML (this);
  }
  constructor()
  {
    super();
    this.track = null;
    this.trackindex = -1;
    this.wtrack_ = { name: '   ' };
    this.dbtip0_ = MINDB;
    this.dbtip1_ = MINDB;
    this.teleobj = null;
    this.telemetry = null;
  }
  disconnectedCallback() {
    super.disconnectedCallback();
    this.telemetry = null;
    Util.telemetry_unsubscribe (this.teleobj);
    this.teleobj = null;
  }
  notify_current_track() // see app.js
  {
    if (this.track === App.current_track)
      this.setAttribute ('current-track', '');
    else
      this.removeAttribute ('current-track');
  }
  updated (changed_props)
  {
    if (changed_props.has ('track'))
      {
	const weakthis = new WeakRef (this); // avoid strong wtrack_->this refs for automatic cleanup
	this.wtrack_ = Util.wrap_ase_object (this.track, { name: '???', midi_channel: -1 }, () => weakthis.deref()?.requestUpdate());
	Util.telemetry_unsubscribe (this.teleobj);
	this.teleobj = null;
	// subscribe telemetry
	const async_updates = async () => {
	  this.telemetry = await Object.freeze (this.track.telemetry());
	  if (!this.teleobj && this.telemetry)
	    this.teleobj = Util.telemetry_subscribe (this.recv_telemetry.bind (this), this.telemetry);
	};
	async_updates();
      }
    // setup level gradient based on MINDB..MAXDB
    this.levelbg_.style.setProperty ('--db-zpc', -MINDB * 100.0 / (MAXDB - MINDB) + '%');
    // cache level width in pixels to avoid expensive recalculations in fps handler
    this.level_width_ = this.levelbg_.getBoundingClientRect().width;
  }
  mcc (n) // midi_channel character
  {
    if (n == this.wtrack_.midi_channel)
      return '√';
    else
      return ' ';
  }
  track_click0 (event)
  {
    event.stopPropagation();
    if (event.button == 0 && this.track)
      App.current_track = this.track;
  }
  menu_open (event)
  {
    App.current_track = this.track;
    // force popup at mouse coords
    this.trackviewcmenu_.popup (event, { origin: 'none' });
    event.stopPropagation();
  }
  async menu_check (uri)
  {
    switch (uri)
    {
      case 'add-track':    return true;
      case 'delete-track': return App.current_track && !await App.current_track.is_master();
      case 'rename-track': return true;
    }
    if (uri.startsWith ('mc-'))
      return true;
    return false;
  }
  async menu_click (event)
  {
    const uri = event.detail.uri;
    // close popup to remove focus guards
    this.trackviewcmenu_.close();
    if (uri == 'add-track')
      {
	const track = await Data.project.create_track ('Track');
	if (track)
	  App.current_track = track;
      }
    if (uri == 'delete-track')
      {
	const del_track = this.track;
	let tracks = App.project.all_tracks();
	Data.project.remove_track (del_track);
	tracks = await tracks;
	const index = Util.array_index_equals (tracks, del_track);
	tracks.splice (index, 1);
	if (index < tracks.length) // false if deleting Master
	  App.current_track = tracks[index];
      }
    if (uri == 'rename-track')
      this.trackname_.activate();
    if (uri.startsWith ('mc-'))
      {
	const ch = parseInt (uri.substr (3));
	this.track.midi_channel (ch);
      }
  }
  recv_telemetry (teleobj, arrays)
  {
    let dbspl0 = arrays[teleobj.dbspl0.type][teleobj.dbspl0.index];
    let dbspl1 = arrays[teleobj.dbspl1.type][teleobj.dbspl1.index];
    dbspl0 = clamp (dbspl0, MINDB, MAXDB);
    dbspl1 = clamp (dbspl1, MINDB, MAXDB);
    this.dbtip0_ = Math.max ((DBOFFSET + this.dbtip0_) * 0.99, DBOFFSET + dbspl0) - DBOFFSET;
    this.dbtip1_ = Math.max ((DBOFFSET + this.dbtip1_) * 0.99, DBOFFSET + dbspl1) - DBOFFSET;
    update_levels.call (this, dbspl0, this.dbtip0_, dbspl1, this.dbtip1_);
  }
}

customElements.define ('b-trackview', BTrackView);

function update_levels (dbspl0, dbtip0, dbspl1, dbtip1) {
  /* Paint model:
   * |                                           ######| covertipN_, dark tip cover layer
   * |             #############################       | covermidN_, dark middle cover
   * |-36dB+++++++++++++++++++++++++++++++0++++++++12dB| levelbg_, dB gradient
   *  ^^^^^^^^^^^^^ visible level (-24dB)       ^ visible tip (+6dB)
   */
  const covertip0 = this.covertip0_, covermid0 = this.covermid0_;
  const covertip1 = this.covertip1_, covermid1 = this.covermid1_;
  const level_width = this.level_width_, pxrs = 1.0 / level_width; // pixel width fraction between 0..1
  if (dbspl0 === undefined) {
    covertip0.style.setProperty ('transform', 'scaleX(1)');
    covertip1.style.setProperty ('transform', 'scaleX(1)');
    covermid0.style.setProperty ('transform', 'scaleX(0)');
    covermid1.style.setProperty ('transform', 'scaleX(0)');
    return;
  }
  const tw = 2; // tip thickness in pixels
  const pxrs_round = (fraction) => Math.round (fraction / pxrs) * pxrs; // scale up, round to pixel, scale down
  // handle multiple channels
  const per_channel = (dbspl, dbtip, covertip, covermid) => {
    // map dB SPL to a 0..1 paint range
    const tip = (dbtip - MINDB) * DIV_DBRANGE;
    const lev = (dbspl - MINDB) * DIV_DBRANGE;
    // scale covertip from 100% down to just the amount above the tip
    let transform = 'scaleX(' + pxrs_round (1 - tip) + ')';
    if (transform !== covertip.style.getPropertyValue ('transform'))    // reduce style recalculations
      covertip.style.setProperty ('transform', transform);
    // scale and translate middle cover
    if (lev + pxrs + tw * pxrs <= tip) {
      const width = (tip - lev) - tw * pxrs;
      const trnlx = level_width - level_width * tip + tw; // translate left in pixels
      transform = 'translateX(-' + Math.round (trnlx) + 'px) scaleX(' + pxrs_round (width) + ')';
    } else {
      // hide covermid if level and tip are aligned
      transform = 'scaleX(0)';
    }
    if (transform != covermid.style.getPropertyValue ('transform'))     // reduce style recalculations
      covermid.style.setProperty ('transform', transform);
  };
  per_channel (dbspl0, dbtip0, covertip0, covermid0);
  per_channel (dbspl1, dbtip1, covertip1, covermid1);
}
