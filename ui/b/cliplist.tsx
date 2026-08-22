// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { createEffect, createMemo, onCleanup, For } from 'solid-js';
import * as Util from '../util.js';
import { ClipView } from './clipview';

/** ## Clip-List
 * The Clip-List allows to start playback of individual clips.
 */

// == STYLE ==
Extra_css`
b-cliplist, .b-cliplist {
  display: flex;
  position: relative;
  .-indicator {
    position: absolute; top: 0; bottom: 0; left: 0; width: 1px; height: 100%;
    background: var(--b-piano-roll-indicator);
    z-index: 2;
    transform: translateX(-9999px);
  }
  .b-clipview {
    margin: 0 1px;
    width: var(--b-clipthumb-width);
    flex-shrink: 0; flex-grow: 0;
  }
}`;

// == Component ==
export function ClipList (props)
{
  let container_ref;
  let indicator_bar;
  let clipviews = [];
  let last_pos = -9999;
  const ratiomul = window.devicePixelRatio;
  const ratiodiv = 1.0 / ratiomul;
  let mounted = true;

  // Subscribe to track telemetry reactively, re-subscribe when the track changes
  createEffect (() => {
    const track = props.track;
    if (!track) return;
    let disposed = false;
    let teleobj = null;
    track.telemetry().then (data => {
      if (disposed || !data) return;
      teleobj = Util.telemetry_subscribe (recv_telemetry, data);
    });
    onCleanup (() => {
      disposed = true;
      if (teleobj)
        Util.telemetry_unsubscribe (teleobj);
    });
  });

  // Reactive clip list - launcher_clips is a SolidJS Signal accessor
  const clips = createMemo (() => props.track?.launcher_clips || []);

  // Measure clipviews positions after render/update
  createEffect (() => {
    clips(); // re-measure when launcher_clips change
    if (!container_ref) return;
    requestAnimationFrame (() => {
      if (!mounted || !container_ref) return;
      clipviews.length = 0;
      for (const element of container_ref.querySelectorAll (".b-clipview"))
        clipviews.push ({
          width: element.getBoundingClientRect().width,
          tickscale: element.tickscale,
          x: element.offsetLeft,
        });
    });
  });

  function recv_telemetry (sub, arrays)
  {
    return; // TODO: re-enable for tracktion_engine

    const clips_arr = props.track?.launcher_clips;
    const current = arrays[sub.current_clip.type][sub.current_clip.index];
    const tick = arrays[sub.current_tick.type][sub.current_tick.index];
    let u;
    if (current >= 0 && current < clipviews.length && tick >= 0)
      {
        const cv = clipviews[current];
        const t = cv.x + tick * cv.tickscale;
        u = Math.round (t * ratiomul) * ratiodiv;
      }
    else
      u = -9999;
    if (u != last_pos)
      {
        indicator_bar.style.transform = "translateX(" + u + "px)";
        last_pos = u;
      }
    const [clip, tickfn] = Shell.piano_current();
    if (tickfn && clip == clips_arr?.[current])
      tickfn (clips_arr?.[current], tick);
  }

  onCleanup (() => {
    mounted = false;
  });

  return (
    <div class="b-cliplist" data-f1="cliplist.html" ref={container_ref}>
      <For each={clips()}>
        {(clip, index) => (
          <ClipView clip={clip} index={index()} track={props.track} trackindex={props.trackindex} />
        )}
      </For>
      <span class="-indicator" ref={indicator_bar}></span>
    </div>
  );
}
