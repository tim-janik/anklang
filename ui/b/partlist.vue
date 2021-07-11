<!-- This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0 -->

<docs>
  # B-PARTLIST
  A Vue template to display a list of Ase.Part instances.
  ## Props:
  *project*
  : The project containing playback tracks.
  *track*
  : The Track containing the parts to display.
</docs>

<template>

  <div class="b-partlist" >
    <b-partthumb v-for="(cliptick, pindex) in parts" :key="cliptick.clip.$id + ' @' + cliptick.tick"
		 :part="cliptick.clip" :tick="cliptick.tick" :trackindex="trackindex" :index="pindex" ></b-partthumb>
  </div>

</template>

<style lang="scss">
  @import 'mixins.scss';
  .b-partlist {
    display: inline-block;
    position: relative;
    height: $b-trackrow-height;	//* fixed height is required to accurately calculate vertical scroll area */
  }
</style>

<script>
import * as Ase from '../aseapi.js';

export default {
  sfc_template,
  props: {
    'track': { type: Ase.Track, },
    'trackindex': { type: Number, },
  },
  watch: {
    track: { immediate: true, async handler (n, o) {
      let clips = await this.track.list_clips();
      clips = clips.map (async clip => ({ clip: clip, tick: 0 }) );
      clips = await Promise.all (clips);
      this.parts = []; // not yet implemented
    } },
  },
  data() { return {
    parts: [],  // [{tick,clip,duration,unique_id}, ...]
  }; },
};

</script>
