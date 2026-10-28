// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { run_tests } from './runner.js';
import * as Dom from '../dom';

// == Test registry ==
const sub_tests: [string, () => Promise<any>][] = [];

/// Replace `object[name]` with `value`, the returned function puts back exactly what was there.
function stub (object: any, name: string, value: any): () => void
{
  const own = Object.hasOwn (object, name), original = object[name];
  object[name] = value;
  return () => { if (own) object[name] = original; else delete object[name]; };
}

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
    let saved_project, saved_name, resolve_name;
    const name_fields = document.createElement ('div');
    const restore_modal_dialog = stub (shell, 'async_modal_dialog', setup => {
      setup.div_handler (name_fields);
      return new Promise (resolve => { resolve_name = resolve; });
    });
    Ase.Jsonipc.send = function (method, params) {
      if (method === 'save_project') {
        saved_project = params[0];
        saved_name = params[1];
        return Promise.resolve (Ase.Error.NONE);
      }
      return send.call (this, method, params);
    };
    try {
      await Dom.ui_next_frame();
      document.querySelector<HTMLButtonElement> ('#g-filemenu button[uri="save"]')!.click();
      for (let frame = 0; frame < 120 && !resolve_name; frame++)
        await Dom.ui_next_frame();
      if (!resolve_name)
        throw new Error ('Save did not ask for a project name');
      name_fields.querySelector<HTMLInputElement> ('input')!.value = 'Saved activation test';
      await app.load_project (old_project);
      resolve_name (1);
      for (let frame = 0; frame < 120 && !saved_project; frame++)
        await Dom.ui_next_frame();
      if (saved_project !== project || saved_name !== 'Saved activation test')
        throw new Error ('Save lost the project or name while its name dialog was open');
    } finally {
      resolve_name?. (-1);
      restore_modal_dialog();
      Ase.Jsonipc.send = send;
    }
  } finally {
    await app.load_project (old_project);
    await project.discard();
  }

  return true;
}
sub_tests.push (['piano_roll', test_activation_piano_roll]);

async function test_open_project (): Promise<boolean>
{
  const app = window.App, old_project = Shell.project;
  const project = await Ase.server.create_project ('Stored project name');
  const send = Ase.Jsonipc.send;
  let error = Ase.Error.NONE, opened_path;
  Ase.Jsonipc.send = function (method, params) {
    if (method === 'open_project') {
      opened_path = params[1];
      return Promise.resolve (error);
    }
    if (method === 'last_project')
      return Promise.resolve (project);
    return send.call (this, method, params);
  };
  try {
    if (await app.load_project ('/projects/different-dir/project.anklang') !== Ase.Error.NONE)
      throw new Error ('Open failed');
    if (Shell.project !== project || opened_path !== '/projects/different-dir/project.anklang')
      throw new Error ('Open did not activate Server.last_project');
    if (await project.$refetch (() => project.name) !== 'Stored project name')
      throw new Error ('Open replaced the saved project name');
    error = Ase.Error.BAD_PROJECT;
    if (await app.load_project ('/projects/bad') !== error || Shell.project !== project)
      throw new Error ('A failed Open changed the current project');
  } finally {
    Ase.Jsonipc.send = send;
    await app.load_project (old_project);
    await project.discard();
  }
  return true;
}
sub_tests.push (['open_project', test_open_project]);

