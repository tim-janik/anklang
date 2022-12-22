<!-- This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0 -->

<docs>
  # B-SHELL
  User interface shell for editing and display of a Ase.Project.
  Instance access is provided via the global `Shell` constant.
  ## Props:
  *project*
  : Implicit *Ase.Project*, using App.project().
</docs>

<style lang="scss">
@import 'mixins.scss';
@import 'spinner.scss';

.b-shell {
  position: relative;
  --b-resize-handle-thickness: #{$b-resize-handle-thickness};
  --b-transition-fast-slide: #{$b-transition-fast-slide};
  width: 100%;
  height: 100%;
  justify-content: space-between;
  align-items: stretch;
  user-select: none;
}
.b-shell-panel1 {
  @include b-panel-box;
  flex-grow: 1;
}
.b-shell-panel2 {
  @include b-panel-box;
  height: 19em;
  flex-shrink: 0;
}
.b-shell-sidebar {
  padding: 3px;
  /* overflow: hidden scroll; */
  text-overflow: ellipsis; white-space: nowrap;
}
.b-shell-resizer {
  width: var(--b-resize-handle-thickness);
  background: $b-resize-handle-bgcolor;
  border-left: $b-resize-handle-border;
  border-right: $b-resize-handle-border;
  cursor: col-resize;
}
html.b-shell-during-drag .b-app {
  .b-shell-resizer { background: $b-resize-handle-hvcolor; }
  * { cursor: col-resize !important; user-select: none !important; }
}

.b-shell .-modal-message {
  .-hfooter {
    justify-content: space-between;
    button, push-button {
      white-space: nowrap;
      $hpadding: 0.75em;
      padding-left: $hpadding; padding-right: $hpadding;
    }
    &.-manybuttons {
      width: 100%;
      button, push-button {
	width: 100%;
      }
    }
  }
}

.b-shell {
  .-fullcoverage {
    position: fixed; top: 0; left: 0; bottom: 0; right: 0;
    width: 100%; height: 100%;
    display: flex;
    pointer-events: none;
  }
}
#b-shell-spinner-layer {
  display: flex;
  img {
    $size: 4em;
    width: $size; height: $size;
    margin: auto; display: inline-block; vertical-align: middle;
    animation: 1.1s linear infinite reverse spinner-svg-rotation-steps;
  }
}

  .b-shell {
    grid-template-columns: min(10em,12%) 1fr min(10em,12%);
    grid-template-rows:
      [menubar] 0fr
      [tracklist] 1fr
      [devices] min-content
      [pianoroll] min-content
      [row5] 0fr
      [row6] 0fr
      [row7] 0fr
      [row8] 0fr
      [statusbar] 0fr;
    place-items: stretch;
    place-content: space-between;
    .-col123		{ grid-column: 1/4; }
    .-col12		{ grid-column: 1/3; }
    .-col1		{ grid-column: 1/2; }
    .-col2		{ grid-column: 2/3; }
    .-col3		{ grid-column: 3/4; }
    .-row1 		{ grid-row: 1/2; }
    .-row2 		{ grid-row: 2/3; }
    .-row3 		{ grid-row: 3/4; }
    .-row4 		{ grid-row: 4/5; }
    .-row5 		{ grid-row: 5/6; }
    .-row6 		{ grid-row: 6/7; }
    .-row7 		{ grid-row: 7/8; }
    .-row8 		{ grid-row: 8/9; }
    .-row9 		{ grid-row: 9/10; }
    .-row28 		{ grid-row: 2/9; }
    .-small		{ max-height: 10em; }
  }
</style>

