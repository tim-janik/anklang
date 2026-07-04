// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

/** @class DeviceEditor
 * @description
 * Editor for audio signal devices.
 *
 * ### Props:
 * *device*
 * : Audio signal processing device.
 */

import { createEffect, createSignal, For, onCleanup } from 'solid-js';
import * as Util from "../util.js";
import { PropGroup } from './propgroup.tsx';
import { MenuTitle } from './menutitle.tsx';
import { ContextMenu } from './contextmenu';

// == STYLE ==
Extra_css`
b-deviceeditor, .b-deviceeditor {
  display: flex;
  flex-basis: auto;
  flex-flow: row nowrap;
  align-items: stretch;
  .b-deviceeditor-sw {
    background: var(--b-device-handle);
    border-radius: var(--b-button-radius); border-top-left-radius: 0; border-bottom-left-radius: 0;
    padding: 0 5px;
    text-align: center;
    /* FF: writing-mode: sideways-rl; */
    writing-mode: vertical-rl; transform: rotate(180deg);
  }
  .b-deviceeditor-areas {
    background: var(--b-device-bg);
    grid-gap: 3px;
    border: var(--b-panel-border); /*DEBUG: border-color: #333;*/
    border-radius: var(--b-button-radius); border-top-left-radius: 0; border-bottom-left-radius: 0;
    justify-content: flex-start;
  }
}`;

function guess_layout_rows (number_of_properties)
{
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

function assign_layout_rows (props, n_lrows)
{
  const run = Math.ceil (props.length / n_lrows);
  for (let i = 0; i < props.length; i++)
    {
      const p = props[i];
      p.lrow_ = Math.trunc (i / run);
    }
}

function prop_visible (prop)
{
  const hints = ':' + prop.hints_ + ':';
  if (hints.search (/:G:/) < 0)
    return false;
  return true;
}

/** Determine layout of properties.
 * TODO: handle combinations of 1-unit properties, mixed in with other that may be 2, 3, or 4 units in width.
 */
async function property_groups (asyncpropertylist, add_destroy_callback)
{
  asyncpropertylist = await asyncpropertylist;
  for (let i = 0; i < asyncpropertylist.length; i++)
    asyncpropertylist[i] = Util.extend_property (asyncpropertylist[i], disconnectcallback => add_destroy_callback (disconnectcallback));
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
  for (const c in cols) // forall columns
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

// == COMPONENT ==
export function DeviceEditor (props)
{
  const [gprops, set_gprops] = createSignal ([]);
  const [device_info, set_device_info] = createSignal ({ name: "" });
  let destroy_callbacks = [];
  let deviceeditorcmenu_ref;
  let gen = 0;

  createEffect (() => {
    const device = props.device;
    const my_gen = ++gen;
    // cleanup old destroy callbacks
    if (destroy_callbacks.length) {
      while (destroy_callbacks.length)
        destroy_callbacks.pop().call();
      destroy_callbacks = [];
    }
    set_gprops ([]);
    set_device_info ({ name: "" });

    if (device) {
      const async_fetch_device = async () => {
        const device_ = device;
        const new_destroy_callbacks = [];
        const info_promise = device_.device_info(); // TODO: watch "notify:device_info"
        let gprops = await device_.access_properties();
        const info = Object.freeze (await info_promise);
        if (my_gen !== gen || device_ !== props.device)
          return;
        gprops = await property_groups (gprops, cb => new_destroy_callbacks.push (cb));
        if (my_gen !== gen || device_ !== props.device) {
          while (new_destroy_callbacks.length)
            new_destroy_callbacks.pop().call();
          return;
        }
        destroy_callbacks = new_destroy_callbacks;
        set_device_info (info);
        set_gprops (gprops);
      };
      async_fetch_device();
    }
  });

  onCleanup (() => {
    gen++; // invalidate pending async
    if (destroy_callbacks.length) {
      while (destroy_callbacks.length)
        destroy_callbacks.pop().call();
      destroy_callbacks = [];
    }
  });

  function group_style (group)
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

  function activate (uri)
  {
    switch (uri) {
      case 'delete-device':
        props.device?.remove_self();
        break;
      case 'toggle-gui':
        // TODO: this.device.gui_toggle() — needs PluginImpl::gui_toggle() impl
        break;
    }
  }

  async function isactive (uri, component)
  {
    if (!props.device)
      return false;
    switch (uri) {
      case 'add-device':
        return false;
      case 'delete-device':
        return true;
      case 'toggle-gui':
        // TODO: return await this.device.gui_supported() — needs PluginImpl::gui_supported() impl
        return false;
    }
    return false;
  }

  return (
    <div class="b-deviceeditor">
      <span class="b-deviceeditor-sw" onContextMenu={e => deviceeditorcmenu_ref?.popup (e)}>
        {device_info().name}
      </span>
      <div class="b-deviceeditor-areas grid">
        <For each={gprops()}>
          {(group) => (
            <PropGroup style={group_style (group)} name={group.name} props={group.props} />
          )}
        </For>
      </div>
      <ContextMenu ref={h => deviceeditorcmenu_ref = h} id="g-deviceeditorcmenu" activate={activate} isactive={isactive}>
        <MenuTitle> Device </MenuTitle>
        <button ic="fa-plus_circle" uri="add-device">Add Device</button>
        <button ic="fa-times_circle" uri="delete-device">Delete Device</button>
        <button ic="md-television_guide" uri="toggle-gui">Toggle GUI</button>
      </ContextMenu>
    </div>
  );
}
