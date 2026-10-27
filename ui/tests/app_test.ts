// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { run_tests } from './runner.js';
import * as Dom from '../dom';

// == Test registry ==
const sub_tests: [string, () => Promise<any>][] = [];

/// The window title includes the project name (M7).
async function test_activation_title (): Promise<boolean>
{
  const app: any = (window as any).App;
  const name = app?.project?.name;
  if (!name)
    throw new Error ('App.project has no name in test environment');

  if (!document.title.includes (name))
    throw new Error (`document.title does not contain project name: "${document.title}"`);
  if (!document.title.includes ('Anklang'))
    throw new Error (`document.title does not contain app name: "${document.title}"`);

  return true;
}
sub_tests.push (['title', test_activation_title]);

/// The initial current_track is editable, not the master output (M7).
async function test_activation_current_track (): Promise<boolean>
{
  const app: any = (window as any).App;
  const shell: any = (window as any).Shell;
  const track = app?.current_track;
  if (!track)
    throw new Error ('App.current_track is not set after boot');
  if (await track.is_control_track())
    throw new Error ('App.current_track is a control track');
  if (shell?.r?.current_track !== track)
    throw new Error ('Shell.r.current_track disagrees with App.current_track');

  return true;
}
sub_tests.push (['current_track', test_activation_current_track]);

/// Activation opens the piano roll for the first clip (M7).
async function test_activation_piano_roll (): Promise<boolean>
{
  const app: any = (window as any).App;
  const shell: any = (window as any).Shell;
  const old_project = app?.project;
  if (!old_project)
    throw new Error ('App.project not set in test environment');

  const project = await Ase.server.create_project ('AppActivationTest');
  if (!project)
    throw new Error ('create_project failed');
  try {
    const tracks = await project.$refetch (() => project.all_tracks);
    let track: any = null;
    for (const candidate of tracks)
      if (!await candidate.is_control_track()) {
        track = candidate;
        break;
      }
    if (!track)
      throw new Error ('project has no editable track');

    const clip = await track.create_midi_clip ('activation-test', 0, 4);
    if (!clip)
      throw new Error ('create_midi_clip failed');
    const track_name = await track.$refetch (() => track.name);
    const clip_name = await clip.$refetch (() => clip.name);

    await app.load_project (project);

    const current_track_name = await shell.r.current_track.$refetch (() => shell.r.current_track.name);
    if (current_track_name !== track_name)
      throw new Error (`activation selected ${current_track_name}, expected ${track_name}`);
    const current_clip_name = await shell.r.piano_roll_source.$refetch (() => shell.r.piano_roll_source.name);
    if (current_clip_name !== clip_name)
      throw new Error ('activation did not open the first clip in the piano roll');
    const send = Ase.Jsonipc.send;
    let saved_project;
    Ase.Jsonipc.send = function (method, params) {
      if (method === 'save_project') {
        saved_project = params[0];
        return Promise.resolve (Ase.Error.NONE);
      }
      return send.call (this, method, params);
    };
    try {
      await Dom.ui_next_frame();
      document.querySelector<HTMLButtonElement> ('#g-filemenu button[uri="saveas"]')!.click();
      for (let frame = 0; frame < 120 && !shell.r.fs_shown; frame++)
        await Dom.ui_next_frame();
      if (!shell.r.fs_shown)
        throw new Error ('Save As did not open its file selector');
      await app.load_project (old_project);
      shell.fs.resolve ('/tmp/anklang-ui-save-test.anklang');
      for (let frame = 0; frame < 120 && !saved_project; frame++)
        await Dom.ui_next_frame();
      if (saved_project !== project)
        throw new Error ('Save As used the project activated while its file selector was open');
    } finally {
      if (shell.r.fs_shown) shell.fs.resolve();
      Ase.Jsonipc.send = send;
    }
  } finally {
    await app.load_project (old_project);
    await project.discard();
  }

  return true;
}
sub_tests.push (['piano_roll', test_activation_piano_roll]);

async function test_overlapping_activation (): Promise<boolean>
{
  const app = window.App;
  const old_project = app.project;
  const first = await Ase.server.create_project ('EarlierActivation');
  const second = await Ase.server.create_project ('LaterActivation');
  try {
    const first_tracks = await first.$refetch (() => first.all_tracks);
    const editable = [];
    for (const track of first_tracks)
      if (!await track.is_control_track()) editable.push (track);
    const second_tracks = await second.$refetch (() => second.all_tracks);
    let second_track;
    for (const track of second_tracks)
      if (!await track.is_control_track()) { second_track = track; break; }
    if (!editable.length || !second_track)
      throw new Error ('activation projects have no editable tracks');
    await editable[0].create_midi_clip ('earlier-clip', 0, 4);
    const second_clip = await second_track.create_midi_clip ('later-clip', 0, 4);
    for (const delayed_method of ['all_tracks', 'is_control_track', '$refetch']) {
      let release!: () => void;
      let entered!: () => void;
      const gate = new Promise<void> (resolve => { release = resolve; });
      const waiting = new Promise<void> (resolve => { entered = resolve; });
      const delayed_track = new Proxy (editable[0], {
        get (target, key) {
          if (key === delayed_method)
            return async (...args) => {
              entered();
              await gate;
              return target[key] (...args);
            };
          return Reflect.get (target, key);
        },
      });
      const delayed_project = new Proxy (first, {
        get (target, key) {
          if (key === '$refetch' && delayed_method === 'all_tracks')
            return async cb => {
              entered();
              await gate;
              return target.$refetch (cb);
            };
          if (key === 'all_tracks')
            return [delayed_track];
          return Reflect.get (target, key);
        },
      });
      const pending = app.assign_project (delayed_project, 'b-app');
      try {
        await waiting;
        await app.assign_project (second, 'b-app');
      } finally {
        release();
        await pending;
      }
      if (app.project !== second || app.current_track !== second_track)
        throw new Error (`stale ${delayed_method} replaced the active project or track`);
      if (window.Shell.r.piano_roll_source !== second_clip)
        throw new Error (`stale ${delayed_method} replaced the active clip`);
    }
  } finally {
    await app.assign_project (old_project, 'b-app');
    await first.discard();
    await second.discard();
  }
  return true;
}
sub_tests.push (['overlapping_activation', test_overlapping_activation]);

export async function test_app (): Promise<boolean>
{
  return run_tests ('app', sub_tests);
}