<template>
  <c-grid class="b-shell" >

    <!-- Menus and Transport -->
    <b-menubar class="-row1 -col123" :project="Data.project" />

    <!-- tracks and clips -->
    <b-tracklist class="-row2 -col2" style="overflow: hidden" :project="Data.project"></b-tracklist>

    <!-- devices -->
    <b-devicepanel class="-row3 -col2" style="overflow: auto visible; padding: 0 0 3px 0" v-show="Data.panel2 == 'd'" :track="Data.current_track" />

    <!-- piano roll -->
    <b-piano-roll class="-row4 -col2" style="overflow: hidden; height:50vh" :clip="Data.piano_roll_source" v-show="Data.panel2 == 'p'" ></b-piano-roll>

    <!-- browser -->
    <v-flex class="b-shell-sidebar -row28 -col1" style="width:10em" >
      Browser <br />
      <b-treeselector :tree="m.filetree" v-show="Data.panel3 == 'b'" ></b-treeselector>
      <span v-show="Data.panel3 == 'i'" ><a href="">Info Panel</a></span>
    </v-flex>

    <!-- Inspector -->
    <v-flex class="-row28 -col3" style="margin-left: 3em" >
      ||| <br />
      Editor <br />
      ||| <br />
    </v-flex>

    <!-- status bar -->
    <b-statusbar class="-row9 -col123" />

    <!-- Modal Dialogs -->
    <div class="-fullcoverage" style="z-index: 90" id="b-app-shell-modaldialogs" >
      <b-aboutdialog v-model:shown="Data.show_about_dialog" />
      <b-preferencesdialog v-model:shown="Data.show_preferences_dialog" />
      <b-filedialog :shown="!!fs.resolve" :title="fs.title" :filters="fs.filters" :button="fs.button"
		    :cwd="fs.cwd" @close="fs.resolve()" @select="fs.resolve($event)" />

      <!-- Modal Message Popups -->
      <b-dialog class="-modal-message" v-for="d in m.modal_dialogs"
		:id="'MDialog_' + d.dialogid" :class="d.class" :key="d.dialogid"
		:shown="d.visible.value" @update:shown="d.input ($event)"
		:exclusive="true" bwidth="9em" style="z-index: 93" >
	<template v-slot:header>
	  {{ d.header }}
	</template>
	<template v-slot:default>
	  <h-flex style="justify-content: flex-start; align-items: center;">
	    <b-icon v-bind="d.icon" />
	    <div style="flex-grow: 1; white-space: pre-line;" >{{ d.body }}</div>
	    <div style="flex-grow: 1; white-space: pre-line;" v-html="d.vhtml" ></div>
	  </h-flex>
	  <b-fed-object class="-modal-fed" ref="fedobject" v-if="d.proplist" :value="d.proplist" />
	  <div class="-div-handler" v-if="d.div_handler"></div>
	</template>
	<template v-slot:footer>
	  <h-flex class="-hfooter" :class="d.footerclass">
	    <component v-for="(b, i) in d.buttons" :key="i" @click="d.click (i)" :disabled="b.disabled"
		       :is="b.canfocus ? 'button' : 'push-button'" :autofocus="b.autofocus" >{{ b.label }}</component>
	  </h-flex>
	</template>
      </b-dialog>
    </div>

    <!-- Noteboard -->
    <b-noteboard ref="noteboard" style="z-index: 95" />

    <!-- Bubbles -->
    <div class="-fullcoverage" style="z-index: 96" id="b-shell-bubble-layer" ></div>

    <!-- Spinners (busy indicator) -->
    <div class="-fullcoverage" style="z-index: 98" id="b-shell-spinner-layer" v-show="m.show_spinner_count > 0" >
      <img ref="spinner" src="assets/spinner.svg" >
    </div>

  </c-grid>
</template>

<script>
import * as Util from '../util.js';
import * as Envue from './envue.js';
import DataBubbleIface from '../b/databubble.js';

async function list_sample_files() {
  // TODO: const crawler = await Ase.server.resource_crawler();
  const entries = []; // TODO: await crawler.list_files ('wave', 'user-downloads');
  return Object.freeze ({ entries: entries });
}

function observable_project_data () { // yields reactive Proxy object
  const data = {
    filetree:	     { default: {}, getter: c => list_sample_files(), },
    usernotehook:    { notify: n => Ase.server.on ("usernote", this.usernote), },
    observable_force_update: Util.observable_force_update,
  };
  return this.observable_from_getters (data, () => Data.project);
}

