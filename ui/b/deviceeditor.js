// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class BDeviceEditor
 * @description
* Editor for audio signal devices.
 *
 * ## Props:
 * *device*
 * : Audio signal processing device.
 */

import { LitComponent, html, render, noChange, JsExtract, docs, ref } from '../little.js';

// == STYLE ==
JsExtract.scss`
b-deviceeditor {
  display: flex;
  flex-basis: auto;
  flex-flow: row nowrap;
  align-items: stretch;
  .b-deviceeditor-sw {
    background: $b-device-handle;
    border-radius: $b-button-radius; border-top-left-radius: 0; border-bottom-left-radius: 0;
    padding: 0 5px;
    text-align: center;
    /* FF: writing-mode: sideways-rl; */
    writing-mode: vertical-rl; transform: rotate(180deg);
  }
  .b-deviceeditor-areas {
    background: $b-device-bg;
    grid-gap: 3px;
    border: $b-panel-border; /*DEBUG: border-color: #333;*/
    border-radius: $b-button-radius; border-top-left-radius: 0; border-bottom-left-radius: 0;
    justify-content: flex-start;
  }
}`;

// == HTML ==
const GROUP_HTML = (t, group) => html`
    <b-propgroup style=${t.group_style (group)} name=${group.name} .props=${group.props} ></b-propgroup>
`;
const HTML = (t, d) => html`
<span class="b-deviceeditor-sw" @contextmenu=${e => t.deviceeditorcmenu.popup (e, null)}
  > ${ t.device_info.name } </span>
<c-grid class="b-deviceeditor-areas" >
  ${ t.gprops.map (group => GROUP_HTML (t, group)) }
</c-grid>
<b-contextmenu ${ref (h => t.deviceeditorcmenu = h)} id="g-deviceeditorcmenu" .activate=${t.activate.bind (t)} .isactive=${t.isactive.bind (t)} >
  <b-menutitle> Device </b-menutitle>
  <button fa="plus-circle"      uri="add-device" >      Add Device		</button>
  <button fa="times-circle"     uri="delete-device" >   Delete Device		</button>
  <button mi="video_settings"   uri="toggle-gui" >      Toggle GUI		</button>
</b-contextmenu>
`;

// == SCRIPT ==
import * as Ase from '../aseapi.js';
import * as Util from "../util.js";

function guess_layout_rows (number_of_properties) {
  let n_lrows = 1;
  if (number_of_properties > 6)
    n_lrows = 2;
  if (number_of_properties > 12)
    n_lrows = 3;
  if (number_of_properties > 18)
    n_lrows = 4;
  if (number_of_properties > 24)
    {/**/} // n_lrows = 5; is not supported, see rows_from_lrows()
  return n_lrows;
}

function assign_layout_rows (props, n_lrows) {
  const run = Math.ceil (props.length / n_lrows);
  for (let i = 0; i < props.length; i++)
    {
      const p = props[i];
      p.lrow_ = Math.trunc (i / run);
    }
}

function prop_visible (prop) {
  const hints = ':' + prop.hints_ + ':';
  if (hints.search (/:G:/) < 0)
    return false;
  return true;
}

/** Determine layout of properties.
 * @this{BDeviceEditor}
 * TODO: handle combinations of 1-unit properties, mixed in with other that may be 2, 3, or 4 units in width.
 */
