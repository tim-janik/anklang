<!-- This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0 -->

<docs>
  # B-PREFERENCESDIALOG
  A modal [b-dialog] to edit preferences.
  ## Events:
  *close*
  : A *close* event is emitted once the "Close" button activated.
</docs>

<style lang="scss" >
  @import 'mixins.scss';
  .b-preferencesdialog	{
    /* max-width: 70em; */
  }
</style>

<template>
  <b-dialog class="b-preferencesdialog" :shown="shown" @update:shown="$emit ('update:shown', $event)" >
    <template v-slot:header>
      <div>Anklang Preferences</div>
    </template>
    <b-fed-object class="b-preferencesdialog-fed" ref="fedobject" :value="proplist" :augment="augment" @input="value_changed" debounce=75 />
    <template v-slot:footer>
      <div><button autofocus @click="$emit ('update:shown', false)" > Close </button></div>
    </template>
  </b-dialog>
</template>

<script>
async function augment_property (xprop) {
  if (xprop.has_choices_)
    {
      Object.assign (xprop.attrs_, { title: xprop.label_ + " Selection" });
      for (let i = 0; i < xprop.value_.choices.length; i++)
	{
	  const c = xprop.value_.choices[i];
	  if (xprop.ident_ == 'pcm_driver')
	    augment_choice_entry (c, 'pcm');
	  else if (xprop.ident_.match (/midi/i))
	    augment_choice_entry (c, 'midi');
	}
    }
}

function augment_choice_entry (c, devicetype) {
  const is_midi = devicetype == 'midi';
  const is_usb = c.label.match (/^USB /) || c.blurb.match (/ at usb-/);
  if (is_usb)
    c.blurb = c.blurb.replace (/ at usb-[0-9].*/, ' (USB)');
  if (is_midi)
    c.blurb = c.blurb.replace (/\n/, ', ');
  const standard_icons = ['pcm', 'midi'];
  const icon_hints = c.icon.split (/\s*,\s*/);
  if (c.icon && icon_hints.length == 0 && !standard_icons.includes (c.icon.replace (/\W/, '')))
    return;
  const is_pcm = devicetype == 'pcm';
  if (c.ident.startsWith ("null"))
    c.icon = "mi-not_interested"; // "fa-deaf";
  else if (c.ident.startsWith ("auto"))
    c.icon = "fa-cog";
  else if (is_midi)
    {
      if (c.label.match (/\bMIDI\W*$/))
	c.icon = 'fa-music';
      else if (is_usb)
	c.icon = 'uc-🎘';
      else
	c.icon = 'fa-music';
    }
  else if (is_pcm)
    {
      const is_rec  = c.blurb.match (/\d\*captur/i);
      const is_play = c.blurb.match (/\d\*play/i);
      if (c.ident.startsWith ("jack="))
	c.icon = "mi-graphic_eq";
      else if (c.ident.startsWith ("alsa=pulse"))
	c.icon = "mi-speaker_group";
      else if (c.label.startsWith ("HDMI"))
	c.icon = "fa-tv";
      else if (icon_hints.includes ("headset"))
	c.icon = "mi-headset_mic";
      else if (icon_hints.includes ("recorder"))
	c.icon = "uc-🎙";
      else if (is_usb)
	c.icon = "fa-usb";
      else if (c.blurb.match (/\bModem\b/))
	c.icon = "uc-☎ ";
      else if (is_rec && !is_play)
	c.icon = "mi-mic";
      else if (is_play && !is_rec)
	c.icon = "fa-volume-up";
      else
	c.icon = "uc-💻";
    }
  else
    c.icon = "mi-not_interested";
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
