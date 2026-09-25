// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class B-AboutDialog
 * SolidJS component that displays version information about Anklang.
 *
 * ### Props:
 * *onClose*
 * : Callback invoked when the Close button is activated.
 */

import { createResource, onMount, onCleanup, For, Show } from 'solid-js';
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
  // Load all content before mounting the dialog so it opens at its final size.
  const [pairs] = createResource (about_pairs);
  return <Show when={pairs()}>{info => <AboutContent pairs={info()} onClose={props.onClose} />}</Show>;
}

function AboutContent (props)
{
  /** @type {HTMLDialogElement} */
  let dialogRef;
  onMount (() => Dom.show_modal (dialogRef));
  onCleanup (() => dialogRef.close());

  return (
    <dialog
      class="b-about-dialog floating-dialog"
      ref={dialogRef}
      onClose={props.onClose}
      exclusive={true}
      bwidth="9em"
      style="z-index: 93">
      <div class="dialog-header">
        About ANKLANG
      </div>
      <div class="grid max-w-full">
        <For each={props.pairs}>
          {pair => (
            <>
              <span class="col-start-1 min-w-[15em] pr-2 text-right align-top font-bold">{pair[0]}</span>
              <span class="col-start-2 whitespace-pre-wrap break-words">{pair[1]}</span>
            </>
          )}
        </For>
      </div>
      <div class="dialog-footer">
        <button class="button-xl" autofocus onClick={() => dialogRef.close()}>Close</button>
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
