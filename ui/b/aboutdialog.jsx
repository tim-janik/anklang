// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class B-AboutDialog
 * SolidJS component that displays version information about Anklang.
 *
 * ### Props:
 * *onClose*
 * : Callback invoked when the Close button is activated.
 */

import { createSignal, onMount, onCleanup, For } from 'solid-js';
import * as Util from "../util.js";
import * as Dom from "../dom.js";

// == STYLE ==
Extra_css`
dialog.b-about-dialog {
  margin: auto;
}
`;

// == Component ==
export function AboutDialog (props)
{
  const [info_pairs, set_info_pairs] = createSignal ([]);
  let dialogRef;
  let cancelled = false; // guard against showModal() after unmount (solid-dialog pattern)

  onMount (async () => {
    const pairs = await about_pairs ();
    if (cancelled) return; // component unmounted during async load — skip open
    set_info_pairs (pairs);
    if (dialogRef)
      Dom.show_modal (dialogRef, () => props.onClose?.());
  });

  // onCleanup runs synchronously before DOM removal (SolidJS lifecycle guarantee)
  // ensures dialogRef.close() fires native 'close' event before element is yanked
  onCleanup (() => {
    cancelled = true;   // prevent late onMount from calling showModal()
    dialogRef?.close(); // close native dialog so 'close' event fires for listener cleanup
    props.onClose?.();  // sync parent state (controlled-prop pattern, like solid-modal/solid-dialog)
  });

  const handleClose = () => {
    props.onClose?.();
  };

  return (
    <dialog
      class="b-about-dialog floating-dialog"
      ref={dialogRef}
      onClose={handleClose}
      exclusive={true}
      bwidth="9em"
      style="z-index: 93">
      <div class="dialog-header">
        About ANKLANG
      </div>
      <div class="grid max-w-full">
        <For each={info_pairs()}>
          {pair => (
            <>
              <span class="col-start-1 min-w-[15em] pr-2 text-right align-top font-bold">{pair[0]}</span>
              <span class="col-start-2 whitespace-pre-wrap break-words">{pair[1]}</span>
            </>
          )}
        </For>
      </div>
      <div class="dialog-footer">
        <button class="button-xl" autofocus onClick={() => props.onClose?.()}>Close</button>
      </div>
    </dialog>
  );
};

// == Data loading (unchanged) ==
async function about_pairs()
{
  const user_agent = navigator.userAgent.replace (/([)0-9]) ([A-Z])/gi, '$1\n$2');
  let array = [
    [ 'Anklang:',		CONFIG.version + ' (' + CONFIG.revdate.split (' ')[0] + ')' ],
    [ 'SoundEngine:',		await Ase.server.get_build_id() ],
    [ 'FLAC:',		        await Ase.server.get_flac_version() ],
    [ 'Opus:',		        await Ase.server.get_opus_version() ],
    [ 'Sndfile:',	        await Ase.server.get_sndfile_version() ],
    [ 'Lit:',			CONFIG.lit_version ],
    [ 'User Agent:',		user_agent ],
  ];
  const Electron = window['Electron'];
  if (Electron)
  {
    const operating_system = Electron.platform + ' ' + Electron.arch + ' (' + Electron.os_release + ')';
    const parray = [
      [ 'Electron:',          Electron.versions.electron ],
      [ 'Chrome:',            Electron.versions.chrome ],
      [ 'Node.js:',           Electron.versions.node ],
      [ 'Libuv:',             Electron.versions.uv ],
      [ 'V8:',                Electron.versions.v8 ],
      [ 'OS:',		operating_system ],
    ];
    array = Array.prototype.concat (array, parray);
  }
  return array;
}