async function property_groups (asyncpropertylist)
{
  asyncpropertylist = await asyncpropertylist;
  for (let i = 0; i < asyncpropertylist.length; i++)
    asyncpropertylist[i] = Util.extend_property (asyncpropertylist[i], disconnectcallback => Util.add_destroy_callback.call (this, disconnectcallback));
  for (let i = 0; i < asyncpropertylist.length; i++)
    asyncpropertylist[i] = await asyncpropertylist[i];
  // split properties into group lists
  const grouplists = {}, groupnames = [];
  for (const p of asyncpropertylist)
    {
      if (!prop_visible (p))
	continue;
      const groupname = p.group_;
      if (!grouplists[groupname])
	{
	  groupnames.push (groupname);
	  grouplists[groupname] = [];
	}
      grouplists[groupname].push (p);
    }
  // split big groups
  const GMAX = 64;
  for (const group of [...groupnames]) // allow in-loop changes to groupnames
    if (grouplists[group].length > GMAX) {
      const PGCOUNT = Math.trunc (grouplists[group].length / GMAX);
      const pages = [], pagenames = [];
      for (let i = 0; i < PGCOUNT; i++)
	pages.push (grouplists[group].splice (0, GMAX));
      if (grouplists[group].length)
	pages.push (grouplists[group]);
      delete grouplists[group];
      for (let i = 0; i < pages.length; i++) {
	const pname = (group + ' Page ' + (1 + i)).trim();
	pagenames.push (pname);
	grouplists[pname] = pages[i];
      }
      groupnames.splice (groupnames.indexOf (group), 1, ...pagenames);
    }
  // create group objects
  const grouplist = [];
  for (const name of groupnames)
    {
      const props = grouplists[name];
      const n_lrows = guess_layout_rows (props.length);
      const group = {
	name, props, n_lrows,
	col: undefined,
	cspan: undefined,
	row: undefined,
	rspan: undefined,
      };
      grouplist.push (group);
    }
  // determine grid rows from group internal layout rows
  const rows_from_lrows = (n_lrows) => {
    /* Available vertical panel areas:
     * 1lrow   2lrows    3lrows       4lrows
     * 123456 123456789 123456789012 123456789012345
     * TT kkk TT kkkqqq TT kkkqqqkkk TT kkkqqqkkkqqq
     *
     * Supporting 5 lrows would not leave room for another panel after a 4 lrow panel,
     * so the 4 lrows and 5 lrows panels would always be stretched to same grid rows.
     */
    if (n_lrows == 1)
      return 2;			// title + knobs
    if (n_lrows == 2)
      return 3;			// title + knobs + knobs
    if (n_lrows == 3)
      return 4;			// title + knobs + knobs + knobs
    if (n_lrows == 4)
      return 5;			// title + knobs + knobs + knobs + knobs
    if (n_lrows == 5)
      {/**/} // return 6; - not supported, layout becomes too crammed
  };
  const lrows_from_rows = (nrows) => nrows - 1; // see rows_from_lrows()
  // wrap groups into columns
  const maxrows = 5, cols = {};
  let c = 0, r = 0;
  for (const group of grouplist)
    {
      let rspan = rows_from_lrows (group.n_lrows);
      if (r > 1 && r + rspan > maxrows)
	{
	  c += 1;
	  r = 0;
	}
      group.col = c;
      group.row = r;
      group.rspan = rspan;
      r += rspan;
      if (!cols[c])
	cols[c] = [];
      cols[c].push (group);
    }
  // distribute excess column space
  for (const c in cols)  // forall columns
    {
      const cgroups = cols[c];
      let r = 0;
      for (const g of cgroups) // forall groups in column
	r += g.rspan;
      const extra = Math.trunc ((maxrows - r) / cgroups.length);
      // distribute extra space evenly
      cgroups[0].rspan += extra;
      r = cgroups[0].rspan;
      for (let i = 1; i < cgroups.length; i++)
	{
	  const prev = cgroups[i - 1];
	  cgroups[i].row = prev.row + prev.rspan;
	  cgroups[i].rspan += extra;
	  r += cgroups[i].rspan;
	}
      // close gap of last row to bottom
      if (r < maxrows)
	cgroups[cgroups.length - 1].rspan += maxrows - r;
    }
  // rspan expansion might have made room for another lrow
  for (const group of grouplist)
    group.n_lrows = lrows_from_rows (group.rspan);
  // assign properties to inner group rows
  for (const group of grouplist)
    assign_layout_rows (group.props, group.n_lrows);
  return Object.freeze (grouplist); // list of groups: [ { name, props: [ Prop... ] }... ]
}

class BDeviceEditor extends LitComponent {
  createRenderRoot() { return this; }
  render()
  {
    const d = {};
    return HTML (this, d);
  }
  static properties = {
    gprops: { state: true }, // private property
    device: { type: Ase.Device, state: true }, // private property
  };
  constructor()
  {
    super();
    this.device = null;
    this.gprops = [];
    this.device_info = "";
  }
  updated (changed_props)
  {
    if (changed_props.has ('device')) {
      this.gprops = [];
      this.device_info = "";
    }
    if (changed_props.has ('device') && this.device) {
      const async_fetch_device = async () => {
	const device = this.device;
	let device_info = device.device_info(); // TODO: watch "notify:device_info"
	let gprops = device.access_properties();
	gprops = await gprops;
	device_info = Object.freeze (await device_info);
	if (device === this.device)
	  gprops = await property_groups.call (this, gprops);
	if (device === this.device) {
	  this.gprops = gprops;
	  this.device_info = device_info;
	}
      };
      async_fetch_device();
    }
  }
  unmounted()
  {
    Util.call_destroy_callbacks.call (this);
    this.gprops = [];
    this.device_info = "";
  }
  group_style (group)
  {
    let s = '';
    if (group.row !== undefined)
      {
	s += 'grid-row:' + (1 + group.row);
	if (group.rspan)
	  s += '/span ' + group.rspan;
	s += ';';
      }
    if (group.col !== undefined)
      {
	s += 'grid-column:' + (1 + group.col);
	if (group.cspan)
	  s += '/span ' + group.cspan;
	s += ';';
      }
    return s;
  }
  activate (uri)
  {
    switch (uri) {
      case 'delete-device':
	this.device.remove_self();
	break;
      case 'toggle-gui':
	this.device.gui_toggle();
	break;
    }
  }
  async isactive (uri, component)
  {
    if (!this.device)
      return false;
    switch (uri) {
      case 'add-device':
	return false;
      case 'delete-device':
	return true;
      case 'toggle-gui':
	return await this.device.gui_supported();
    }
    return false;
  }
}
customElements.define ('b-deviceeditor', BDeviceEditor);