async function test_save_project_name (): Promise<boolean>
{
  const app = window.App, old_project = Shell.project;
  const project = await Ase.server.create_project ('Untitled');
  const send = Ase.Jsonipc.send;
  let saved_name, saves = 0;
  Ase.Jsonipc.send = function (method, params) {
    if (method === 'save_project') {
      if (params[0] !== project || params.length !== 2)
        throw new Error ('Save sent a filename or collection argument');
      saved_name = params[1];
      saves++;
      return Promise.resolve (Ase.Error.NONE);
    }
    return send.call (this, method, params);
  };
  try {
    await app.load_project (project);
    for (const accept of [false, true]) {
      document.querySelector<HTMLButtonElement> ('#g-filemenu button[uri="save"]')!.click();
      let dialog: HTMLDialogElement | null = null;
      for (let frame = 0; frame < 120 && !dialog; frame++) {
        await Dom.ui_next_frame();
        dialog = document.querySelector<HTMLDialogElement> ('dialog.b-modals[open]');
      }
      if (!dialog)
        throw new Error ('An unnamed project needs a name dialog');
      dialog.querySelector<HTMLInputElement> ('input')!.value = 'First save';
      dialog.querySelectorAll<HTMLButtonElement> ('footer button')[accept ? 1 : 0].click();
      await Dom.ui_next_frame();
      await Dom.ui_next_frame();
      if (!accept && saves)
        throw new Error ('Cancelling the name dialog saved the project');
      if (accept) {
        for (let frame = 0; frame < 120 && !saves; frame++)
          await Dom.ui_next_frame();
        if (saves !== 1 || saved_name !== 'First save')
          throw new Error ('Save did not pass the project name');
      }
    }
  } finally {
    Ase.Jsonipc.send = send;
    document.querySelector<HTMLDialogElement> ('dialog.b-modals[open]')?.close();
    await app.load_project (old_project);
    await project.discard();
  }
  return true;
}
sub_tests.push (['save_project_name', test_save_project_name]);

async function test_save_project_as (): Promise<boolean>
{
  const app = window.App, shell = window.Shell, old_project = shell.project;
  const project = await Ase.server.create_project ('Save As test');
  const send = Ase.Jsonipc.send;
  let saved, notice = '';
  Ase.Jsonipc.send = function (method, params) {
    if (method === 'save_project_as') {
      saved = params;
      return Promise.resolve (Ase.Error.NONE);
    }
    return send.call (this, method, params);
  };
  const restore = [ stub (shell, 'select_file', async () => '/projects/Copy'),
                    stub (shell, 'show_notice', message => { notice = message; }) ];
  try {
    await app.load_project (project);
    document.querySelector<HTMLButtonElement> ('#g-filemenu button[uri="saveas"]')!.click();
    for (let frame = 0; frame < 120 && !notice; frame++)
      await Dom.ui_next_frame();
    if (saved?.[0] !== project || saved?.[1] !== '/projects/Copy' || !notice.includes ('Project Saved'))
      throw new Error ('Save As did not save the project into the selected dir');
  } finally {
    Ase.Jsonipc.send = send;
    restore.forEach (undo => undo());
    await app.load_project (old_project);
    await project.discard();
  }
  return true;
}
sub_tests.push (['save_project_as', test_save_project_as]);

async function test_file_menu_replaces_project (): Promise<boolean>
{
  const app = window.App, shell = window.Shell, old_project = shell.project;
  const source = await Ase.server.create_project ('Replaced project');
  const opened = await Ase.server.create_project ('Opened project');
  const send = Ase.Jsonipc.send;
  const discarded = new Set();
  let replacement;
  Ase.Jsonipc.send = function (method, params) {
    if (method === 'open_project')
      return Promise.resolve (Ase.Error.NONE);
    if (method === 'last_project')
      return Promise.resolve (opened);
    if (method === 'discard')
      discarded.add (params[0]);
    return send.call (this, method, params);
  };
  const restore_select_file = stub (shell, 'select_file', async () => '/projects/replaced/project.anklang');
  try {
    await app.load_project (source);
    document.querySelector<HTMLButtonElement> ('#g-filemenu button[uri="loadnew"]')!.click();
    for (let frame = 0; frame < 120 && !discarded.has (source); frame++)
      await Dom.ui_next_frame();
    if (shell.project === source || !discarded.has (source))
      throw new Error ('New kept the replaced project open');
    replacement = shell.project;
    if (await source.save_project ('Replaced') !== Ase.Error.INTERNAL)
      throw new Error ('The replaced project can still save');
    document.querySelector<HTMLButtonElement> ('#g-filemenu button[uri="load"]')!.click();
    for (let frame = 0; frame < 120 && !discarded.has (replacement); frame++)
      await Dom.ui_next_frame();
    if (shell.project !== opened || !discarded.has (replacement))
      throw new Error ('Open kept the replaced project open');
  } finally {
    restore_select_file();
    Ase.Jsonipc.send = send;
    await app.load_project (old_project);
    await source.discard();
    await opened.discard();
    await replacement?.discard();
  }
  return true;
}
sub_tests.push (['file_menu_replaces_project', test_file_menu_replaces_project]);

