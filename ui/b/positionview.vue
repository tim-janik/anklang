<!-- This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0 -->

<docs>
  ## b-positionview - Display of the project transport position pointer and related information
  ### Props:
  - **project** - The object providing playback API.
</docs>

<style lang="scss">
@import 'mixins.scss';
$b-positionview-fg: $t-lcdscreen-fg;
$b-positionview-bg: $t-lcdscreen-bg;
$b-positionview-b0: zmod($b-positionview-bg,Jz-=1);
$b-positionview-b1: zmod($b-positionview-bg,Jz+=1);

.b-positionview {
  margin: 0; padding: 5px 1em;
  letter-spacing: 0.05em;
  border-radius: $b-button-radius; align-items: baseline;
  border-top:    1px solid zmod($b-positionview-bg, Jz-=3%);
  border-left:   1px solid zmod($b-positionview-bg, Jz-=3%);
  border-right:  1px solid zmod($b-positionview-bg, Jz+=3%);
  border-bottom: 1px solid zmod($b-positionview-bg, Jz+=3%);
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
    height: 1.2em;
  }
  .b-positionview-counter { width: 7em; } /* fixed size reduces layouting during updates */
  .b-positionview-timer	  { width: 7em; } /* fixed size reduces layouting during updates */
}
</style>

<template>

  <h-flex class="b-positionview inter tabular-nums" >
    <b-editable class="b-positionview-sig" @change="event => apply_sig (event.detail.value)"
                selectall >{{ numerator + '/' + denominator }}</b-editable>
    <span class="b-positionview-counter" ref="counter" />
    <b-editable class="b-positionview-bpm" @change="event => project.set_value ('bpm', 0 | event.detail.value)"
		selectall >{{ bpm }}</b-editable>
    <span class="b-positionview-timer" ref="timer" />
  </h-flex>

</template>

<script>
import * as Ase from '../aseapi.js';

function data () {
  const data = {
    //tpqn:        { getter: c => this.project.get_prop ("tpqn"),        notify: n => this.project.on ("notify:tpqn", n), },
    bpm:         { getter: c => this.project.get_value ('bpm'),         notify: n => this.project.on ("notify:bpm", n), },
    numerator:   { getter: c => this.project.get_value ('numerator'),   notify: n => this.project.on ("notify:numerator", n), },
    denominator: { getter: c => this.project.get_value ('denominator'), notify: n => this.project.on ("notify:denominator", n), },
    telemetry:   { getter: async c => Object.freeze (await this.project.telemetry()), },
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
    apply_sig (v) {
      const parts = ("" + v).split ('/');
      if (parts.length == 2) {
	const n = parts[0] | 0, d = parts[1] | 0;
	if (n > 0 && d > 0) {
	  this.project.set_value ('numerator', n);
	  this.project.set_value ('denominator', d);
	}
      }
    },
    dom_create() {
      this.counter_text = document.createTextNode ("");
      this.$refs.counter.appendChild (this.counter_text);
      this.timer_text = document.createTextNode ("");
      this.$refs.timer.appendChild (this.timer_text);
    },
    recv_telemetry (teleobj, arrays) {
      if (!this.timer_text) return;
      // const tick = arrays[teleobj.current_tick.type][teleobj.current_tick.index];
      // const bpm = arrays[teleobj.current_bpm.type][teleobj.current_bpm.index];
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
      if (!this.teleobj && this.telemetry) {
	const telefields = [ 'current_bar', 'current_beat', 'current_sixteenth', 'current_minutes', 'current_seconds' ];
	const subscribefields = this.telemetry.filter (field => telefields.includes (field.name));
	this.teleobj = Util.telemetry_subscribe (this.recv_telemetry.bind (this), subscribefields);
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