class ShellClass extends Envue.Component {
  data_bubble = null;
  constructor (vm) {
    super (vm);
    this.fs = Vue.reactive ({ title: 'File Selector', button: 'Select', cwd: 'MUSIC', filters: [], resolve: null });
    this.m = observable_project_data.call (vm);
    this.m.modal_dialogs = [];
    this.m.show_spinner_count = 0;
    this.note_cache = {};
    this.piano_current_clip = null;
    this.piano_current_tick = null;
  }
  show_spinner() {
    this.m.show_spinner_count++;
  }
  hide_spinner() {
    console.assert (this.m.show_spinner_count > 0);
    this.m.show_spinner_count--;
  }
  created() {
    this.m.observable_force_update();
  }
  mounted() {
    this.data_bubble = new DataBubbleIface();
    this.switch_panel2 = App.switch_panel2.bind (App);
    Util.add_hotkey ('RawBackquote', this.switch_panel2);
    this.switch_panel3 = App.switch_panel3.bind (App);
    Util.add_hotkey ('I', this.switch_panel3);
  }
  unmounted() {
    Util.remove_hotkey ('RawBackquote', this.switch_panel2);
    Util.remove_hotkey ('I', this.switch_panel3);
    App.shell_unmounted();
  }
  updated() {
    for (const d of this.m.modal_dialogs)
      if (d.div_handler) {
	const dialog = this.$vm.$el.querySelector (`#MDialog_${d.dialogid}`);
	if (dialog) {
	  const div = dialog.querySelector (`.-div-handler`);
	  if (div)
	    d.div_handler (div, dialog);
	}
      }
  }
  usernote (user_note_event) {
    this.create_note (user_note_event.text);
  }
  create_note (...args) {
    const noteboard = Util.envue_object (this.$vm.$refs.noteboard);
    return noteboard?.create_note (...args);
  }
  sidebar_mouse (e) {
    const sidebar = this.$refs.sidebarcontainer;
    console.assert (sidebar);
    const html_classes = document.documentElement.classList;
    if (e.type == 'mousedown' && !this.listening)
      {
	this.listening = Util.debounce (this.sidebar_mouse.bind (this));
	document.addEventListener ('mousemove', this.listening);
	document.addEventListener ('mouseup', this.listening);
	this.startx = e.clientX; //  - e.offsetX;
	this.startwidth = sidebar.getBoundingClientRect().width;
	html_classes.add ('b-shell-during-drag');
      }
    if (this.listening && e.type == 'mouseup')
      {
	document.removeEventListener ('mousemove', this.listening);
	document.removeEventListener ('mouseup', this.listening);
	this.listening = undefined;
	html_classes.remove ('b-shell-during-drag');
      }
    let newwidth = this.startwidth - (e.clientX - this.startx);
    const pwidth = sidebar.parentElement.getBoundingClientRect().width;
    const maxwidth = pwidth * 0.6 |0, minwidth = 120;
    if (newwidth < minwidth / 2)
      {
	const cs = getComputedStyle (sidebar);
	newwidth = parseInt (cs.getPropertyValue ('--b-resize-handle-thickness'), 10);
      }
    else
      newwidth = Util.clamp (newwidth, minwidth, maxwidth);
    sidebar.style.transition = newwidth > minwidth ? "" : "width var(--b-transition-fast-slide)";
    const flexwidth = '0 0 ' + (newwidth / pwidth) * 100 + '%';
    if (flexwidth != sidebar.style.flex)
      sidebar.style.flex = flexwidth;
    // Resize via: https://www.w3.org/TR/css-flexbox-1/#flex-common
    e.preventDefault();
  }
  async select_file (opt = {}) {
    if (this.fs.resolve)
      return undefined;
    Object.assign (this.fs, opt);
    let resolve;
    const fileselector_promise = new Promise (r => resolve = r);
    this.fs.resolve = path => { // assignment shows file selector
      this.fs.resolve = null;   // assignment hides file selector
      resolve (path);
    };
    return await fileselector_promise;
  }
  async_modal_dialog = async_modal_dialog;
  // == note_cache ==
  async _update_note_cache (clip) {
    const cache = this.note_cache[clip.$id];
    while (cache.dirty) {
      cache.dirty = 0;
      const notes = await clip.list_all_notes();
      cache.notes = Util.freeze_deep (notes);
      cache.rgen.value += 1; // trigger Vue reactivity hooks
      for (const cb of cache.callbacks_)
	cb();
    }
    cache.promise = null;
  }
  get_note_cache (clip) {
    if (!this.note_cache[clip.$id]) {
      const cache = { rgen: Vue.ref (1), // generational counter, Vue reactive
		      destroynotify: null, promise: null, dirty: 0,
		      callbacks_: [],
		      notes: Object.freeze ([]) };
      const update_note_cache = () => {
	cache.dirty++;
	if (!cache.promise)
	  cache.promise = this._update_note_cache (clip);
      };
      cache.destroynotify = clip.on ("notify:notes", update_note_cache);
      this.note_cache[clip.$id] = cache;
      update_note_cache();
    }
    const cache = this.note_cache[clip.$id];
    return Object.freeze ({
      get gen() {
	return cache.rgen.value;	// notifies Vue reactivity hooks
      },
      get notes() {
	// use cache.rgen to notify Vue reactivity hooks
	return cache.rgen.value && cache.notes;
      },
      add_callback (cb) { cache.callbacks_.push (cb); },
      del_callback (cb) { return Util.array_remove (cache.callbacks_, cb); },
    });
  }
  async note_cache_notes (clip) {
    this.get_note_cache (clip);
    const cache = this.note_cache[clip.$id];
    await cache.promise;
    return cache.rgen.value && cache.notes;
  }
  old_cache_notes (clip) {
    return this.get_note_cache (clip).notes;
  }
}
export default ShellClass.vue_export ({ sfc_template });

