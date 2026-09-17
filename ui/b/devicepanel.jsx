// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class B-DevicePanel
 * SolidJS component for editing of devices.
 *
 * ### Props:
 * *track*
 * : Container for the devices (Ase.Track).
 */

import { createResource, splitProps } from 'solid-js';
import * as Util from "../util.js";
import * as Ase from '../../ase/gen/api-jsonipc.g.ts';
import { More } from './more';
import { ContextMenu } from './contextmenu';

// == STYLE ==
Extra_css`
@reference "../tailwind.css";
--scrollbar-height: 6px; /* Should match Firefox 'scrollbar-width:thin' */
.b-devicepanel {
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
  const [local, rest] = splitProps (props, ['class', 'track']);
  let cmenu_ref;
  const [device_data] = createResource (() => local.track, async track => {
    const chain = await track.access_device();
    return chain ? { chain, items: await list_device_types (chain) } : null;
  });
  const ready = () => !device_data.loading && device_data();

  const activate = async (uri) => {
    const chain = ready()?.chain;
    if (chain && !await chain.append_device (uri))
      console.error ('Ase.append_device failed, got null:', uri);
  };

  const menuopen = (event) => {
    cmenu_ref?.popup (event, { origin: 'none' });
    Util.prevent_event (event);
  };

  return (
    <div {...rest} class={'b-devicepanel' + (local.class ? ' ' + local.class : '')}>
      <div class="b-devicepanel-scroller">
      <span class="b-devicepanel-vtitle">Device Panel</span>
      <div class="b-devicepanel-hstack hflex">
        <More
          onMousedown={e => menuopen (e)}
          data-tip="**CLICK** Add New Elements"
        />
        <ContextMenu
          ref={e => { cmenu_ref = e; }}
          activate={activate}
          isactive={() => !!ready()}
          id="g-devicepanelcmenu"
          items={[{ type: 'title', label: 'Devices' }, ...(ready()?.items ?? [])]}
        />
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
    cats[category] = cats[category] || { type: 'submenu', label: category, items: [] };
    cats[category].items.push ({ uri: e.uri, label: e.label || e.name });
  }
  const list = [];
  for (const c of Object.keys (cats).sort ())
    list.push (cats[c]);
  return Object.freeze (list);
}
