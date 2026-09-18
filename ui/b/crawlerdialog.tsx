// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class BCrawlerDialog
 * A modal [dialog] that allows file and directory selections.
 *
 * ## Properties:
 * *title*
 * : The dialog title.
 * *button*
 * : Title of the activation button.
 * *cwd*
 * : Initial path to start out with.
 * *filters*
 * : List of file type constraints.
 *
 * ## Callbacks:
 * *onSelect (uri)*
 * : Called when a specific path is selected via clicks or focus activation.
 * *onClose*
 * : Called when the "Close" button activated.
 */

import { createSignal, createResource, createMemo, onMount, onCleanup, For, Show } from 'solid-js';
import * as Ase from '../../ase/gen/api-jsonipc.g.ts';
import { hex, basename, dirname, displayfs, displaybasename, displaydirname } from '../strings.js';
import * as Util from "../util.js";
import * as Kbd from "../kbd.js";
import * as Dom from "../dom.js";
import { Icon } from './icon';

// == STYLE ==
Extra_css`
@reference "../tailwind.css";
.b-crawlerdialog {
  dialog {
    width: unset; /* <- leave width to INPUT.-file, see below */
    max-width: 95%;
    height: 45em; max-height: 95%;
    overflow-y: hidden;
  }
  input.-direntry {
    @apply pl-[var(--b-button-radius)] pr-[var(--b-button-radius)] text-left outline-0;
    &::selection { background: #2d53c4; }
    z-index: 1;	/* push onto its own layer */
    @apply b-style-inset;
    /* @include b-focus-outline; */
  }
  input.-pathentry {
    @apply rounded-[var(--b-button-radius)] pl-[var(--b-button-radius)] pr-[var(--b-button-radius)] text-left outline-0;
    &::selection { background: #2d53c4; }
    /* <INPUT/> change causes re-layout: https://bugs.chromium.org/p/chromium/issues/detail?id=1116001 */
    z-index: 1;	/* push onto its own layer */
    @apply b-style-inset;
    /* @include b-focus-outline; */
  }
  .-entry-grid {
    @apply grid grow grid-flow-col justify-start justify-items-start border border-solid border-[#222] text-[#eee];
    gap: 5px 10px;
    overflow: scroll hidden;
    grid-template-rows: repeat(auto-fit, 1.5em);
    grid-auto-columns: max-content;
    background: #111;
    color: #eee;
  }
  .-entry-grid > button {
    @apply m-0 inline-block inline-flex cursor-pointer flex-col truncate rounded-none border-[none] p-0 pr-1 text-[unset] no-underline;
    flex-flow: row nowrap;
    min-width: 10em;
    background: unset; font: unset;
    -webkit-appearance: none; -moz-appearance: none;
    &:active { border: none; }
    @include b-focus-outline;
    .b-icon {
      width: 1.9rem;
      vertical-align: middle;
      @include b-font-weight-bold();
      &[ic="md-folder"] { color: #bba460; }
    }
  }
}`;

// == Component ==
export function CrawlerDialog (props)
{
  // Each opening owns a fresh crawler and one native dialog result.
  return <Show when={props.shown}><CrawlerContent {...props} /></Show>;
}

