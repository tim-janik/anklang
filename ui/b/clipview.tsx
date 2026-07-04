// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

/** @class BClipView
 * @description
 * Displays a small thumbnail view of a MIDI clip with a canvas showing notes.
 */

import { createEffect, onCleanup } from 'solid-js';
import * as Util from '../util.js';

// == STYLE ==
Extra_css`
:root {
  --b-clipview-font-color: rgba(255 255 255 / 0.7);
  --b-clipview-note-color: rgba(255 255 255 / 0.7);
  --b-clipview-color-hues: 75, 177, 320, 225, 45, 111, 5, 259, 165, 290;
}
.b-clipview {
  display: flex; position: relative;
  flex-shrink: 0;
  border: 0;
  margin: 0 0 0 2px;
  .-canvas {
    display: inline; position: absolute; inset: 0;
    --clipview-font-color: var(--b-clipview-font-color); --clipview-font: var(--b-canvas-font);
    --clipview-note-color: var(--b-clipview-note-color);
    --clipview-color-hues: var(--b-clipview-color-hues);
    box-shadow: inset 0px 0 1px #fff9, inset -1px 0 1px #000;
    border-radius: var(--b-button-radius);
  }
  .-play {
    display: inline;
    position: absolute;
    padding: 3px;
    top: 50%;
    left: 50%;
    transform: translate(-50%, -50%);
    color: var(--b-clip-play-fg);
    background: var(--b-clip-play-bg);
    border-radius: calc(var(--b-button-radius) * 0.66);
  }
}`;

import * as Z from 'zcam-js';

const sRGB_viewing_conditions = {
  Fs: Z.ZCAM_DIM,       // DIM comes closest to CIELAB L* in ZCAM and CIECAM97
  Yb: 9.1,              // Background luminance factor so Jz=50 yields #777777
  La: 80,               // La = Lw * Yb / 100; Safdar21, ZCAM, a colour appearance model
  Xw: Z.ZCAM_D65.x, Yw: Z.ZCAM_D65.y, Zw: Z.ZCAM_D65.z,
};
const default_gamut = new Z.Gamut (sRGB_viewing_conditions);

function render_canvas (canvas, clip, tickscale) {
  const pixelratio = Util.resize_canvas (canvas, canvas.parentElement.clientWidth, canvas.parentElement.clientHeight, true);
  const ctx = canvas.getContext ('2d'), cstyle = getComputedStyle (canvas), csp = cstyle.getPropertyValue.bind (cstyle);
  const width = canvas.width, height = canvas.height;
  //const width = canvas.clientWidth, height = canvas.clientHeight;
  //canvas.width = width; canvas.height = height;
  const ts = tickscale * pixelratio;
  ctx.clearRect (0, 0, width, height);
  // color setup
  let cindex;
  // cindex = Util.hash53 (this.clip.name);	// - color from clip name
  // cindex = this.index;			// - color per clip
  // cindex = this.trackindex;			// - color per track
  cindex = 0;
  const hues = csp ('--clipview-color-hues').split (',');
  const hz = hues[cindex % hues.length];
  const gamut = default_gamut, viewing = gamut.viewing;
  let zcam = { Jz: 55, Cz: 30, viewing, hz: parseFloat (hz), };
  const max_Cz = gamut.maximize_Cz (zcam);
  zcam.Cz = max_Cz * 0.7;
  let rgb = gamut.contains (zcam);
  if (!rgb.inside)
    rgb = gamut.contains (gamut.clamp_chroma (zcam));
  const bgcol = Z.srgb_hex (rgb);
  // paint clip background
  ctx.fillStyle = bgcol;
  ctx.fillRect (0, 0, width, height);
  // draw name
  const fpx = height / 3;
  const note_font = csp ('--clipview-font');
  const fpx_clips = note_font.split (/\s*\d+px\s*/i); // 'bold 10px sans' -> [ ['bold', 'sans']
  ctx.font = fpx_clips[0] + ' ' + fpx + 'px ' + (fpx_clips[1] || '');
  ctx.fillStyle = csp ('--clipview-font-color');
  ctx.textAlign = 'left';
  ctx.textBaseline = 'top';
  // paint notes
  ctx.fillText (clip.name, 1.5, .5);
  ctx.fillStyle = csp ('--clipview-note-color');
  const noteoffset = 12;
  const notescale = height / (123.0 - 2 * noteoffset);
  for (const note of clip.all_notes)
    ctx.fillRect (note.tick * ts, height - (note.key - noteoffset) * notescale, note.duration * ts, 1 * pixelratio);
}

// == COMPONENT ==
export function ClipView (props) {
  let canvas_ref;
  let root_ref;
  let pending_raf = 0;

  const tickscale = 10.0 / Util.PPQN;

  // Expose tickscale on DOM element for parent measurement
  createEffect (() => {
    if (root_ref)
      root_ref.tickscale = tickscale;
  });

  // Re-render canvas when clip properties change
  onCleanup (() => cancelAnimationFrame (pending_raf));
  createEffect (() => {
    // Access clip properties to track reactivity
    props.clip?.name;
    props.clip?.end_tick;
    props.clip?.all_notes;

    if (canvas_ref && props.clip) {
      cancelAnimationFrame (pending_raf);
      pending_raf = requestAnimationFrame (() => {
        render_canvas (canvas_ref, props.clip, tickscale);
      });
    }
  });

  function click (event) {
    App.open_piano_roll (props.clip);
    Util.prevent_event (event);
  }

  function click_play (event) {
    Util.prevent_event (event);
    debug ("PLAY: clip:", props.clip?.$id);
  }

  return (
    <div class="b-clipview hflex" ref={root_ref} onClick={click}>
      <canvas class="-canvas" ref={canvas_ref}></canvas>
      <span class="-play" onClick={click_play}>▶</span>
    </div>
  );
}
