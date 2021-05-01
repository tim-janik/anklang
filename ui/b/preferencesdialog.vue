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
async function augment_property (xprop) {
  if (xprop.has_choices_)
    {
      Object.assign (xprop.attrs_, { title: xprop.label_ + " Selection" });
      for (let i = 0; i < xprop.value_.choices.length; i++)
	{
	  const c = xprop.value_.choices[i];
	  if (!c.icon && xprop.ident_ == 'pcm_driver')
	    c.icon = adjust_icon (c, 'pcm');
	  else if (!c.icon && xprop.ident_.match (/midi/i))
	    c.icon = adjust_icon (c, 'midi');
	}
    }
}

function adjust_icon (entry, hint) {
  debug (entry);
  const is_midi = hint == 'midi';
  const is_pcm = hint == 'pcm';
  const is_usb = entry.label.match (/^USB /) || entry.blurb.match (/ at usb-/);
  const is_rec  = entry.blurb.match (/\d\*captur/i);
  const is_play = entry.blurb.match (/\d\*play/i);
  if (is_usb)
    entry.blurb = entry.blurb.replace (/ at usb-[0-9].*/, ' (USB)');
  if (is_midi && !(is_rec || is_play))
    entry.blurb = entry.blurb.replace (/\n/, ', ');
  if (entry.ident.startsWith ("jack="))
    return "mi-graphic_eq";
  if (entry.ident.startsWith ("alsa=pulse"))
    return "mi-speaker_group";
  if (entry.ident.startsWith ("null"))
    return "mi-not_interested"; // "fa-deaf";
  if (entry.ident.startsWith ("auto"))
    return "fa-cog";
  if (entry.label.startsWith ("HDMI"))
    return "fa-tv";
  if (entry.label.match (/\bMIDI\W*$/))
    return 'fa-music';
  if (is_usb && is_midi)
    return 'uc-🎘';
  if (is_usb)
    return "fa-usb";
  if (is_midi)
    return 'fa-music';
  if (is_pcm)
    {
      if (entry.blurb.match (/\bModem\b/))
	return "uc-☎ ";
      if (is_rec && !is_play)
	return "mi-mic";
      if (is_play && !is_rec)
	return "fa-volume-up";
      return "mi-headset_mic";
    }
  return "mi-not_interested";
}

function component_data () {
  const data = {
    proplist: { default: [], getter: c => Ase.server.access_prefs(),
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
