// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** == B-TRACKVIEW ==
 * A SolidJS component to display a project's Ase.Track.
 * ### Props:
 * *track*
 * : The *Ase.Track* to display.
 * *trackindex*
 * : The index of this track in the track list.
 */

import { createEffect, onMount, onCleanup } from 'solid-js';
import { html } from '../little.js';
import * as Util from '../util.js';
import { clamp } from '../util.js';
import { Editable } from './editable';
import { render_contextmenu } from './contextmenu.js';
import { get_uri } from '../dom.js';

// == STYLE ==
Extra_css`
b-trackview, .b-trackview {
  display: flex;
  align-items: stretch;
  background-color: var(--b-button-border);
  border: 1px solid var(--b-button-border);
  border-top-left-radius: var(--b-button-radius);
  border-bottom-left-radius: var(--b-button-radius);
  .-lvm-main { /* level meter */
    height: calc(var(--b-track-meter-thickness) + var(--b-track-meter-gap) + var(--b-track-meter-thickness));
    position: relative;
    /* push element onto own compositing layer to reduce rendering overhead */
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
  .-lvm-coverspace                     { top: calc(var(--b-track-meter-thickness) - 0.25px); height: calc(var(--b-track-meter-gap) + 0.5px); }
  .-lvm-covertip1, .-lvm-covermid1     { top: calc(var(--b-track-meter-thickness) + var(--b-track-meter-gap)); }
  .-lvm-coverspace {
    background-color: rgba( 0 0 0 / .80);
  }
  .-lvm-covertip0, .-lvm-covermid0, .-lvm-covertip1, .-lvm-covermid1 {
    height: var(--b-track-meter-thickness);
    background-color: rgba( 0 0 0 / .75);
    transform-origin: center right;
    will-change: transform;
    transform: scaleX(1);
  }
  .-lvm-covertip1, .-lvm-covermid1 {
    height: calc(var(--b-track-meter-thickness) + 1px);
    /* add 1px to cover for rounded coords */
  }
  .b-trackview-control {
    display: flex;
    flex-direction: column;
    justify-content: space-between;
    margin-right: 5px;
    overflow: hidden;
  }
}
b-trackview[data-current-track] .b-trackview-control,
.b-trackview[data-current-track] .b-trackview-control {
  background-color: oklch(from var(--b-button-border) calc(l * 1.25) c h);
}`;

// == Constants ==
const MINDB = -72.0; // -96.0;
const MAXDB =  +6.0; // +12.0;
const DBOFFSET = Math.abs (MINDB) * 1.5;
const DIV_DBRANGE = 1.0 / (MAXDB - MINDB);

// == Contextmenu template ==
const HTML_CONTEXTMENU = (t, d) => html`
  <b-contextmenu @activate=${t.menu_click} .isactive=${t.menu_check} @close=${t.menu_close} @cancel=${t.menu_close} >
    <b-menutitle>                                         Track             </b-menutitle>
    <button ic="fa-plus_circle"    uri="add-track" >      Add Track             </button>
    <button ic="fa-music"          uri="add-midi-clip">   Add MIDI Clip         </button>
    <button ic="fa-i_cursor"       uri="rename-track" >   Rename Track          </button>
    <button ic="fa-toggle_down"    uri="bounce-track" >   Bounce Track          </button>
    <button ic="md-eye_off" uri="track-details" >  Show / Hide Track Details </button>
    <b-menuseparator></b-menuseparator>
    <b-menurow> <!-- ic="fa-clone" uri="clone-track" >    Dupl.                 -->
      <button ic="fa-times_circle" uri="delete-track" >   Delete                </button>
      <button ic="fa-scissors"     uri="cut-track" >      Cut                   </button>
      <button ic="fa-files_o"      uri="copy-track" >     Copy                  </button>
      <button ic="fa-clipboard"    uri="paste-track" >    Paste                 </button>
    </b-menurow>
    <b-menuseparator></b-menuseparator>
    <b-menutitle> Playback </b-menutitle>
    <button ic="uc-Ｍ"             uri="mute-track" >     Mute Track            </button>
    <button ic="uc-Ｓ"             uri="solo-track" >     Solo Track            </button>
    <b-menuseparator></b-menuseparator>
    <b-menutitle> MIDI Channel </b-menutitle>
    <button   uri="mc-0"  ic=${t.mcc (0)}  > Internal Channel </button>
    <b-menurow noturn>
      <button uri="mc-1"  ic=${t.mcc (1)}  >  1 </button>
      <button uri="mc-2"  ic=${t.mcc (2)}  >  2 </button>
      <button uri="mc-3"  ic=${t.mcc (3)}  >  3 </button>
      <button uri="mc-4"  ic=${t.mcc (4)}  >  4 </button>
    </b-menurow> <b-menurow noturn>
      <button uri="mc-5"  ic=${t.mcc (5)}  >  5 </button>
      <button uri="mc-6"  ic=${t.mcc (6)}  >  6 </button>
      <button uri="mc-7"  ic=${t.mcc (7)}  >  7 </button>
      <button uri="mc-8"  ic=${t.mcc (8)}  >  8 </button>
    </b-menurow> <b-menurow noturn>
      <button uri="mc-9"  ic=${t.mcc (9)}  >  9 </button>
      <button uri="mc-10" ic=${t.mcc (10)} > 10 </button>
      <button uri="mc-11" ic=${t.mcc (11)} > 11 </button>
      <button uri="mc-12" ic=${t.mcc (12)} > 12 </button>
    </b-menurow> <b-menurow noturn>
      <button uri="mc-13" ic=${t.mcc (13)} > 13 </button>
      <button uri="mc-14" ic=${t.mcc (14)} > 14 </button>
      <button uri="mc-15" ic=${t.mcc (15)} > 15 </button>
      <button uri="mc-16" ic=${t.mcc (16)} > 16 </button>
    </b-menurow>
  </b-contextmenu>
`;

