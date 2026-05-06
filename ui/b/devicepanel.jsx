// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class B-DevicePanel
 * SolidJS component for editing of devices.
 *
 * ### Props:
 * *track*
 * : Container for the devices (Ase.Track).
 */

import { createSignal, createEffect } from 'solid-js';
import * as Util from "../util.js";
import * as Ase from '../../ase/gen/api-jsonipc.g.ts';

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

// == Component ==
export function DevicePanel (props)
{
  let cmenu_ref;
  const [chain, set_chain] = createSignal (null);
  const [devicetypes, set_devicetypes] = createSignal (null);
  const [menu_sibling, set_menu_sibling] = createSignal (null);

  // Watch track changes and fetch device info
  createEffect (async () => {
    const track = props.track;
    set_chain (null);
    set_devicetypes (null);
    if (!track) return;

    const dev = await track.access_device ();
    const types = !dev ? null : await list_device_types (dev);
    set_devicetypes (types);
    set_chain (dev);
  });

  const activate = async (uri) => {
    // close popup to remove focus guards
    if (!chain () || uri.startsWith ('DevicePanel:')) // assuming b-treebrowser.devicetypes
      return;
    const sibling = menu_sibling ();
    let newdev;
    if (sibling)
      newdev = chain ().insert_device (uri, sibling);
    else
      newdev = chain ().append_device (uri);
    set_menu_sibling (null);
    await newdev;
    if (!newdev)
      console.error ("Ase.insert_device failed, got null:", uri);
  };

  const isactive = (uri) => {
    if (!props.track)
      return false;
    return true;
  };

  const menuopen = (event, sibling) => {
    set_menu_sibling (sibling);
    cmenu_ref?.popup (event, { origin: 'none' });
    Util.prevent_event (event);
  };

  return (
    <div class="b-devicepanel">
      <div class="b-devicepanel-scroller">
      <span class="b-devicepanel-vtitle">Device Panel</span>
      <div class="b-devicepanel-hstack hflex">
        {/* TODO: render devices in chain — needs Device::get_devices() impl */}
        <b-more
          onMousedown={e => menuopen (e)}
          data-tip="**CLICK** Add New Elements"
        ></b-more>
        <b-contextmenu
          ref={e => {
            cmenu_ref = e;
            if (e) {
              e.activate = activate;
              e.isactive = isactive;
            }
          }}
          id="g-devicepanelcmenu"
        >
          <b-menutitle>Devices</b-menutitle>
          <b-treebrowser tree={devicetypes ()} expandall={false}></b-treebrowser>
        </b-contextmenu>
      </div>
      </div>
    </div>
  );
}

// == Helper functions (unchanged from Lit version) ==

/**
 * @param {Ase.Device} [device] - Track device.
 */
async function list_device_types (device)
{
  const deviceinfos = await device.list_device_types (); // [{ uri, name, category, },...]
  const cats = {};
  for (const e of deviceinfos) {
    const category = e.category || 'Other';
    cats[category] = cats[category] || { label: category, type: 'resource-type-folder', entries: [] };
    e.label = e.label || e.name;
    cats[category].entries.push (e);
  }
  const list = [];
  for (const c of Object.keys (cats).sort ())
    list.push (cats[c]);
  return Object.freeze (list);
}
