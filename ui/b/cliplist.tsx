// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { createMemo, For } from 'solid-js';
import { ClipView } from './clipview';

/** ## Clip-List
 * The Clip-List allows to start playback of individual clips.
 */

// == STYLE ==
Extra_css`
.b-cliplist {
  display: flex;
  position: relative;
  .b-clipview {
    margin: 0 1px;
    width: var(--b-clipthumb-width);
    flex-shrink: 0; flex-grow: 0;
  }
}`;

// == Component ==
export function ClipList (props)
{
  // Reactive clip list - launcher_clips is a SolidJS Signal accessor
  const clips = createMemo (() => props.track?.launcher_clips || []);

  return (
    <div class="b-cliplist" data-f1="cliplist.html">
      <For each={clips()}>
        {(clip, index) => (
          <ClipView clip={clip} index={index()} track={props.track} trackindex={props.trackindex} />
        )}
      </For>
    </div>
  );
}