async function test_quit_project (): Promise<boolean>
{
  const app = window.App, shell = window.Shell, old_project = shell.project, send = Ase.Jsonipc.send;
  const projects = [];
  try {
    for (const scenario of [
      { unnamed: true, choice: 0, error: Ase.Error.NONE },
      { unnamed: false, choice: 0, error: Ase.Error.NONE },
      { unnamed: true, choice: 0, error: Ase.Error.FILE_WRITE_FAILED },
      { unnamed: true, choice: 1, error: Ase.Error.NONE },
      { unnamed: true, choice: 2, error: Ase.Error.NONE },
      { unnamed: true, choice: 2, error: Ase.Error.FILE_WRITE_FAILED },
    ]) {
      const project = await Ase.server.create_project ('Quit test');
      projects.push (project);
      let closes = 0, removals = 0, saves = 0, prompted = false, notice = '', error_dialogs = 0, resolve_removal;
      const restore = [
        stub (app, 'close_window', () => { closes++; }),
        stub (app, 'async_button_dialog', async (title, text, buttons) => {
          if (buttons.length === 1) {
            error_dialogs++;
            if (!text.includes ('Failed to save project:') || buttons[0] !== 'Close')
              throw new Error ('The save error dialog needs an error message and a Close button');
            return 0;
          }
          prompted = true;
          return scenario.choice;
        }),
        stub (shell, 'show_notice', message => { notice = message; }),
        stub (shell, 'async_modal_dialog', async setup => {
          const div = document.createElement ('div');
          setup.div_handler (div);
          div.querySelector<HTMLInputElement> ('input')!.value = 'Quit saved';
          return 1;
        }),
      ];
      Ase.Jsonipc.send = function (method, params) {
        if (params[0] === project) {
          if (method === 'get/unnamed')
            return Promise.resolve (scenario.unnamed);
          if (method === 'remove_project_dir') {
            removals++;
            return new Promise (resolve => { resolve_removal = resolve; });
          }
          if (method === 'save_project') {
            saves++;
            return Promise.resolve (scenario.error);
          }
        }
        return send.call (this, method, params);
      };
      try {
        await app.load_project (project);
        await Dom.ui_next_frame();
        document.querySelector<HTMLButtonElement> ('#g-filemenu button[uri="quit"]')!.click();
        for (let frame = 0; frame < 120 && !prompted; frame++)
          await Dom.ui_next_frame();
        if (!prompted)
          throw new Error ('Quit did not ask whether to save');
        const removing = scenario.unnamed && scenario.choice === 0;
        if (removing) {
          for (let frame = 0; frame < 120 && !resolve_removal; frame++)
            await Dom.ui_next_frame();
          if (!resolve_removal || closes)
            throw new Error ('Quit did not wait for directory removal');
          resolve_removal (scenario.error);
        }
        if (scenario.choice !== 1)
          for (let frame = 0; frame < 120 && !closes && !notice && !error_dialogs; frame++)
            await Dom.ui_next_frame();
        await Dom.ui_next_frame();
        const save_failed = scenario.choice === 2 && scenario.error !== Ase.Error.NONE;
        const closing = scenario.choice === 0 || (scenario.choice === 2 && !save_failed);
        if (removals !== Number (removing) || closes !== Number (closing) || saves !== Number (scenario.choice === 2))
          throw new Error ('Quit removed, saved or closed the wrong project');
        if (error_dialogs !== Number (save_failed))
          throw new Error ('Quit must show an error dialog only when saving fails');
      } finally {
        if (resolve_removal) {
          resolve_removal (Ase.Error.NONE);
          for (let frame = 0; frame < 120 && !closes; frame++)
            await Dom.ui_next_frame();
        }
        Ase.Jsonipc.send = send;
        restore.forEach (undo => undo());
      }
    }
  } finally {
    await app.load_project (old_project);
    for (const project of projects)
      await project.discard();
  }
  return true;
}
sub_tests.push (['quit_project', test_quit_project]);

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