function CrawlerContent (props)
{
  let dialogRef: any;
  let direntryRef: any;
  let pathentryRef: any;
  let entrygridRef: any;

  const [crawler] = createResource (async () => {
    const c = await Ase.server.dir_crawler (props.cwd || '~MUSIC');
    await c.$refetch (() => [c.folder, c.entries]);
    return c;
  });
  const [assigning, set_assigning] = createSignal (false);
  let alive = true;
  let selected: string | undefined;

  const ctrl_l_grab_focus = () => {
    pathentryRef.focus();
    pathentryRef.select();
  };

  onMount (() => {
    Dom.show_modal (dialogRef);
    Kbd.add_hotkey ('Ctrl+L', ctrl_l_grab_focus, dialogRef);
  });
  onCleanup (() => {
    alive = false;
    Kbd.remove_hotkey ('Ctrl+L', ctrl_l_grab_focus);
    dialogRef.close();
  });

  const handle_close = () => {
    if (!alive)
      return;
    if (selected !== undefined)
      props.onSelect?.(selected);
    else
      props.onClose?.();
  };

  const folder = createMemo (() => displayfs ((crawler()?.folder?.uri || '/').replace (/^file:\/+/, '/')));
  const unfiltered_entries = () => crawler()?.entries || [];

  /// filtered_entries - filter hidden entries
  const filtered_entries = createMemo (() => {
    let e = unfiltered_entries();
    if (!Array.isArray (e)) e = [];
    // e = e.slice (0, 500); // limit number of entries
    e = e.filter ((a: any) => a.label && (a.label == '..' || a.label[0] != '.'));
    e.sort (function (a: any, b: any) {
      if (a.type != b.type)
        return a.type > b.type ? -1 : +1;
      if (a.mtime != b.mtime)
        { /* return a.mtime - b.mtime; */ }
      const al = a.label.toLowerCase(), bl = b.label.toLowerCase();
      if (al != bl)
        return al < bl ? -1 : +1;
      if (a.label != b.label)
        return a.label < b.label ? -1 : +1;
      return 0;
    });
    return e;
  });

  const update_inflight = () => crawler.loading || assigning();

  /// assign_utf8path - assign a path in UTF-8 encoding and possibly select it
  const assign_utf8path = async (filepath: string, pickfile = false) =>
  {
    if (update_inflight() || !dialogRef.open)
      return;
    set_assigning (true);
    try {
      const c = crawler();
      const [, file] = await c.assign (filepath, props.existing !== false);
      await c.$asyncs();
      if (!alive || !dialogRef.open)
        return;
      pathentryRef.value = file;
      if (document.activeElement === document.body)
        pathentryRef.focus();
    } finally {
      set_assigning (false);
    }
    if (pickfile)
      select_entry (null);
  };

  const entrygrid_keydown = (event: KeyboardEvent) =>
  {
    if (Kbd.match_key_event (event, 'Tab')) {
      Util.prevent_event (event);
      pathentryRef?.focus();
    } else
      Kbd.keydown_move_focus (event);
  };

  const pathentry_keydown = (event: KeyboardEvent) =>
  {
    if (Kbd.match_key_event (event, 'Enter'))
      assign_utf8path ((event.target as HTMLInputElement).value, true);
    else if (Kbd.match_key_event (event, 'Shift+Tab')) {
      const entries = Kbd.list_focusables (entrygridRef);
      if (entries.length) {
        Util.prevent_event (event);
        (entries[0] as HTMLElement).focus();
      }
    }
    //  this.assign_utf8path (event.target.value, false);
    // else Kbd.keydown_move_focus_up (event);
  };

  const entry_event = (event: Event, entry: any) => {
    if (update_inflight())
      return;
    pathentryRef.value = entry.label;
    if (event.type === 'dblclick' || (event.type === 'click' && (event as MouseEvent).detail === 0)) {
      if (entry.uri.endsWith ('/'))
        assign_utf8path (entry.uri);
      else
        select_entry (entry);
    }
  };

  const select_entry = (entry: any) => {
    if (update_inflight() || !dialogRef.open)
      return false;
    let uri = entry?.uri;
    if (!uri) {
      const filename = pathentryRef.value.trim();
      if (!filename || filename.includes ('/'))
        return false;
      uri = folder().replace (/\/$/, '') + '/' + filename;
    }
    if (uri.endsWith ('/'))
      return false;
    selected = uri;
    dialogRef.close();
    return true;
  };

  return (
    <div class="b-crawlerdialog">
      <dialog ref={dialogRef} onClose={handle_close}
        class="floating-dialog [&:not([open])]:hidden">
        <div class="dialog-header">{props.title || 'File Dialog'}</div>

        <input class="-direntry pointer-events-none mb-4 select-none outline outline-2 outline-offset-2"
          ref={direntryRef}
          value={folder()}
          tabindex="-1" readonly
          onFocus={() => pathentryRef?.focus()}
          inert
          type="text"
          onSelect={Util.prevent_event} />

        <div data-subfocus="*" class="-entry-grid grid"
          ref={entrygridRef}
          onKeyDown={entrygrid_keydown}>
          <For each={filtered_entries()}>
            {(entry: any) => (
              <button
                onFocus={ev => entry_event (ev, entry)}
                onClick={ev => entry_event (ev, entry)}
                onDblClick={ev => entry_event (ev, entry)}>
                <Icon ic={entry.type == Ase.ResourceType.FOLDER ? "md-folder" : "fa-file_o"}/>
                {entry.label}
              </button>
            )}
          </For>
          <div class="-spin-wrapper hflex"
            style="height: 100%; width: 100%; text-align: center; align-items: center; justify-content: center">
            <div style="text-align: center" > ⥁ </div>
          </div>
        </div>

        <input class="-pathentry mt-4 outline outline-2 outline-offset-2"
          ref={pathentryRef}
          value=""
          type="text"
          onKeyDown={pathentry_keydown}
          onSelect={Util.prevent_event} />

        <div class="dialog-footer">
          <button class="button-xl" onClick={() => select_entry (null)} onKeyDown={Kbd.keydown_move_focus_up}
            disabled={update_inflight() ? true : undefined}>
            {props.button || 'Select'}
          </button>
          <button class="button-xl" onClick={() => dialogRef.close()} onKeyDown={Kbd.keydown_move_focus_up}>
            Close
          </button>
        </div>
      </dialog>
    </div>
  );
}
