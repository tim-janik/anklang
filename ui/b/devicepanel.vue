<!-- This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0 -->

<docs>
  # B-DEVICEPANEL
  Panel for editing of devices.
  ## Props:
  *track*
  : Container for the devices.
</docs>

<style lang="scss">
  $scrollbar-height: 6px; //* Should match Firefox 'scrollbar-width:thin' */
  .b-devicepanel {
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
      padding: 1em 5px;
      border-top-right-radius: inherit;
      border-bottom-right-radius: inherit;
      align-self: stretch;
      //* Add slight shadow to the right for a soft scroll boundary */
      box-shadow: -2px 0 $b-scroll-shadow-blur 0px #000;
      background: #000000ef;
      z-index: 9; //* raise above scrolled siblings */
    }
    .b-devicepanel-after-scroller {
      //* Add slight shadow to the left for a soft scroll boundary */
      box-shadow: 0 0 5px 2px #000;
      width: 1px; background: #000000ef;
      z-index: 9; //* raise above scrolled siblings */
    }
    .b-devicepanel-scroller {
      align-self: stretch;
      padding-top: $scrollbar-height;
      padding-bottom: 0;
      > * {
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
      <template v-for="proc in devs" :key="proc.$id" >
	<b-more @click.stop="menuopen" :sibling.prop="proc"
		data-tip="**CLICK** Add New Elements" />
	<b-deviceeditor :device="proc" center />
      </template>
      <b-more @click.stop="menuopen" :sibling.prop="null"
	      data-tip="**CLICK** Add New Elements" />
      <b-contextmenu ref="devicepanelcmenu" id="g-devicepanelcmenu" :activate.prop="activate" :isactive.prop="isactive" >
	<b-menutitle> Devices </b-menutitle>
	<b-treeselector :tree="devicetypes" :defaultcollapse="false"> </b-treeselector>
      </b-contextmenu>
    </h-flex>
    <span class="b-devicepanel-after-scroller"></span>
  </h-flex>
</template>

<script>
import * as Ase from '../aseapi.js';

async function list_device_types () {
  const deviceinfos = await this.chain_.list_device_types(); // [{ uri, name, category, },...]
  const cats = {};
  for (const e of deviceinfos)
    {
      const category = e.category || 'Other';
      cats[category] = cats[category] || { label: category, type: 'resource-type-folder', entries: [] };
      e.label = e.label || e.name;
      cats[category].entries.push (e);
    }
  const list = [];
  for (const c of Object.keys (cats).sort())
    list.push (cats[c]);
  return Object.freeze ({ entries: list });
}

function observable_device_data () {
  const data = {
    devs:	  { default: [],	 notify: n => this.chain_.on ("sub", n), // sub:insert sub:remove
		    getter: async c => Object.freeze (await this.chain_.list_devices()), },
    devicetypes:  { getter: async c => await list_device_types.call (this), },
    menu_origin:  { default: null, },
  };
  const fetch_chain = async () => {
    if (this.last_track_ != this.track)
      {
	this.chain_ = this.track ? await this.track.access_device() : null;
	this.last_track_ = this.track;
      }
    return this.chain_;
  };
  return this.observable_from_getters (data, fetch_chain);
}

export default {
  sfc_template,
  props: {
    track: { type: Ase.Track, },
  },
  data() { return observable_device_data.call (this); },
  methods: {
    async activate (uri)
    {
      // close popup to remove focus guards
      if (this.chain_ && !uri.startsWith ('DevicePanel:')) // assuming b-treeselector.devicetypes
	{
	  const sibling = this.menu_origin?.sibling;
	  let newdev;
	  if (sibling)
	    newdev = this.chain_.insert_device (uri, sibling);
	  else
	    newdev = this.chain_.append_device (uri);
	  this.menu_origin = null;
	  newdev = await newdev;
	  if (!newdev)
	    console.error ("Ase.insert_device failed, got null:", uri);
	}
    },
    isactive (uri)
    {
      if (!this.track)
	return false;
      return false;
    },
    menuopen (event)
    {
      this.menu_origin = event.target;
      this.$refs.devicepanelcmenu.popup (event, { origin: 'none' });
    },
  },
};
</script>
