<!-- This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0 -->

<docs>
  # B-CLIPLIST
  A Vue template to display a list of Ase.Clip instances.
  ## Props:
  *project*
  : The project containing playback tracks.
  *track*
  : The Track containing the clips to display.
</docs>

<style lang="scss">
@import 'mixins.scss';
.b-cliplist {
  display: flex;
  position: relative;
  height: $b-trackrow-height;	//* fixed height is required to accurately calculate vertical scroll area */
  .-indicator {
    position: absolute; top: 0; bottom: 0; left: 0; width: 1px; height: 100%;
    background: #f0f0f0cc;
    z-index: 2;
    transform: translateX(-9999px);
  }
  .b-clipview {
    margin: 0 1px;
    width: $b-clipthumb-width;
  }
}
</style>

<template>

  <div class="b-cliplist" >
    <b-clipview v-for="(clip, index) in clips" :key="clip.$id"
		:clip="clip" :index="index" :track="track" :trackindex="trackindex" />
    <span class="-indicator" ref="indicator" />
  </div>

</template>

<script>
import * as Ase from '../aseapi.js';

function cliplist_data () {
  const data = {
    clips:	{ default: [],		notify: n => this.track.on ("notify:clips", n),
		  getter: async c => Object.freeze (await this.track.list_clips()), },
    telemetry:	{ getter: async c => Object.freeze (this.track.telemetry()), },
  };
  return this.observable_from_getters (data, () => this.track);
}

export default {
  sfc_template,
  props: {
    track:	{ type: Ase.Track, },
    trackindex:	{ type: Number, },
  },
  data() { return cliplist_data.call (this); },
  methods: {
    dom_create () {
      this.ratiomul = window.devicePixelRatio;
      this.ratiodiv = 1.0 / this.ratiomul;
    },
    dom_update () {
      this.clipviews = [];
      for (const clipview of this.$el.querySelectorAll (".b-cliplist > .b-clipview")) {
	const cv = Util.vue_component (clipview);
	this.clipviews.push ({ width: clipview.getBoundingClientRect().width,
			       tickscale: cv.tickscale,
			       x: clipview.offsetLeft, });
      }
      if (!this.teleobj && this.telemetry)
	this.teleobj = Util.telemetry_subscribe (this.recv_telemetry.bind (this), this.telemetry);
    },
    dom_destroy () {
      Util.telemetry_unsubscribe (this.teleobj);
      this.teleobj = null;
    },
    recv_telemetry (teleobj, arrays) {
      const current = arrays[teleobj.current_clip.type][teleobj.current_clip.index];
      const tick = arrays[teleobj.current_tick.type][teleobj.current_tick.index];
      // const next = arrays[teleobj.next.type][teleobj.next.index];
      let u;
      if (current >= 0 && current < this.clipviews.length && tick >= 0)
	{
	  const cv = this.clipviews[current];
	  const t = cv.x + tick * cv.tickscale;
	  u = Math.round (t * this.ratiomul) * this.ratiodiv; // screen pixel alignment
	}
      else
	u = -9999;
      if (u != this.last_pos)
	{
	  this.$refs.indicator.style = "transform: translateX(" + u + "px);";
	  this.last_pos = u;
	}
      if (Shell.piano_current_clip == this.clips[current])
	Shell.piano_current_tick (this.clips[current], tick);
    },
  },
};

</script>
