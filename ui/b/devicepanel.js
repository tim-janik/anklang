// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class BDevicePanel
 * @description
 * Panel for editing of devices.
 * ## Props:
 * *track*
 * : Container for the devices.
 */

import { LitComponent, html, render, noChange, JsExtract, docs, ref, repeat } from '../little.js';
import * as Util from "../util.js";
import * as Kbd from '../kbd.js';
import { text_content, get_uri, valid_uri, has_uri } from '../dom.js';

// == STYLE ==
Extra_css`
@reference "../tailwind.css";
--scrollbar-height: 6px; /* Should match Firefox 'scrollbar-width:thin' */
b-devicepanel {
  @apply hflex;
  padding: 0 0 3px 0;
  background: var(--b-devicepanel-bg);
  border-radius: inherit;
  overflow: hidden;

  .b-devicepanel-scroller {
    @apply hflex;
    overflow: auto visible;
  justify-content: flex-start;
  align-items: center;
  }

  .b-devicepanel-vtitle {
    text-align: center;
    /* FF: writing-mode: sideways-rl; */
    writing-mode: vertical-rl;
    transform: rotate(180deg);
    border-right: 7px solid #9c61ff;
    padding: 1em 5px;
    border-top-right-radius: inherit;
    border-bottom-right-radius: inherit;
    align-self: stretch;
    /* Add slight shadow to the right for a soft scroll boundary */
    box-shadow: -2px 0 var(--b-scroll-shadow-blur) 0px #000;
    background: #000000ef;
    z-index: 9; /* raise above scrolled siblings */
  }
  .b-devicepanel-hstack {
    align-self: stretch;
    padding-top: var(--scrollbar-height);
    padding-bottom: 0;
    > * { flex-grow: 0; }
    .b-more { margin-top: var(--scrollbar-height); }
  }
  position: relative;
  &::after {
    @apply pointer-events-none absolute inset-0;
    content: ' '; z-index: 9; /* raise above scrolled siblings */
    box-shadow: inset -10px 0 7px -7px #000, inset 10px 0 7px -7px #000;
  }
}`;

// == HTML ==
const DEVICE_HTML = (t, dev) => html`
  <b-more @mousedown=${e => t.menuopen (e, dev)}
    data-tip="**CLICK** Add New Elements" ></b-more>
  <b-deviceeditor .device=${dev} center ></b-deviceeditor>
`;
const HTML = (t) => html`
  <div class="b-devicepanel-scroller" >
    <span class="b-devicepanel-vtitle"> Device Panel </span>
    <div class="b-devicepanel-hstack hflex" >
      <!-- TODO: $ {repeat (t.chain_?.devices || [], dev => dev.$id, dev => DEVICE_HTML (t, dev))} — needs Device::get_devices() impl -->
      <b-more @mousedown=${e => t.menuopen (e)}
	data-tip="**CLICK** Add New Elements" ></b-more>
      <b-contextmenu ${ref (h => t.devicepanelcmenu = h)}
	id="g-devicepanelcmenu" .activate=${t.activate.bind (t)} .isactive=${t.isactive.bind (t)} >
	<b-menutitle> Devices </b-menutitle>
	<b-treebrowser .tree=${t.devicetypes} ?expandall="false"> </b-treebrowser>
      </b-contextmenu>
    </div>
  </div>
`;

// == SCRIPT ==
import * as Ase from '../../ase/gen/api-jsonipc.g.ts';

/**
 * @param {Ase.Device} [device] - Track device.
 */
async function list_device_types (device)
{
  const deviceinfos = await device.list_device_types(); // [{ uri, name, category, },...]
  const cats = {};
  for (const e of deviceinfos) {
    const category = e.category || 'Other';
    cats[category] = cats[category] || { label: category, type: 'resource-type-folder', entries: [] };
    e.label = e.label || e.name;
    cats[category].entries.push (e);
  }
  const list = [];
  for (const c of Object.keys (cats).sort())
    list.push (cats[c]);
  return Object.freeze (list);
}

class BDevicePanel extends LitComponent {
  createRenderRoot() { return this; }
  render() { return HTML (this); }
  static properties = {
    track:	{ type: Ase.Track, reflect: true },
  };
  constructor()
  {
    super();
    this.track = null;
    this.chain_ = null;
    this.menu_sibling = null;
    this.devicepanelcmenu = null;
  }
  updated (changed_props)
  {
    let info_promise;
    if (changed_props.has ('track')) {
      this.chain_ = null;
      this.devicetypes = null;
      const track_fetch_device = async () => {
	const chain = await this.track.access_device();
	const devicetypes = !chain ? null : await list_device_types (chain);
	this.devicetypes = devicetypes;
	this.chain_ = chain;
	this.request_update();
      };
      if (this.track)
	track_fetch_device();
    }
  }
  async activate (uri)
  {
    // close popup to remove focus guards
    if (this.chain_ && !uri.startsWith ('DevicePanel:')) // assuming b-treebrowser.devicetypes
      {
	const sibling = this.menu_sibling;
	let newdev;
	if (sibling)
	  newdev = this.chain_.insert_device (uri, sibling);
	else
	  newdev = this.chain_.append_device (uri);
	this.menu_sibling = null;
	newdev = await newdev;
	if (!newdev)
	  console.error ("Ase.insert_device failed, got null:", uri);
      }
  }
  isactive (uri)
  {
    if (!this.track)
      return false;
    return true;
  }
  menuopen (event, sibling)
  {
    this.menu_sibling = sibling;
    this.devicepanelcmenu.popup (event, { origin: 'none' });
    Util.prevent_event (event);
  }
}
customElements.define ('b-devicepanel', BDevicePanel);
