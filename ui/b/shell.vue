<!-- This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0 -->

<docs>
  # B-SHELL
  Shell for editing and display of a Ase.Project.
  ## Props:
  *project*
  : Implicit *Ase.Project*, using App.project().
</docs>

<style lang="scss">
  @import '../styles.scss';

  .b-shell {
    --b-resize-handle-thickness: #{$b-resize-handle-thickness};
    --b-transition-fast-slide: #{$b-transition-fast-slide};
    width: 100%;
    height: 100%;
    justify-content: space-between;
    align-items: stretch;
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
    overflow: hidden scroll;
    &, * { text-overflow: ellipsis; white-space: nowrap; }
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
</style>

<template>
  <v-flex class="b-shell">

    <b-menubar :project="Data.project" :song="Data.song" />

    <h-flex style="overflow: hidden; flex: 1 1 auto">
      <v-flex grow1 shrink1>
	<!-- upper main area -->
	<h-flex class="b-shell-panel1" >
	  <b-track-list class="grow1" :song="Data.song"></b-track-list>
	</h-flex>
	<!-- lower main area -->
	<h-flex class="b-shell-panel2" :style="panel2_style()" >
	  <b-piano-roll class="grow1" :msrc="Data.piano_roll_source" v-show="Data.panel2 == 'p'" ></b-piano-roll>
	  <b-devicepanel v-show="Data.panel2 == 'd'" :track="Data.current_track" />
	</h-flex>
      </v-flex>

      <!-- browser -->
      <h-flex ref="sidebarcontainer" style="width: 0; flex: 0 0 15%" >
	<div     style="flex-grow: 0; flex-shrink: 0" class="b-shell-resizer" @mousedown="sidebar_mouse" ></div>
	<v-flex class="b-shell-sidebar" start shrink1 grow1 >
	  <b-treeselector :tree="m.filetree" v-show="Data.panel3 == 'b'" ></b-treeselector>
	  <span v-show="Data.panel3 == 'i'" >Info Panel</span>
	</v-flex>
      </h-flex>
    </h-flex>

    <!-- status bar -->
    <b-statusbar />

    <!-- Generic modal dialogs -->
    <b-modaldialog v-for="d in m.modal_dialogs" :key="d.uid"
		   :shown="d.visible.value" @update:shown="d.input ($event)"
		   :exclusive="true" bwidth="9em" >
      <template v-slot:header>
	<div>{{ d.header }}</div>
      </template>
      <template v-slot:default>
	<h-flex style="justify-content: flex-start; align-items: center;">
	  <b-icon v-bind="d.icon" />
	  <div style="flex-grow: 1; white-space: pre-line;" >{{ d.body }}</div>
	</h-flex>
      </template>
      <template v-slot:footer>
	<h-flex :style="d.footerstyle" style="justify-content: space-between" >
	  <component v-for="(b, i) in d.buttons" :key="i" @click="d.click (i)" :disabled="b.disabled"
		     :is="b.canfocus ? 'button' : 'push-button'" :autofocus="b.autofocus" >{{ b.label }}</component>
	</h-flex>
      </template>
    </b-modaldialog>

    <!-- Modal Components -->
    <b-aboutdialog v-model:shown="Data.show_about_dialog" />
    <b-preferencesdialog v-model:shown="Data.show_preferences_dialog" />

  </v-flex>
</template>

<script>
import * as Util from '../util.js';
import * as Envue from './envue.js';

async function list_sample_files() {
  // TODO: const crawler = await Ase.server.resource_crawler();
  const entries = []; // TODO: await crawler.list_files ('wave', 'user-downloads');
  return Object.freeze ({ entries: entries });
}

function observable_project_data () { // yields reactive Proxy object
  const data = {
    filetree:	     { default: {}, getter: c => list_sample_files(), },
    usermessagehook: { notify: n => Ase.server.on ("usermessage", this.usermessage), },
    // TODO: tracks: { getter: c => list_tracks.call (this), notify: n => song.on ("treechange", n), },
    // update Data.current_track if tracks change
    __update__: Util.observable_force_update,
  };
  return this.observable_from_getters (data, () => Data.project);
}

class Shell extends Envue.Component {
  constructor (vm) {
    super (vm);
    this.m = observable_project_data.call (vm);
    this.m.modal_dialogs = [];
  }
  created() {
    // provide default project
    App.load_project (Ase.server.last_project());
    // load_project() also forces an update with new Shell properties in place
  }
  mounted() {
    this.switch_panel2 = App.switch_panel2.bind (App);
    Util.add_hotkey ('Backquote', this.switch_panel2);
    this.switch_panel3 = App.switch_panel3.bind (App);
    Util.add_hotkey ('KeyI', this.switch_panel3);
  }
  unmounted() {
    Util.remove_hotkey ('Backquote', this.switch_panel2);
    Util.remove_hotkey ('KeyI', this.switch_panel3);
    App.shell_unmounted();
  }
  update() {
    this.$vm?.$forceUpdate();
    this.m.__update__();
  }
  panel2_style() {
    return Data.panel2 == 'p' ? 'flex-grow: 5' : '';
  }
  usermessage (e) {
    let msg = '';
    switch (e.umtype)
    {
      case Ase.UserMessageType.ERROR:		msg += '### '; break;
      case Ase.UserMessageType.WARNING:	msg += '### '; break;
      case Ase.UserMessageType.INFO:		msg += '# '; break;
      case Ase.UserMessageType.DEBUG:		msg += '##### '; break;
    }
    msg += e.title + '\n';
    msg += e.text1 + '\n';
    msg += e.text2 + '\n';
    if (e.text3)
      msg += '  \n  \n**' + e.text3 + '**\n';
    Util.show_note (msg);
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
  async_modal_dialog = async_modal_dialog;
}

export default Shell.vue_export ({
  sfc_template,
});

// == modal dialog creation ==
let modal_dialog_counter = 1;
function async_modal_dialog (title, btext, buttons = [], icon) {
  const shell = this;
  let resolve;
  const promise = new Promise (r => resolve = r);
  const m = {
    uid: modal_dialog_counter++,
    visible: Vue.reactive ({ value: false }),
    input (v) {
      if (!this.visible.value || v)
	return;
      this.visible.value = false;
      resolve (this.result);
      setTimeout (_ => Util.array_remove (shell.m.modal_dialogs, this), CONFIG.transitiondelay);
    },
    result: -1,
    click (r) {
      this.result = r;
      this.input (false);
    },
    header: title,
    body: btext,
    icon: modal_icons[icon] || {},
    footerstyle: '',
    buttons: []
  };
  const is_string = s => typeof s === 'string' || s instanceof String;
  const check_bool = (v, dflt) => v !== undefined ? !!v : dflt;
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
    m.footerstyle = 'width: 100%;';
  shell.m.modal_dialogs.push (m);
  setTimeout (_ => m.visible.value = true, 0); // changing value triggers animation
  return promise;
}
const modal_icons = {
  QUESTION:	{ fa: "question-circle",	style: "font-size: 300%; padding-right: 1rem; float: left; color: #538cc1" },
  ERROR:	{ fa: "times-circle",		style: "font-size: 300%; padding-right: 1rem; float: left; color: #cc2f2a" },
};
</script>