// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class B-TrackList
 * SolidJS component for vertical display of Ase.Track instances.
 *
 * ### Props:
 * *project*
 * : The *Ase.Project* containing playback tracks.
 */

import { createSignal, createEffect, onCleanup, For } from 'solid-js';
import { ClipList } from './cliplist';
import { TrackView } from './trackview.tsx';

// == STYLE ==
Extra_css`
:root {
  --scroll-shadow-inset: 7px;
}
b-tracklist, .b-tracklist {
  display: flex; flex-direction: column; align-items: stretch;
  position: relative;
  .grid {
    display: grid; flex-grow: 1;
    position: relative; inset: 0;
    padding: 0 3px; /* 0.5*var(--scroll-shadow-inset) */
    align-items: stretch;
    grid-template-columns: min-content 3fr 2fr;
    grid-template-rows: min-content 1fr min-content;
    background-color: var(--b-tracklist-bg);
    overflow: hidden;
    /* for :before box-shadow */
  }
  .trackviews,
  .partlists,
  .cliplists {
    display: flex; flex-flow: column nowrap;
    align-items: flex-start; /* needed for scroll-x */
    padding: var(--scroll-shadow-inset) 0;
    min-height: 0;
    > * { height: 46px; box-sizing: border-box; overflow: hidden; flex-grow: 0; flex-shrink: 0; }
  }
  .trackviews {
    grid-area: 2/1 / 3/2;
    overflow: scroll hidden;
  }
  .cliplists {
    grid-area: 2/2 / 3/3;
    overflow: scroll;
  }
  .partlists {
    grid-area: 2/3 / 3/4;
    overflow: scroll;
  }
  .scrollshadow {
    grid-area: 2/1 / 3/4;
    position: absolute; top: 0; left: 0; height: 200%;
    width: calc(100% + 200px); margin-left: -100px;
    /* add inner box-shadow to indicate scrolling borders */
    z-index: 1; pointer-events: none; user-select: none;
    box-shadow: black 0 0 var(--scroll-shadow-inset) 0px inset;
  }
}`;

// == Component ==
export function TrackList (props)
{
  const [tracks, set_tracks] = createSignal ([]);
  let trackviews_ref;
  let cliplists_ref;
  let partlists_ref;

  // Fetch tracks when project changes and on notifications
  createEffect (async () => {
    const project = props.project;
    if (!project) {
      set_tracks ([]);
      return;
    }
    const list = await project.all_tracks ();
    set_tracks (list || []);
  });

  // Listen for track list changes
  let notify_cleanup;
  createEffect (() => {
    const project = props.project;
    if (notify_cleanup) {
      notify_cleanup ();
      notify_cleanup = undefined;
    }
    if (project) {
      notify_cleanup = project.on ("notify:all_tracks", async () => {
        const list = await project.all_tracks ();
        set_tracks (list || []);
      });
    }
  });

  onCleanup (() => {
    if (notify_cleanup) {
      notify_cleanup ();
      notify_cleanup = undefined;
    }
  });

  // Setup scroll sync after DOM is ready
  createEffect (() => {
    if (!cliplists_ref || !trackviews_ref || !partlists_ref)
      return;
    const onscroll = (event) => {
      const st = event.target.scrollTop;
      trackviews_ref.scrollTop = st;
      cliplists_ref.scrollTop = st;
      partlists_ref.scrollTop = st;
    };
    cliplists_ref.addEventListener ('scroll', onscroll);
    partlists_ref.addEventListener ('scroll', onscroll);
    trackviews_ref.addEventListener ('scroll', onscroll);
  });

  const list_dblclick = (event) => {
    if (event.target === trackviews_ref && props.project)
      props.project.create_track ();
  };

  return (
    <div class="b-tracklist" onDblclick={list_dblclick}>
      <div class="grid">
        <div style="grid-area: 1/1 / 2/4;"> {/* HEADER */} </div>
        <div class="trackviews" ref={e => trackviews_ref = e}>
          <For each={tracks ()}>
            {(track, idx) => (
              <TrackView track={track} trackindex={idx ()} />
            )}
          </For>
        </div>
        <div class="cliplists" ref={e => cliplists_ref = e}>
          <For each={tracks ()}>
            {(track, idx) => (
              <ClipList track={track} trackindex={idx ()} />
            )}
          </For>
        </div>
        <div class="partlists" ref={e => partlists_ref = e}>
          <For each={tracks ()}>
            {(track, idx) => (
              <ClipList track={track} trackindex={idx ()} />
            )}
          </For>
        </div>
        <div style="grid-area: 3/1 / 4/4;"> {/* FOOTER */} </div>
        <div class="scrollshadow"></div>
      </div>
    </div>
  );
}
