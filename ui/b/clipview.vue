<!-- This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0 -->

<docs>
  # B-CLIPVIEW
  A Vue template to display a small view of Ase.Clip.
  ## Props:
  *clip*
  : The Ase.Clip to display.
  *tick*
  : The Ase.Track tick position.
  *tickscale* (read-only)
  : The pixel to PPQN ratio.
</docs>

<style lang="scss">
@import 'mixins.scss';
$b-clipview-font: normal 9px $b-theme-font-family !default;    // the '9px' part is dynamically resized
$b-clipview-font-color: rgba(255, 255, 255, 0.7) !default;
$b-clipview-note-color: rgba(255, 255, 255, 0.7) !default;
$b-clipview-color-hues: 75, 177, 320, 225, 45, 111, 5, 259, 165, 290;
$b-clipview-color-zmod: Jz=55, Cz=30;

.b-clipview {
  display: flex; position: relative;
  flex-shrink: 0;
  border: 0;
  .-canvas {
    display: inline; position: absolute; top: 0px; bottom: 0px; left: 0px; right: 0px;
    height: $b-trackrow-height;
    border-radius: calc($b-theme-border-radius * 0.66);
    --clipview-font-color: #{$b-clipview-font-color}; --clipview-font: #{$b-clipview-font};
    --clipview-note-color: #{$b-clipview-note-color};
    --clipview-color-hues: $b-clipview-color-hues;
    --clipview-color-zmod: $b-clipview-color-zmod;
    background-color: #222;
    box-shadow: inset 0px 0 1px #fff9, inset -1px 0 1px #000;
  }
  .-play {
    display: inline-block;
    position: absolute; top: 0px; left: 0px;
    padding: 3px;
    position: absolute;
    padding: 3px;
    top: 50%;
    left: 50%;
    transform: translate(-50%, -50%);
    color: $b-clip-play-fg;
    background: $b-clip-play-bg;
    border-radius: 3px;
  }
}
</style>

<template>
  <h-flex class="b-clipview"
	  @click.stop="App.open_piano_roll (clip)">
    <canvas class="-canvas" ref="canvas"
	    :x-style="{ left: pxoffset + 'px', width: canvas_width + 'px', }" />
    <span class="-play" @click.stop="click_play" >▶</span>
  </h-flex>
</template>

<script>
import * as Util from '../util.js';
import * as Ase from '../aseapi.js';

const tick_quant = Util.PPQN;

function observable_clip_data () {
  const data = {
    clipname: { getter: c => this.clip.name(),          notify: n => this.clip.on ("notify:name", n), },
    endtick:  { getter: c => this.clip.end_tick(),	notify: n => this.clip.on ("notify:end_tick", n), },
    allnotes: { default: [],                            notify: n => this.clip.on ("notify:notes", n),
		getter: async c => Object.freeze (await this.clip.list_all_notes()), },
  };
  return this.observable_from_getters (data, () => this.clip);
}

export default {
  sfc_template,
  props: {
    clip: { type: Ase.Clip, },
    tick: { type: Number, },
    index: { type: Number, },
    trackindex: { type: Number, },
  },
  data() { return observable_clip_data.call (this); },
  computed: {
    tickscale:    function() { return 10.0 / Util.PPQN; },
    pxoffset:     function() { return this.tick * this.tickscale; }, // TODO: adjust for tick_quant?
    canvas_width: function() {
      return this.tickscale * Math.floor ((this.endtick + tick_quant - 1) / tick_quant) * tick_quant;
    },
  },
  methods: {
    click_play() {
      debug ("PLAY: clip:", this.clip.$id);
    },
    dom_update() {
      render_canvas.call (this);
    },
  },
};

import * as C from '../colors.js';

function render_canvas () {
  // canvas setup
  const canvas = this.$refs.canvas;
  const pixelratio = Util.resize_canvas (canvas, canvas.clientWidth, canvas.clientHeight, true);
  const ctx = canvas.getContext ('2d'), cstyle = getComputedStyle (canvas), csp = cstyle.getPropertyValue.bind (cstyle);
  const width = canvas.width, height = canvas.height;
  const tickscale = this.tickscale * pixelratio;
  //const width = canvas.clientWidth, height = canvas.clientHeight;
  //canvas.width = width; canvas.height = height;
  ctx.clearRect (0, 0, width, height);
  // color setup
  let cindex;
  // cindex = Util.fnv1a_hash (this.clipname);	// - color from clip name
  cindex = this.index;				// - color per clip
  cindex = this.trackindex;			// - color per track
  const hues = csp ('--clipview-color-hues').split (',');
  const zmods = csp ('--clipview-color-zmod').split (',');
  const hz = hues[cindex % hues.length];
  const bgcol = C.zmod ('hz=' + hz, ...zmods);
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
  ctx.fillText (this.clipname, 1.5, .5);
  // paint notes
  ctx.fillStyle = csp ('--clipview-note-color');
  const pnotes = this.allnotes; // await clip.list_notes_crossing (0, MAXINT);
  const noteoffset = 12;
  const notescale = height / (123.0 - 2 * noteoffset); // MAX_NOTE
  for (const note of pnotes) {
    ctx.fillRect (note.tick * tickscale, height - (note.key - noteoffset) * notescale, note.duration * tickscale, 1 * pixelratio);
  }
}

</script>
