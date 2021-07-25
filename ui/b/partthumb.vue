<!-- This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0 -->

<docs>
  # B-PARTTHUMB
  A Vue template to display a thumbnail for a Ase.Part.
  ## Props:
  *part*
  : The Ase.Part to display.
  *tick*
  : The Ase.Track tick position.
</docs>

<style lang="scss">
  @import 'mixins.scss';
  .b-partthumb {
    display: inline-block; position: absolute; top: 0px; bottom: 0px;
    height: $b-trackrow-height;
    border-radius: $b-theme-border-radius * 0.66;
    --partthumb-font-color: #{$b-partthumb-font-color}; --partthumb-font: #{$b-partthumb-font};
    --partthumb-note-color: #{$b-partthumb-note-color}; --partthumb-colors: #{$b-partthumb-colors};
    background-color: #222;
    box-shadow: inset 0px 0 1px #fff9, inset -1px 0 1px #000;
  }
</style>

<template>
  <canvas ref="canvas" class="b-partthumb" @click="App.open_piano_roll (part)"
	  :style="{ left: pxoffset + 'px', width: canvas_width + 'px', }" ></canvas>
</template>

<script>
import * as Util from '../util.js';
import * as Ase from '../aseapi.js';

const tick_quant = Util.PPQN;

function observable_part_data () {
  const data = {
    partname: { getter: c => this.part.get_name(),      notify: n => this.part.on ("notify:name", n), },
    lasttick: { getter: c => this.part.get_last_tick(), notify: n => this.part.on ("notify:last_tick", n), },
    allnotes: { default: [],                            notify: n => this.part.on ("noteschanged", n),
		getter: async c => Object.freeze (await this.part.list_notes_crossing (0, CONFIG.MAXINT)), },
  };
  return this.observable_from_getters (data, () => this.part);
}

export default {
  sfc_template,
  props: {
    part: { type: Ase.Part, },
    tick: { type: Number, },
    index: { type: Number, },
    trackindex: { type: Number, },
  },
  data() { return observable_part_data.call (this); },
  computed: {
    tickscale:    function() { return 10.0 / Util.PPQN; },
    pxoffset:     function() { return this.tick * this.tickscale; }, // TODO: adjust for tick_quant?
    canvas_width: function() {
      return this.tickscale * Math.floor ((this.lasttick + tick_quant - 1) / tick_quant) * tick_quant;
    },
  },
  methods: {
    dom_update() {
      if (this.lasttick)
	render_canvas.call (this);
    },
  },
};

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
  const colors = Util.split_comma (csp ('--partthumb-colors'));
  let cindex;
  cindex = this.trackindex;			// - color per track
  cindex = (cindex + 1013904223) * 1664557;	//   LCG randomization step
  cindex = this.index;				// - color per part
  cindex = Util.fnv1a_hash (this.partname);	// - color from part name
  const bgcol = colors[(cindex >>> 0) % colors.length];
  // paint part background
  ctx.fillStyle = bgcol;
  ctx.fillRect (0, 0, width, height);
  // draw name
  const fpx = height / 3;
  const note_font = csp ('--partthumb-font');
  const fpx_parts = note_font.split (/\s*\d+px\s*/i); // 'bold 10px sans' -> [ ['bold', 'sans']
  ctx.font = fpx_parts[0] + ' ' + fpx + 'px ' + (fpx_parts[1] || '');
  ctx.fillStyle = csp ('--partthumb-font-color');
  ctx.textAlign = 'left';
  ctx.textBaseline = 'top';
  ctx.fillText (this.partname, 1.5, .5);
  // paint notes
  ctx.fillStyle = csp ('--partthumb-note-color');
  const pnotes = this.allnotes; // await part.list_notes_crossing (0, MAXINT);
  const noteoffset = 12;
  const notescale = height / (123.0 - 2 * noteoffset); // MAX_NOTE
  for (const note of pnotes) {
    ctx.fillRect (note.tick * tickscale, height - (note.key - noteoffset) * notescale, note.duration * tickscale, 1 * pixelratio);
  }
}

</script>
