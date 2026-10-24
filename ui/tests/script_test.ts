// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import * as Script from '../script.js';
import * as PianoCtrl from '../b/piano-ctrl.js';

export async function test_script_note_units (): Promise<boolean>
{
  const clip = await App.current_track.create_midi_clip ('Script Notes', 0, 4);
  const previous_clip = Shell.r.piano_roll_source;
  const script_url = URL.createObjectURL (new Blob ([`
    let clip;
    await host.register ('Tests', 'Script Notes', async ({ insert, removed }) => {
      clip ||= await host.piano_roll_clip();
      if (removed)
        return clip.insert_note (64, 0.25, 0.5);
      if (insert)
        return [await clip.insert_note (60, 0.125, 0.375), await clip.insert_note (62, 1.5, 0.25, 0, 7, true)];
      return clip.list_selected_notes();
    }, '');
  `], { type: 'text/javascript' }));
  const NativeWorker = globalThis.Worker;
  const host_url = new URL ('../host.js', import.meta.url);
  let script_host;
  try {
    globalThis.Worker = class extends NativeWorker {
      constructor (_url: string | URL, options?: WorkerOptions) { super (host_url, options); }
    };
    script_host = Script.load_script (script_url);
  } finally {
    globalThis.Worker = NativeWorker;
  }
  const registered = new Promise<string> (resolve => {
    script_host._register = (_category, _label, funid) => resolve (funid);
  });
  const script_error = new Promise<never> ((_resolve, reject) => {
    script_host.worker.addEventListener ('error', event => reject (new Error (event.message)));
    script_host.worker.onmessage = event => { script_host.onmessage (event).catch (reject); };
  });
  let acquired = false;
  let removed = false;
  try {
    Shell.r.piano_roll_source = clip;
    const run = async () => {
      const funid = await registered;
      const ids = await script_host.call (funid, { insert: true });
      acquired = true;
      const notes = await clip.list_all_notes();
      const first = notes.find (note => note.id === ids[0]);
      const second = notes.find (note => note.id === ids[1]);
      if (notes.length !== 2 || ids[0] <= 0 || ids[1] <= 0 || ids[0] === ids[1] ||
          first?.note_number !== 60 || first.start_beat !== 0.125 || first.length_beats !== 0.375 ||
          first.velocity !== 127 || first.colour !== 0 || first.mute ||
          second?.note_number !== 62 || second.start_beat !== 1.5 || second.length_beats !== 0.25 ||
          second.velocity !== 0 || second.colour !== 7 || !second.mute)
        throw new Error ('script insertion lost note handles, beat units or Tracktion note values');
      if ((await script_host.call (funid, {})).length)
        throw new Error ('script insertion selected notes');
      const controller = new PianoCtrl.PianoCtrl ({ clip });
      const select = shiftKey => controller.keydown ({
        keyCode: 65, ctrlKey: true, shiftKey, altKey: false, preventDefault() {}, stopPropagation() {},
      });
      await select (false);
      const selected = await script_host.call (funid, {});
      if (selected.length !== 2 || selected.some (note => !note.selected || !ids.includes (note.id)))
        throw new Error ('script did not read the UI note selection');
      await select (true);
      if ((await script_host.call (funid, {})).length)
        throw new Error ('script kept a stale note selection');
      await clip.remove_self();
      removed = true;
      if (await script_host.call (funid, { removed: true }) !== 0)
        throw new Error ('script insertion into a removed clip did not return handle 0');
    };
    await Promise.race ([run(), script_error]);
  } finally {
    script_host.terminate();
    if (acquired)
      script_host._release_refhandle (clip.$id);
    Shell.r.piano_roll_source = previous_clip;
    URL.revokeObjectURL (script_url);
    if (!removed)
      await clip.remove_self();
  }
  return true;
}
