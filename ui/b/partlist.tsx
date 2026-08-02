// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { createEffect, createSignal, onCleanup, For } from 'solid-js';
import * as Util from '../util.js';
import { ClipView } from './clipview';

/** @class BPartList
 * @description
 * The <b-partlist> element allows to arrange Clip objects for playback.
 */

// == STYLE ==
Extra_css`
b-partlist, .b-partlist {
  display: flex;
  position: relative;
  .b-clipview {
    margin: 0 1px;
    width: calc(5 * var(--b-clipthumb-width));
    flex-shrink: 0; flex-grow: 0;
  }
}`;

// == COMPONENT ==
export function PartList (props)
{
  const [parts, set_parts] = createSignal ([]);

  // Wrap track to react to arranger part changes
  createEffect (() => {
    const track = props.track;
    let wtrack = { arranger_parts: [] };	// dummy
    if (track && typeof track.arranger_parts === 'function')	// TODO: re-add Ase.Track.arranger_parts()
      wtrack = Util.wrap_ase_object (track, { arranger_parts: [] }, () => set_parts (wtrack.arranger_parts || []));
    set_parts (wtrack.arranger_parts || []);
    onCleanup (() => (wtrack as any)?.__cleanup__?.());
  });

  function dblclick (event)
  {
    if (props.track && typeof props.track.create_part === 'function')	// TODO: re-add Ase.Track.create_part()
      props.track.create_part (0);
  }

  return (
    <div class="b-partlist" data-f1="#part-list" onDblClick={dblclick}>
      <For each={parts ()}>
        {(clip, index) => (
          <ClipView clip={clip} index={index()} track={props.track} trackindex={props.trackindex} />
        )}
      </For>
    </div>
  );
}