// == Component ==
export function TrackView (props)
{
  let root_ref = null;
  let trackviewcontrol_ref = null;
  let trackname_ref = null;
  let levelbg_ref = null;
  let covermid0_ref = null;
  let covertip0_ref = null;
  let covermid1_ref = null;
  let covertip1_ref = null;

  let dbtip0_ = MINDB;
  let dbtip1_ = MINDB;
  let teleobj = null;
  let telemetry = null;
  let telemetry_gen = 0;
  let level_width_ = 0;
  let trackview_contextmenu = null;

  function mcc (n) // midi_channel character
  {
    return n == props.track.midi_channel ? '\u221a' : ' ';
  }

  function track_click0 (event)
  {
    event.stopPropagation();
    if (event.button == 0 && props.track)
      Shell.current_track = props.track;
  }

  function menu_close ()
  {
    trackview_contextmenu?.close();
  }

  function menu_open (event)
  {
    Shell.current_track = props.track;
    // update trackview menu for popup
    trackview_contextmenu = render_contextmenu (trackview_contextmenu, HTML_CONTEXTMENU, {
      mcc,
      menu_click,
      menu_check,
      menu_close,
    });
    // popup menu at mouse coords
    trackview_contextmenu.popup (event, { origin: 'none' });
    return Util.prevent_event (event);
  }

  async function menu_check (uri)
  {
    switch (uri)
    {
      case 'add-track':
      case 'add-midi-clip': return true;
      case 'delete-track': return Shell.current_track && !await Shell.current_track.is_master();
      case 'rename-track': return true;
    }
    if (uri.startsWith ('mc-'))
      return true;
    return false;
  }

  async function menu_click (event)
  {
    const uri = get_uri (event.detail);
    // close popup to remove focus guards
    menu_close();
    if (uri == 'add-track')
      {
	const track = await Shell.project.create_track ('Track');
	if (track)
	  Shell.current_track = track;
      }
    if (uri == 'add-midi-clip')
      {
	const track = Shell.current_track;
	if (track && !await track.is_master())
	  {
	    const clip = await track.create_midi_clip ('MIDI Clip', 0.0, 4.0);
	    if (clip)
	      App.open_piano_roll (clip);
	  }
      }
    if (uri == 'delete-track')
      {
	const del_track = props.track;
	let tracks = Shell.project.all_tracks();
	del_track.remove_self ();
	tracks = await tracks;
	const index = Util.array_index_equals (tracks, del_track);
	tracks.splice (index, 1);
	if (index < tracks.length) // false if deleting Master
	  Shell.current_track = tracks[index];
      }
    if (uri == 'rename-track')
      trackname_ref?.activate();
    if (uri.startsWith ('mc-'))
      {
	const ch = parseInt (uri.substr (3));
	props.track.midi_channel = ch;
      }
  }

  function recv_telemetry (teleobj, arrays)
  {
    let dbspl0 = arrays[teleobj.dbspl0.type][teleobj.dbspl0.index];
    let dbspl1 = arrays[teleobj.dbspl1.type][teleobj.dbspl1.index];
    dbspl0 = clamp (dbspl0, MINDB, MAXDB);
    dbspl1 = clamp (dbspl1, MINDB, MAXDB);
    dbtip0_ = Math.max ((DBOFFSET + dbtip0_) * 0.99, DBOFFSET + dbspl0) - DBOFFSET;
    dbtip1_ = Math.max ((DBOFFSET + dbtip1_) * 0.99, DBOFFSET + dbspl1) - DBOFFSET;
    update_levels (dbspl0, dbtip0_, dbspl1, dbtip1_);
  }

  function update_levels (dbspl0, dbtip0, dbspl1, dbtip1)
  {
    /* Paint model:
     * |                                           ######| covertipN_, dark tip cover layer
     * |             #############################       | covermidN_, dark middle cover
     * |-36dB+++++++++++++++++++++++++++++++0++++++++12dB| levelbg_, dB gradient
     *  ^^^^^^^^^^^^^ visible level (-24dB)       ^ visible tip (+6dB)
     */
    const covertip0 = covertip0_ref, covermid0 = covermid0_ref;
    const covertip1 = covertip1_ref, covermid1 = covermid1_ref;
    const level_width = level_width_, pxrs = 1.0 / level_width; // pixel width fraction between 0..1
    if (dbspl0 === undefined) {
      covertip0?.style.setProperty ('transform', 'scaleX(1)');
      covertip1?.style.setProperty ('transform', 'scaleX(1)');
      covermid0?.style.setProperty ('transform', 'scaleX(0)');
      covermid1?.style.setProperty ('transform', 'scaleX(0)');
      return;
    }
    const tw = 2; // tip thickness in pixels
    // handle multiple channels
    const per_channel = (dbspl, dbtip, covertip, covermid) => {
      if (!covertip || !covermid) return;
      // map dB SPL to a 0..1 paint range
      const tip = (dbtip - MINDB) * DIV_DBRANGE;
      const lev = (dbspl - MINDB) * DIV_DBRANGE;
      // scale covertip from 100% down to just the amount above the tip
      let transform = 'scaleX(' + (1 - tip) + ')';
      if (transform !== covertip.style.getPropertyValue ('transform'))    // reduce style recalculations
	covertip.style.setProperty ('transform', transform);
      // scale and translate middle cover
      if (lev + pxrs + tw * pxrs <= tip) {
	const width = (tip - lev) - tw * pxrs;
	const trnlx = level_width - level_width * tip + tw; // translate left in pixels
	transform = 'translateX(-' + trnlx + 'px) scaleX(' + width + ')';
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

  // Track current-track attribute reactively
  createEffect (() => {
    if (!root_ref) return;
    if (Shell.r.current_track === props.track)
      root_ref.setAttribute ('data-current-track', '');
    else
      root_ref.removeAttribute ('data-current-track');
  });

  // Setup contextmenu on mount
  onMount (() => {
    if (!trackview_contextmenu)
      trackview_contextmenu = render_contextmenu (trackview_contextmenu, HTML_CONTEXTMENU, {
	mcc,
	menu_click,
	menu_check,
	menu_close,
      });
  });

  // Setup telemetry on track change
  createEffect (() => {
    const track = props.track;
    if (!track) return;
    track.midi_channel; // access field, we need it later on.
    Util.telemetry_unsubscribe (teleobj);
    teleobj = null;
    telemetry = null;
    const gen = ++telemetry_gen;
    (async () => {
      telemetry = await Object.freeze (track.telemetry());
      if (gen !== telemetry_gen) return; // stale async, track changed during await
      if (!teleobj && telemetry)
	teleobj = Util.telemetry_subscribe (recv_telemetry, telemetry);
    })();
  });

  onCleanup (() => {
    telemetry = null;
    Util.telemetry_unsubscribe (teleobj);
    teleobj = null;
  });

  // Setup level gradient based on MINDB..MAXDB
  createEffect (() => {
    if (levelbg_ref) {
      levelbg_ref.style.setProperty ('--db-zpc', -MINDB * 100.0 / (MAXDB - MINDB) + '%');
      // cache level width in pixels to avoid expensive recalculations in fps handler
      level_width_ = levelbg_ref.getBoundingClientRect().width;
    }
  });

  const on_editable_change = (event) => {
    props.track.name = event.detail.value.trim();
  };

  return (
    <div class="b-trackview" ref={root_ref}>
      <div class="b-trackview-control" data-tip="**CLICK** Select Track **RIGHTCLICK** Track Menu"
	onClick={track_click0}
	onContextMenu={menu_open}
	ref={e => trackviewcontrol_ref = e}>
	<Editable ref={e => trackname_ref = e} clicks={2} style="min-width: 4em; width: 7em"
	  selectall onChange={on_editable_change}
	  value={props.track.name} />
	<div class="-lvm-main">
	  <div class="-lvm-levelbg" ref={e => levelbg_ref = e}></div>
	  <div class="-lvm-covermid0" ref={e => covermid0_ref = e}></div>
	  <div class="-lvm-covertip0" ref={e => covertip0_ref = e}></div>
	  <div class="-lvm-coverspace" ></div>
	  <div class="-lvm-covermid1" ref={e => covermid1_ref = e}></div>
	  <div class="-lvm-covertip1" ref={e => covertip1_ref = e}></div>
	</div>
      </div>
    </div>
  );
}
