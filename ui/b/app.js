// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** == B-APP ==
 * Global application instance for Anklang.
 * *zmovehooks*
 * : An array of callbacks to be notified on pointer moves.
 * *zmove()*
 * : Trigger the callback list `zmovehooks`. This is useful to get debounced
 * notifications for pointer movements, including 0-distance moves after significant UI changes.
 */

import { render } from 'solid-js/web';
import { ShellTemplate } from './shell';

const component_modules = import.meta.glob (['../b/*.js', '../b/*.jsx'], { eager: true });
// Object.entries (component_modules).map (([path, mod]) => console.log ("IMPORT:", path, !!mod.default));

import * as Util from '../util.js';
import * as Mouse from '../mouse.js';
import { hex, basename, dirname, displayfs, displaybasename, displaydirname } from '../strings.js';
import { Signal, createSignal, State, Computed, Watcher, tracking_wrapper } from "../signal.js";

/// Create a new reactive proxy with Solid.js signals from the fields in `tmpl`
function make_reactive (tmpl)
{
  const signals = {};
  for (const key in tmpl) {
    const options = {}, value = tmpl[key];
    if (Array.isArray (value))
      options.equals = false; // always re-render on Array reassignment
    Object.defineProperty (signals, key, { value: createSignal (value, options), enumerable: true, configurable: false, writable: true });
  }
  const handler = {
    get (target, prop, receiver) {
      const gs = Reflect.get (target, prop); // receiver
      return Array.isArray (gs) ? gs[0]() : undefined;
    },
    set (target, prop, value, receiver) {
      const gs = Reflect.get (target, prop); // receiver
      Array.isArray (gs) && gs[1] (() => value);
      return true; // success
    }
  };
  return new Proxy (signals, handler);
}
window.make_reactive = make_reactive;

// == App ==
export class AppClass {
  panel2_types = [ 'd' /*devices*/, 'p' /*pianoroll*/ ];
  panel3_types = [ 'i' /*info*/, 'b' /*browser*/ ];
  constructor ()
  {
    // super();
    { // mimick familiar LitComponent API
      let update_queued = false;
      this.request_update = () => {
	if (update_queued) return;
	update_queued = true;
	queueMicrotask (() => {
	  update_queued = false;
	  this.updated ({});
	});
      };
      this.updated = tracking_wrapper (this.request_update, this.updated.bind (this));
    }
    Object.defineProperty (globalThis, 'App', { value: this });
    let data = {
      project: null,
      mtrack: null, // master track
      panel3: 'i',
      panel2: 'p',
      piano_roll_source: undefined,
      current_track: undefined,
      show_preferences_dialog: false,
    };
    Object.defineProperty (globalThis, 'Data', { value: make_reactive (data) });
    this.request_update();
  }
  get project ()  { return Data.project; }
  set project (p) { Data.project = p; }
  get current_track () { return Data.current_track; }
  set current_track (t)
  {
    if (Data.current_track === t) return;
    Data.current_track = t;
    if (this.shell)
      for (const tv of this.shell.querySelectorAll ('b-trackview')) // TODO: remove explicit notifies
	tv.notify_current_track(); // see trackview.js
  }
  updated (changed_props)
  {
    const name = this.project?.name;
    document.title = Util.format_title ('Anklang', name);
  }
  mount (id)
  {
    console.assert (!globalThis.Shell);
    const shell_parent = document.getElementById (id);
    shell_parent.innerHTML = '';
    const solid_render_dispose = render (() => ShellTemplate ({}), shell_parent);
    console.assert (globalThis.Shell);
  }
  shell_unmounted() {
  }
  switch_panel3 (n) {
    const a = this.panel3_types;
    if ('string' == typeof n)
      Data.panel3 = n;
    else
      Data.panel3 = a[(a.indexOf (Data.panel3) + 1) % a.length];
  }
  switch_panel2 (n) {
    const a = this.panel2_types;
    if ('string' == typeof n)
      Data.panel2 = n;
    else
      Data.panel2 = a[(a.indexOf (Data.panel2) + 1) % a.length];
  }
  open_piano_roll (midi_source) {
    Data.piano_roll_source = midi_source;
    if (Data.piano_roll_source)
      this.switch_panel2 ('p');
  }
  async load_project_checked (project_or_path) {
    const err = await this.load_project (project_or_path);
    if (err !== Ase.Error.NONE) {
      let errblurb = Ase.server.error_blurb (err);
      let msg = '# File IO Error\n  \n  \n';
      msg += 'Failed to load project:\n\n';
      msg += '`' + displayfs (project_or_path) + ": " + await errblurb + '`';
      Shell.show_notice (msg);
    }
    return err;
  }
  async load_project (project_or_path) {
    // always replace the existing project with a new one
    let newproject = project_or_path instanceof Ase.Project ? project_or_path : null;
    if (!newproject)
      {
	// Create afresh
	newproject = await Ase.server.create_project ('Untitled');
	// Loads from disk
	if (project_or_path)
	  {
	    const error = await newproject.load_project (project_or_path);
	    if (error != Ase.Error.NONE)
	      return error;
	    newproject.name = displaybasename (project_or_path);
	  }
      }
    const mtrack = await newproject.master_track();
    const tracks = await newproject.all_tracks();
    // shut down old project
    let need_reload = false;
    if (App.project)
      {
	App.project.stop_playback();
	App.project = null; // TODO: should trigger FinalizationGroup
	// TODO: App.open_piano_roll (undefined);
	need_reload = true;
      }
    // replace project & master track without await, to synchronously trigger updates for both
    App.project = newproject; // assigns Data.project
    Data.mtrack = mtrack;
    App.current_track = tracks[0];
    const clips = await App.current_track.launcher_clips();
    App.open_piano_roll (clips.length ? clips[0] : null);
    if (this.shell)
      this.shell.update();
    if (need_reload) {
      window.location.reload();
      // TODO: only reload the UI partially when the project changes, this requires a full port to LitElements
    }
    return Ase.Error.NONE;
  }
  async save_project (projectpath, collect = true) {
    Shell.show_spinner();
    let error = !Data.project ? Ase.Error.INTERNAL :
		  Data.project.save_project (projectpath, collect);
    error = await error;
    // await new Promise (r => setTimeout (r, 3 * 1000)); // artificial wait to test spinner
    Shell.hide_spinner();
    return error;
  }
  status (...args) {
    console.log (...args);
  }
  async_modal_dialog (dialog_setup) {
    return this.shell.async_modal_dialog (dialog_setup);
  }
  async_button_dialog (title, text, buttons = [], emblem) {
    const dialog_setup = {
      title,
      text,
      buttons,
      emblem,
    };
    return Shell.async_modal_dialog (dialog_setup);
  }
  zmoves_add = Mouse.zmove_add;
  zmove = Mouse.zmove_trigger;
  zmove_last = Mouse.zmove_last;
}

// == addvc ==
export async function create_app() {
  if (globalThis.App)
    return globalThis.App;
  // common globals
  const global_properties = {
    CONFIG: globalThis.CONFIG,
    debug: globalThis.debug,
    Util: globalThis.Util,
    Ase: globalThis.Ase,
    window: globalThis.window,
    document: globalThis.document,
  };
  // create main App instance
  const app = new AppClass ();
  console.assert (app === globalThis.App);
  return globalThis.App;
}
