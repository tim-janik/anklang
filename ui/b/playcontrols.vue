<!-- This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0 -->

<docs>
  # B-PLAYCONTROLS
  A container holding the play and seek controls for a Ase.song.
  ## Props:
  *project*
  : Injected *Ase.Project*, using `b-shell.project`.
</docs>

<style lang="scss">
  @import 'mixins.scss';
  .b-playcontrols {
    button, push-button	{ padding: 5px; }
  }
</style>

<template>

  <b-button-bar
      class="b-playcontrols" >
    <push-button disabled @click="pcall ('...Last')"      ><b-icon fw lg fa="fast-backward" /></push-button>
    <push-button disabled @click="pcall ('...Backwards')" ><b-icon fw lg fa="backward"      /></push-button>
    <push-button @click="pcall ('stop_playback')" data-hotkey="S"
		 data-tip="**CLICK** Stop playback" >      <b-icon fw lg fa="stop"          /></push-button>
    <push-button @click="toggle_play()"  data-hotkey="RawSpace"
		 data-tip="**CLICK** Start/stop playback" ><b-icon fw lg fa="play"          /></push-button>
    <push-button disabled @click="pcall ('...Record')"    ><b-icon fw lg fa="circle"        /></push-button>
    <push-button disabled @click="pcall ('...Forwards')"  ><b-icon fw lg fa="forward"       /></push-button>
    <push-button disabled @click="pcall ('...Next')"      ><b-icon fw lg fa="fast-forward"  /></push-button>
  </b-button-bar>

</template>

<script>
export default {
  sfc_template,
  methods:  {
    async pcall (method, ev) {
      const project = Data.project;
      let m = project[method], message;
      if (m !== undefined) {
	let result = await m.call (project);
	if (result == undefined)
	  result = 'ok';
	message = method + ': ' + result;
      }
      else
	message = method + ': unimplemented';
      App.status (message);
    },
    async toggle_play() {
      const project = Data.project;
      const playing = await project.is_playing();
      this.pcall (playing ? 'stop_playback' : 'start_playback');
    },
  },
};
</script>
