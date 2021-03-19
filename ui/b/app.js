// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

// Global application instance for Anklang.
// *zmovehooks*
// : An array of callbacks to be notified on pointer moves.
// *zmove (event)*
// : Trigger the callback list `zmovehooks`, passing `event` along. This is useful to get debounced
// notifications for pointer movements, including 0-distance moves after significant UI changes.

import * as Util from '../util.js';
import DataBubbleIface from '../b/databubble.js';

// == zmove() ==
class ZMove {
  static
  zmove (ev) {
    if (ev && ev.screenX && ev.screenY &&
	ev.timeStamp > (ZMove.last_event?.timeStamp || 0))
      ZMove.last_event = ev;
    ZMove.trigger();
  }
  static
  trigger_hooks() {
    if (ZMove.last_event)
      for (const hook of ZMove.zmovehooks)
        hook (ZMove.last_event);
  }
  static trigger = Util.debounce (ZMove.trigger_hooks);
  static zmovehooks = [];
  static zmoves_add (cb) {
    ZMove.zmovehooks.push (cb);
    return _ => {
      Util.array_remove (ZMove.zmovehooks, cb);
    };
  }
}

// == App ==
export class AppClass {
  data_bubble = null;
  panel2_types = [ 'd' /*devices*/, 'p' /*pianoroll*/ ];
  panel3_types = [ 'i' /*info*/, 'b' /*browser*/ ];
  constructor (vue_app) {
    // super();
    this.vue_app = vue_app;
    this.data_bubble = new DataBubbleIface();
    const data = {
      project: null,
      mtrack: null, // master track
      panel3: 'i',
      panel2: 'p',
      piano_roll_source: undefined,
      current_track: undefined,
      show_about_dialog: false,
      show_preferences_dialog: false,
    };
    this.data = Vue.reactive (data);
    // watch: {
    // show_about_dialog:       function (newval) { if (newval && this.show_preferences_dialog) this.show_preferences_dialog = false; },
    // show_preferences_dialog: function (newval) { if (newval && this.show_about_dialog) this.show_about_dialog = false; },
    // }
    Object.defineProperty (globalThis, 'App', { value: this });
    Object.defineProperty (globalThis, 'Data', { value: this.data });
    this.vue_app.config.globalProperties.App = App;   // global App, export methods
    this.vue_app.config.globalProperties.Data = Data; // global Data, reactive
  }
  mount (id) {
    this.shell = this.vue_app.mount (id);
    if (!this.shell)
      throw Error (`failed to mount App at: ${id}`);
  }
  shell_unmounted() {
    this.notifynameclear?.();
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
  async load_project (project_or_path) {
    project_or_path = await project_or_path;
    // always replace the existing project with a new one
    let newproject = project_or_path instanceof Ase.Project ? project_or_path : null;
    if (!newproject)
      {
	newproject = await Ase.server.create_project ('Untitled');
	// load from disk if possible
	if (project_or_path)
	  {
	    const ret = await newproject.restore_from_file (project_or_path);
	    if (ret != Ase.Error.NONE)
	      {
		await App.async_modal_dialog ("File Open Error",
					      "Failed to open project.\n" +
					      project_or_path + ": " +
					      await Ase.server.describe_error (ret),
					      [ 'Dismiss', 0 ],
					      'ERROR');
		return ret;
	      }
	    const basename = project_or_path.replace (/.*\//, '');
	    await newproject.set_name (basename);
	  }
      }
    const mtrack = await newproject.master_track();
    const tracks = await newproject.list_tracks();
    // shut down old project
    if (Data.project)
      {
	this.notifynameclear();
	App.open_piano_roll (undefined);
	Data.project.stop();
	Data.project = null; // TODO: should trigger FinalizationGroup
      }
    // replace project & master track without await, to synchronously trigger Vue updates for both
    Data.project = newproject;
    Data.mtrack = mtrack;
    Data.current_track = tracks[0];
    const update_title = async () => {
      const name = Data.project ? await Data.project.name() : undefined;
      document.title = Util.format_title ('Anklang', name);
    };
    this.notifynameclear = Data.project.on ("notify:name", update_title);
    update_title();
    this.shell.update();
    return Ase.Error.NONE;
  }
  async save_project (projectpath) {
    const self_contained = true;
    return !Data.project ? Ase.Error.INTERNAL :
	   Data.project.store (projectpath, self_contained);
  }
  status (...args) {
    console.log (...args);
  }
  async_modal_dialog() {
    return this.shell.async_modal_dialog.apply (this.shell, arguments);
  }
  zmoves_add = ZMove.zmoves_add;
  zmove = ZMove.zmove;
}
