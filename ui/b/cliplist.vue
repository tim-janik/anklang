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
  }
  .b-cliplist-clip {
    display: flex;
    flex-shrink: 0;
    width: $b-clipthumb-width;
    border: 1px solid black;
  }
</style>

<template>

  <div class="b-cliplist" >
    <span class="b-cliplist-clip" v-for="(clip, index) in clips" :key="clip.$id"
	  :clip="clip" :index="index" :track="track" @click.stop="click" >
      {{ index + ":" + clip.$id }}</span>
  </div>

</template>

<script>
import * as Ase from '../aseapi.js';

function cliplist_data () {
  const data = {
    clips:	{ default: [],		notify: n => this.track.on ("notify:clips", n),
		  getter: async c => Object.freeze (await this.track.list_clips()), },
  };
  return this.observable_from_getters (data, () => this.track);
}

export default {
  sfc_template,
  props: {
    'track': { type: Ase.Track, },
  },
  data() { return cliplist_data.call (this); },
  methods: {
    click (ev) {
      const indexattr = ev.target.getAttribute ('index');
      if (indexattr !== undefined)
	{
	  const index = indexattr >>> 0;
	  const clip = this.clips[index];
	  if (clip)
	    App.open_piano_roll (clip);
	}
    },
  },
};

</script>
