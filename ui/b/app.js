// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

/** # B-APP
 * Global application instance for Anklang.
 * *zmovehooks*
 * : An array of callbacks to be notified on pointer moves.
 * *zmove (event)*
 * : Trigger the callback list `zmovehooks`, passing `event` along. This is useful to get debounced
 * notifications for pointer movements, including 0-distance moves after significant UI changes.
 */

import VueComponents from '../all-components.js';
import ShellClass from '../b/shell.js';
import * as Util from '../util.js';

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
  panel2_types = [ 'd' /*devices*/, 'p' /*pianoroll*/ ];
  panel3_types = [ 'i' /*info*/, 'b' /*browser*/ ];
  constructor (vue_app) {
    // super();
    this.vue_app = vue_app;
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
    Object.defineProperty (globalThis, 'Shell', { value: this.shell });
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
  async load_project_checked (project_or_path) {
    const err = await this.load_project (project_or_path);
    if (err !== Ase.Error.NONE)
      App.async_button_dialog ("Project Loading Error",
			       "Failed to open project.\n" +
			       project_or_path + ":\n" +
			       await Ase.server.error_blurb (err), [
				 { label: 'Dismiss', autofocus: true },
			       ], 'ERROR');
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
	    await newproject.name (project_or_path.replace (/.*\//, ''));
	  }
      }
    const mtrack = await newproject.master_track();
    const tracks = await newproject.list_tracks();
    // shut down old project
    if (Data.project)
      {
	this.notifynameclear();
	App.open_piano_roll (undefined);
	Data.project.stop_playback();
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
    if (this.shell)
      this.shell.update();
    return Ase.Error.NONE;
  }
  async save_project (projectpath) {
    Shell.show_spinner();
    const self_contained = false;
    let error = !Data.project ? Ase.Error.INTERNAL :
		  Data.project.save_dir (projectpath, self_contained);
    error = await error;
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
    return this.shell.async_modal_dialog (dialog_setup);
  }
  zmoves_add = ZMove.zmoves_add;
  zmove = ZMove.zmove;
}

// == addvc ==
export async function create_app() {
  if (globalThis.App)
    return globalThis.App;
  // prepare Vue component templates
  for (const [__name, component] of Object.entries (VueComponents))
    if (component.sfc_template) // also constructs Shell.template
      component.template = component.sfc_template.call (null, Util.tmplstr, null);
  // create and configure Vue App
  const vue_app = Vue.createApp (ShellClass); // must have Shell.template
  vue_app.config.compilerOptions.isCustomElement = tag => !!window.customElements.get (tag);
  vue_app.config.compilerOptions.whitespace = 'preserve';
  // common globals
  const global_properties = {
    CONFIG: globalThis.CONFIG,
    debug: globalThis.debug,
    Util: globalThis.Util,
    Ase: globalThis.Ase,
    window: globalThis.window,
    document: globalThis.document,
    observable_from_getters: Util.observable_from_getters,
  };
  Object.assign (vue_app.config.globalProperties, global_properties);
  // register directives, mixins, components
  for (let directivename in Util.vue_directives) // register all utility directives
    vue_app.directive (directivename, Util.vue_directives[directivename]);
  for (let mixinname in Util.vue_mixins)         // register all utility mixins
    vue_app.mixin (Util.vue_mixins[mixinname]);
  for (const [name, component] of Object.entries (VueComponents))
    if (component !== ShellClass)
      vue_app.component (name, component);
  // create main App instance
  const app = new AppClass (vue_app);
  console.assert (app === globalThis.App);
  return globalThis.App;
}