// == modal dialog creation ==
let modal_dialog_counter = 1;
function async_modal_dialog (dialog_setup) {
  const shell = this;
  let resolve;
  const promise = new Promise (r => resolve = r);
  const m = {
    dialogid: modal_dialog_counter++,
    div_handler: dialog_setup.div_handler,
    class: dialog_setup.class,
    proplist: dialog_setup.proplist || [],
    visible: Vue.reactive ({ value: false }),
    input (v) {
      if (!this.visible.value || v)
	return;
      this.visible.value = false;
      if (dialog_setup.destroy)
	dialog_setup.destroy();
      resolve (this.result);
      setTimeout (_ => Util.array_remove (shell.m.modal_dialogs, this), CONFIG.transitiondelay);
    },
    result: -1,
    click (r) {
      this.result = r;
      this.input (false);
    },
    header: dialog_setup.title,
    body: dialog_setup.text,
    vhtml: dialog_setup.html,
    icon: dialog_emblems[dialog_setup.emblem] || {},
    footerclass: '',
    buttons: []
  };
  const is_string = s => typeof s === 'string' || s instanceof String;
  const check_bool = (v, dflt) => v !== undefined ? !!v : dflt;
  const buttons = dialog_setup.buttons;
  for (let i = 0; i < buttons.length; i++)
    {
      const label = is_string (buttons[i]) ? buttons[i] : buttons[i].label;
      const disabled = check_bool (buttons[i].disabled, false);
      const canfocus = check_bool (buttons[i].canfocus, true);
      const autofocus = check_bool (buttons[i].autofocus, false);
      const button = { label, disabled, autofocus, canfocus };
      m.buttons.push (button);
    }
  if (m.buttons.length >= 2)
    m.footerclass = '-manybuttons';
  shell.m.modal_dialogs.push (m);
  setTimeout (_ => m.visible.value = true, 0); // changing value triggers animation
  return promise;
}
const dialog_emblems = {
  PIANO:	{ mi: "piano",			style: "font-size: 300%; padding-right: 1rem; float: left; color: #ffbbbb" },
  QUESTION:	{ fa: "question-circle",	style: "font-size: 300%; padding-right: 1rem; float: left; color: #538cc1" },
  ERROR:	{ fa: "times-circle",		style: "font-size: 300%; padding-right: 1rem; float: left; color: #cc2f2a" },
  KEYBOARD:	{ mi: "keyboard",		style: "font-size: 300%; padding-right: 1rem; float: left; color: #538cc1" },
};
</script>
