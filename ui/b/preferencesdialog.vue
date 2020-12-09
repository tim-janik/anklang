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
    <b-fed-object class="b-preferencesdialog-fed" ref="fedobject" :value="proplist" @input="value_changed" debounce=75 />
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
async function fetch_current_config (addcleanup) { // FIXME
  const d = await Ase.server.get_prefs();
  if (d.__typename__)
    {
      d.__typedata__ = []; // TODO: await Ase.server.find_typedata (d.__typename__);
      d.__fieldhooks__ = {
	'pcm_driver.picklistitems': () => {
	  if (this.pcmrefresh)  // Ase provides no notification for list_pcm_drivers
	    this.pcmrefresh();  // so we poll it when necessary
	  return this.pcmlist.map (drivers2picklist.bind (this, 'pcm'));
	},
	'midi_driver.picklistitems': () => {
	  if (this.midirefresh) // Ase provides no notification for list_midi_drivers
	    this.midirefresh(); // so we poll it when necessary
	  return this.midilist.map (drivers2picklist.bind (this, 'midi'));
	},
      };
    }
  return Object.freeze (d);
}

function component_data () {
  const data = {
    proplist: { default: [], getter: async c => await Ase.server.access_prefs(), },
    pcmlist:  { getter: async c => Object.freeze (await Ase.server.list_pcm_drivers()),
		notify: n => { this.pcmrefresh = n; return () => this.pcmrefresh = null; }, },
    midilist: { getter: async c => Object.freeze (await Ase.server.list_midi_drivers()),
		notify: n => { this.midirefresh = n; return () => this.midirefresh = null; }, },
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
