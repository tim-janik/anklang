<!-- This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0 -->

<docs>
  ## b-positionview - Display of the project transport position pointer and related information
  ### Props:
  - **project** - The object providing playback API.
</docs>

<style lang="scss">
@import 'mixins.scss';
$b-positionview-fg: #71cff2; $b-positionview-b0: #011214; $b-positionview-b1: #00171a;
$b-positionview-bg: mix($b-positionview-b0, $b-positionview-b1);

.b-positionview {
  margin: 0; padding: 5px 1em;
  letter-spacing: 0.05em;
  border-radius: $b-theme-border-radius; align-items: baseline;
  border-top:    1px solid  darken($b-positionview-bg, 3%);
  border-left:   1px solid  darken($b-positionview-bg, 3%);
  border-right:  1px solid lighten($b-positionview-bg, 3%);
  border-bottom: 1px solid lighten($b-positionview-bg, 3%);
  background-color: $b-positionview-bg;
  background: linear-gradient(to bottom, $b-positionview-b0 0%, $b-positionview-b1 100%);
  color: $b-positionview-fg;
  white-space: pre;
  .b-positionview-counter,
  .b-positionview-timer	{ font-size: 110%; margin-right: .5em; }
  .b-positionview-bpm,
  .b-positionview-sig		{ font-size: 90%; margin: 0 0.5em; }
  .b-positionview-sig, .b-positionview-counter, .b-positionview-bpm, .b-positionview-timer {
    position: relative;
    text-align: right;
  }
}
</style>

<template>

  <h-flex class="b-positionview inter tabular-nums" >
    <span class="b-positionview-sig">{{ numerator?.value_.val + '/' + denominator?.value_.val }}</span>
    <span class="b-positionview-counter" ref="counter" />
    <b-editable class="b-positionview-bpm" @change="v => bpm.apply_ (v)" selectall >{{ bpm?.value_.val }}</b-editable>
    <span class="b-positionview-timer" ref="timer" />
  </h-flex>

</template>

<script>
import * as Ase from '../aseapi.js';

async function tick_moniotr (addcleanup) {
  const tickpos_offset = await this.project.get_shm_offset (Ase.SongTelemetry.I32_TICK_POINTER);
  const mon = {
    sub_i32tickpos: Util.shm_subscribe (tickpos_offset, 4),
  };
  const dtor = () => {	// register cleanups
    Util.shm_unsubscribe (mon.sub_i32tickpos);
  };
  addcleanup (dtor);
  return mon;
}

function data () {
  const data = {
    //tpqn:        { getter: c => this.project.get_prop ("tpqn"),        notify: n => this.project.on ("notify:tpqn", n), },
    bpm:	{ getter: c => Util.extend_property (this.project.access_property ("bpm"), c), },
    numerator:	{ getter: c => Util.extend_property (this.project.access_property ("numerator"), c), },
    denominator:{ getter: c => Util.extend_property (this.project.access_property ("denominator"), c), },
    telemetry:	{ getter: c => this.project.telemetry(), },
    //tmon:        { getter: c => tick_moniotr.call (this, c), },
    fps:         { default: 0, },
  };
  return this.observable_from_getters (data, () => this.project);
}

const ds = "\u2007"; // FIGURE SPACE - "Tabular width", the width of digits
const s3 = n => (n >= 100 ? "" : n >= 10 ? ds : ds + ds) + n;
const s2 = n => (n >= 10 ? "" : ds) + n;
const z2 = n => (n >= 10 ? "" : "0") + n;
const ff = (n, d = 2) => Number.parseFloat (n).toFixed (d);

export default {
  sfc_template,
  props: {
    project: { type: Ase.Project, },
  },
  data,
  methods:  {
    dom_create() {
      this.counter_text = document.createTextNode ("");
      this.$refs.counter.appendChild (this.counter_text);
      this.timer_text = document.createTextNode ("");
      this.$refs.timer.appendChild (this.timer_text);
    },
    recv_telemetry (teleobj, arrays) {
      if (!this.timer_text) return;
      const tick = arrays[teleobj.current_tick.type][teleobj.current_tick.index];
      const bpm = arrays[teleobj.current_bpm.type][teleobj.current_bpm.index];
      const bar = arrays[teleobj.current_bar.type][teleobj.current_bar.index];
      const beat = arrays[teleobj.current_beat.type][teleobj.current_beat.index];
      const sixteenth = arrays[teleobj.current_sixteenth.type][teleobj.current_sixteenth.index];
      const minutes = arrays[teleobj.current_minutes.type][teleobj.current_minutes.index];
      const seconds = arrays[teleobj.current_seconds.type][teleobj.current_seconds.index];
      const barpos = s3 (1 + bar) + "." + s2 (1 + beat) + "." + (1 + sixteenth).toFixed (2);
      const timepos = z2 (minutes) + ":" + z2 (ff (seconds, 3));
      if (this.counter_text.nodeValue != barpos)
	this.counter_text.nodeValue = barpos;
      if (this.timer_text.nodeValue != timepos)
	this.timer_text.nodeValue = timepos;
    },
    dom_update() {
      if (!this.teleobj && this.telemetry)
	this.teleobj = Util.telemetry_subscribe (this.recv_telemetry.bind (this), this.telemetry);
      this.last_tickpos = -1;
      this.dom_trigger_animate_playback (false);
      if (this.project && this.tmon)
	{
	  this.i32tickpos = this.tmon.sub_i32tickpos[0] / 4;
	  this.dom_trigger_animate_playback (true);
	}
    },
    dom_destroy() {
      Util.telemetry_unsubscribe (this.teleobj);
      this.teleobj = null;
      this.counter_text = null;
      this.timer_text = null;
    },
  },
};
</script>
