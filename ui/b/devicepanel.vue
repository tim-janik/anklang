<!-- This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0 -->

<docs>
  # B-DEVICEPANEL
  Panel for editing of devices.
  ## Props:
  *track*
  : Container for the devices.
</docs>

<style lang="scss">
  @import 'mixins.scss';

  $scrollbar-height: 6px; //* Should match Firefox 'scrollbar-width:thin' */

  .b-devicepanel {
    height: 100%; width: 0; flex-grow: 1;
    background: $b-devicepanel-bg;
    border-radius: inherit;
    justify-content: flex-start;
    align-items: center;
    @include scrollbar-hover-area;

    .b-devicepanel-before-scroller {
      text-align: center;
      //* FF: writing-mode: sideways-rl; */
      writing-mode: vertical-rl; transform: rotate(180deg);
      border-right: 7px solid #9c61ff;
      padding: 0 5px;
      border-top-right-radius: inherit;
      border-bottom-right-radius: inherit;
      height: 100%;
      //* Add slight shadow to the right for a soft scroll boundary */
      box-shadow: -2px 0 $b-scroll-shadow-blur 0px #000;
      background: #000000ef;
      z-index: 9; //* raise above scrolled siblings */
    }
    .b-devicepanel-after-scroller {
      height: 100%;
      //* Add slight shadow to the left for a soft scroll boundary */
      box-shadow: 0 0 5px 2px #000;
      width: 1px; background: #000000ef;
      z-index: 9; //* raise above scrolled siblings */
    }
    .b-devicepanel-scroller {
      height: 100%; flex-grow: 1;
      padding-top: $scrollbar-height;
      padding-bottom: 0;
      overflow-y: hidden;
      overflow-x: scroll;
      & > * {
	flex-grow: 0;
      }
      .b-more {
	margin-top: $scrollbar-height;
      }
    }
  }
</style>

<template>
  <h-flex class="b-devicepanel" >
    <span class="b-devicepanel-before-scroller"> Device Panel </span>
    <h-flex class="b-devicepanel-scroller" >
      <template v-for="proc in procs" :key="proc.$id" >
	<b-more @click.native.stop="menuopen" :sibling="proc"
		data-tip="**CLICK** Add New Elements" />
	<b-deviceeditor :device="proc" center />
      </template>
      <b-more @click.native.stop="menuopen" :sibling="null"
	      data-tip="**CLICK** Add New Elements" />
      <b-contextmenu ref="cmenu" @click="menuactivation" yscale="1.6" >
	<b-menutitle> Device </b-menutitle>
	<b-menuitem fa="plus-circle"      uri="DevicePanel:add-device" >      Add Device		</b-menuitem>
	<b-menuitem fa="times-circle"     uri="DevicePanel:delete-device" >   Delete Device		</b-menuitem>
	<b-treeselector :tree="devicetypes"> </b-treeselector>
      </b-contextmenu>
    </h-flex>
    <span class="b-devicepanel-after-scroller"></span>
  </h-flex>
</template>

<script>
import * as Ase from '../aseapi.js';

async function list_audio_device_types () {
  if (list_audio_device_types.return_entries)
    return list_audio_device_types.return_entries;
  const crawler = await Ase.server.resource_crawler();
  const entries = await crawler.list_devices (Ase.ResourceType.AUDIO_DEVICE);
  const cats = {};
  for (const e of entries)
    {
      const category = e.category || 'Other';
      cats[category] = cats[category] || { label: category, type: 'resource-type-folder', entries: [] };
      cats[category].entries.push (e);
    }
  const list = [];
  for (const c of Object.keys (cats).sort())
    list.push (cats[c]);
  list_audio_device_types.return_entries = Object.freeze ({ entries: list });
  return list_audio_device_types.return_entries;
}

function observable_device_data () {
  const data = {
    procs:	  { default: [],	 notify: n => this.combo_.on ("sub", n), // sub:insert sub:remove
		    getter: async c => Object.freeze (await this.combo_.list_processors()), },
    devicetypes:  { getter: c => list_audio_device_types(), },
  };
  const fetch_combo = async () => {
    if (this.last_track_ != this.track)
      {
	this.combo_ = this.track ? await this.track.access_combo() : null;
	this.last_track_ = this.track;
      }
    return this.combo_;
  };
  return this.observable_from_getters (data, fetch_combo);
}

export default {
  sfc_template,
  props: {
    track: { type: Ase.Track, },
  },
  data() { return observable_device_data.call (this); },
  methods: {
    menuactivation (uri) {
      const popup_options = this.$refs.cmenu.popup_options;
      // close popup to remove focus guards
      this.$refs.cmenu.close();
      if (uri == 'DevicePanel:add-device' || uri == 'DevicePanel:delete-device')
	debug ("devicepanel.vue:", uri);
      if (this.combo_ && !uri.startsWith ('DevicePanel:')) // assuming b-treeselector.devicetypes
	{
	  if (popup_options.device_sibling)
	    this.combo_.create_processor_before (uri, popup_options.device_sibling);
	  else
	    this.combo_.create_processor (uri);
	}
    },
    menucheck (uri, component) {
      if (!this.track)
	return false;
      switch (uri)
      {
	case 'add-device':   return true;
      }
      return false;
    },
    menuopen (event) {
      const sibling = event.target?.__vue__?.$attrs?.sibling;
      this.$refs.cmenu.popup (event, { check: this.menucheck.bind (this),
				       device_sibling: sibling });
    },
  },
};
</script>
