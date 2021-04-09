<!-- This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0 -->

<docs>
  # B-PREFERENCESDIALOG
  A [b-modaldialog] to edit preferences.
  ## Events:
  *close*
  : A *close* event is emitted once the "Close" button activated.
</docs>

<style lang="scss" >
  @import 'mixins.scss';
  .b-preferencesdialog	{
    /* max-width: 70em; */
  }
  body .b-fed-picklist-contextmenu { //* popup menus are reparented into a body shield  */
    span.b-preferencesdialog-warning { color: $b-style-fg-warning; }
    span.b-preferencesdialog-notice  { color: $b-style-fg-notice; }
    //* for focus/active: b-fed-picklist-lines already applies $b-style-fg-filter */
  }
</style>

<template>
  <b-modaldialog class="b-preferencesdialog"
		 :shown="shown" @update:shown="$emit ('update:shown', $event)" >
    <template v-slot:header>
      <div>Anklang Preferences</div>
    </template>
    <b-fed-object class="b-preferencesdialog-fed" ref="fedobject" :value="proplist" :augment="augment" @input="value_changed" debounce=75 />
    <template v-slot:footer>
      <div><button autofocus @click="$emit ('update:shown', false)" > Close </button></div>
    </template>
  </b-modaldialog>
</template>

<script>
function drivers2picklist (hint, e) {
  let noticeclass = '';
  if (e.notice.startsWith ("Warning:"))
    noticeclass = "b-preferencesdialog-warning";
  if (e.notice.startsWith ("Note:") || e.notice.startsWith ("Notice:"))
    noticeclass = "b-preferencesdialog-notice";
  const item = {
    uri:  e.devid,
    icon:  this.driver_icon (e, hint),
    label: e.device_name,
    line1: e.capabilities,
    line2: e.device_info,
    // line8: 'Priority: ' + e.priority.toString (16),
    line9: e.notice, line9class: noticeclass,
  };
  return item;
}

async function list_properties (addcleanup) {
  let d = { prefs: Ase.server.access_prefs(),
	    pcmlist: Ase.server.list_pcm_drivers(),
	    midilist: Ase.server.list_midi_drivers(),
  };
  await Util.object_await_values (d);
  this.pcmlist = d.pcmlist;
  this.midilist = d.midilist;
  return d.prefs;
}

async function augment_property (xprop) {
  if (xprop.hints_.search (/:choice:/) >= 0) {
    xprop.attrs_ = Object.assign ({}, xprop.attrs_, { title: xprop.label_ + " Selection" });
  }
}

function component_data () {
  const data = {
    proplist: { default: [], getter: c => list_properties.call (this, c),
		notify: n => { this.proprefresh = n; return () => this.proprefresh = null; }, },
  };
  return this.observable_from_getters (data, () => true);
}

export default {
  sfc_template,
  props: {
    shown: { default: false, type: Boolean },
  },
  emits: { 'update:shown': null, },
  data() { return component_data.call (this); },
  watch: {
    shown (vnew, vold) { if (vnew && this.prefrefresh) this.prefrefresh(); },
  },
  methods: {
    augment (p) { return augment_property.call (this, p); },
    driver_icon (entry, hint) {
      const is_midi = hint == 'midi';
      const is_pcm = hint == 'pcm';
      const is_usb = entry.device_name.match (/^USB /) || entry.device_info.match (/ at usb-/);
      if (entry.devid.startsWith ("jack="))
	return "mi-graphic_eq";
      if (entry.devid.startsWith ("alsa=pulse"))
	return "mi-speaker_group";
      if (entry.devid.startsWith ("null"))
	return "mi-not_interested"; // "fa-deaf";
      if (entry.devid.startsWith ("auto"))
	return "fa-cog";
      if (entry.device_name.startsWith ("HDMI"))
	return "fa-tv";
      if (entry.device_name.match (/\bMIDI\W*$/))
	return 'fa-music';
      if (is_usb && is_midi)
	return 'uc-🎘';
      if (is_usb)
	return "fa-usb";
      if (is_midi)
	return 'fa-music';
      if (is_pcm)
	{
	  if (entry.modem)
	    return "uc-☎ ";
	  if (entry.readonly)
	    return "mi-mic";
	  if (entry.writeonly)
	    return "fa-volume-up";
	  return "mi-headset_mic";
	}
      return "mi-not_interested";
    },
    async value_changed (po) {
      const prefs = await Ase.server.get_prefs();
      Util.assign_forin (prefs, po);
      await Ase.server.set_prefs (prefs);
      if (this.prefrefresh)
	this.prefrefresh();
    },
  },
};
</script>
