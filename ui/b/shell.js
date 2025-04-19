// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class B-SHELL
 * User interface shell for editing and display of a Ase.Project.
 * Instance access is provided via the global `Shell` constant.
 *
 * ### Properties:
 * *project*
 * : Implicit *Ase.Project*, using App.project().
 */

import { LitComponent, html, css, ref, repeat, JsExtract } from '../little.js';
import * as Util from "../util.js";
import * as Ase from '../aseapi.js';
import * as Dom from "../dom.js";
import DataBubbleIface from '../b/databubble.js';

// == STYLE ==
Extra_css`
/* global.scss includes @import 'spinner.scss'; */

b-shell {
  display: grid;
  position: relative;
  --b-resize-handle-thickness: var(--b-resize-handle-thickness);
  --b-transition-fast-slide: var(--b-transition-fast-slide);
  width: 100%;
  height: 100%;
  justify-content: space-between;
  align-items: stretch;
  user-select: none;
}
.b-shell-sidebar {
  padding: 3px;
  /* overflow: hidden scroll; */
  text-overflow: ellipsis; white-space: nowrap;
}
.b-shell-resizer {
  width: var(--b-resize-handle-thickness);
  background: var(--b-resize-handle-bgcolor);
  border-left: var(--b-resize-handle-border);
  border-right: var(--b-resize-handle-border);
  cursor: col-resize;
}
html.b-shell-during-drag .b-app {
  .b-shell-resizer { background: var(--b-resize-handle-hvcolor); }
  * { cursor: col-resize !important; user-select: none !important; }
}

b-shell .-modal-message {
  .-hfooter {
    justify-content: space-between;
    button, push-button {
      white-space: nowrap;
      --hpadding: 0.75em;
      padding-left: var(--hpadding); padding-right: var(--hpadding);
    }
    &.-manybuttons {
      width: 100%;
      button, push-button {
	width: 100%;
      }
    }
  }
}

b-shell {
  .-fullcoverage {
    position: fixed; inset: 0;
    width: 100%; height: 100%;
    display: flex;
    pointer-events: none;
  }
}
#b-shell-spinner-layer {
  display: flex;
  img {
    --size: 4em;
    width: var(--size); height: var(--size);
    margin: auto; display: inline-block; vertical-align: middle;
    animation: 1.1s linear infinite reverse spinner-svg-rotation-steps;
  }
}

b-shell {
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
`;

// == HTML ==
const HTML = (t, m, fs) => [ html`
  <!-- Menus and Transport -->
  <b-menubar class="-row1 -col123" .project=${Data.project} ></b-menubar>

  <!-- tracks and clips -->
  <b-tracklist class="-row2 -col2" style="overflow: hidden" .project=${Data.project}></b-tracklist>

  <!-- devices -->
  <b-devicepanel class="-row3 -col2" ?hidden=${Data.panel2 == 'd'} .track=${App.current_track}></b-devicepanel>

  <!-- piano roll -->
  <b-piano-roll class="-row4 -col2" style="overflow: hidden; height:50vh" .clip=${Data.piano_roll_source}
    ${ref (h => t.piano_roll_ = h)} ?hidden=${Data.panel2 == 'p'}></b-piano-roll>

  <!-- browser -->
  <div class="b-shell-sidebar vflex -row28 -col1" style="width:10em">
    Browser <br />
    <b-treebrowser .tree=${m.filetree} ?hidden=${Data.panel3 == 'b'}></b-treebrowser>
    <span ?hidden=${Data.panel3 == 'i'}><a href="">Info Panel</a></span>
  </div>

  <!-- Inspector -->
  <div class="vflex -row28 -col3" style="margin-left: 3em">
    ||| <br />
    Editor <br />
    ||| <br />
  </div>

  <!-- status bar -->
  <b-statusbar class="-row9 -col123" ></b-statusbar>

  <!-- Modal Dialogs -->
`, html`
  <b-aboutdialog ${ref (h => t.aboutdialog_ = h)} ?shown=${t.show_about_dialog_} @close=${ev => t.show_about_dialog (0)}></b-aboutdialog>
`, html`
  <b-preferencesdialog ?shown=${Data.show_preferences_dialog} @close=${ev => (Data.show_preferences_dialog = false)} ></b-preferencesdialog>
  <b-crawlerdialog ?shown=${!!fs.resolve} .title=${fs.title} .filters=${fs.filters} .button=${fs.button}
    ?existing="fs.existing" .cwd=${fs.cwd} @close=${ev => fs.resolve()} @select=${event => fs.resolve (event.detail?.uri)} ></b-crawlerdialog>
  <div class="-fullcoverage" style="z-index: 90" id="b-app-shell-modaldialogs">
    <!-- Modal Message Popups -->
    ${ DIALOGS_HTML (t) }
  </div>

  <!-- Noticeboard -->
  <b-noticeboard style="z-index: 95" ></b-noticeboard>

  <!-- Bubbles -->
  <div class="-fullcoverage" style="z-index: 96" id="b-shell-bubble-layer"></div>

  <!-- Spinners (busy indicator) -->
  <div class="-fullcoverage" style="z-index: 98" id="b-shell-spinner-layer" ?hidden=${m.show_spinner_count > 0}>
    <img src="assets/spinner.svg" />
  </div>
` ];
// ^^^ FIXME: ref @update:shown v-html autofocus  :*props*  .prop  v-*
const DIALOGS_HTML = (t) =>
  t.m.modal_dialogs.map (d => html`
    <dialog class="-modal-message"
	      .id=${'MDialog_' + d.dialogid} .class=${d.class} .key=${d.dialogid}
	      .shown=${d.visible} @close=${event => d.input (event)}
	      .exclusive=${true} bwidth="9em" style="z-index: 93">
      <template v-slot:header>
	{{ d.header }}
      </template>
      <template v-slot:default>
	<div class="hflex" style="justify-content: flex-start; align-items: center;">
	  <b-icon v-bind="d.icon" ></b-icon>
	  <div style="flex-grow: 1; white-space: pre-line;">{{ d.body }}</div>
	  <div style="flex-grow: 1; white-space: pre-line;" v-html="d.vhtml"></div>
	</div>
	<b-fed-object class="-modal-fed" ?shown=${d.proplist} .value=${d.proplist} ></b-fed-object>
	<div class="-div-handler" ?shown=${d.div_handler}></div>
      </template>
      <template v-slot:footer>
	<div class="hflex -hfooter" .class=${d.footerclass}>
	  <component v-for="(b, i) in d.buttons" .key=${i} @click=${ev => d.click(i)} .disabled=${b.disabled}
	    .is=${b.canfocus ? 'button' : 'push-button'} .autofocus=${b.autofocus}>{{ b.label }}</component>
	</div>
      </template>
    </dialog>
  `);

// == SCRIPT ==
async function list_sample_files() {
  // TODO: const crawler = await Ase.server.resource_crawler();
  const entries = []; // TODO: await crawler.list_files ('wave', 'user-downloads');
  return Object.freeze ({ entries: entries });
}

class BShell extends LitComponent {
  static properties = {
    fs: 		{ state: true },
    m: 			{ state: true },
    note_cache: 	{ state: true },
    piano_current_clip: { state: true },
    piano_current_tick: { state: true },
    show_about_dialog_: { state: true },
  };
  render()
  {
    return HTML (this, this.m, this.fs);
  }
  createRenderRoot()
  {
    this.classList.add ("b-shell");
    return this;
  }
  constructor (input_m = {})
  {
    super();
    this.data_bubble = null;
    this.aboutdialog_ = null;
    this.switch_panel2 = null;
    this.switch_panel3 = null;
    this.f1_help_ = null;
    this.fs = { title: 'File Selector', button: 'Select', cwd: '~MUSIC', filters: [], resolve: null };
    this.m = input_m;
    this.m.modal_dialogs = [];
    this.m.show_spinner_count = 0;
    this.m.filetree = list_sample_files();
    this.m.usernotehook = Ase.server.on ("usernote", this.usernote); // FIXME
    this.note_cache = {};
    this.piano_current_clip = null;
    this.piano_current_tick = null;
    this.m.show_about_dialog_ = false; // FIXME
    this.piano_roll_ = null;
  }
  /// Access PianoRoll component
  get piano_roll() { return Shell.piano_roll_; }
  /// Show spinner to indicate "busy" state (increment reason)
  show_spinner()
  {
    this.m.show_spinner_count++;
  }
  /// Hide spinner to indicate "busy" state (decrement reason)
  hide_spinner()
  {
    console.assert (this.m.show_spinner_count > 0);
    this.m.show_spinner_count--;
  }
  firstUpdated()
  {
    super.firstUpdated();
    this.switch_panel2 = App.switch_panel2.bind (App);
    Util.add_hotkey ('RawBackquote', this.switch_panel2);
    this.switch_panel3 = App.switch_panel3.bind (App);
    Util.add_hotkey ('I', this.switch_panel3);
    this.f1_help_ = this.f1_help.bind (this);
    Util.add_key_filter (112, this.f1_help_); // F1
  }
  connectedCallback()
  {
    super.connectedCallback();
  }
  disconnectedCallback()
  {
    super.disconnectedCallback();
    Util.remove_hotkey ('RawBackquote', this.switch_panel2);
    Util.remove_hotkey ('I', this.switch_panel3);
    Util.remove_key_filter (112, this.f1_help_); // F1
    App.shell_unmounted();
  }
  updated (changedprops)
  {
    if (!this.data_bubble) {
      this.data_bubble = new DataBubbleIface (this);
    }
    for (const d of this.m.modal_dialogs)
      if (d.div_handler) {
        const dialog = this.shadowRoot.querySelector(`#MDialog_${d.dialogid}`); // FIXME
        if (dialog) {
          const div = dialog.querySelector (`.-div-handler`);
          if (div)
	    d.div_handler (div, dialog);
        }
      }
  }
  show_about_dialog (onof = undefined)
  {
    if (undefined === onof)
      return this.show_about_dialog_;
    onof = !!onof;
    if (onof !== this.show_about_dialog_) {
      document.startViewTransition (async () => {
	this.show_about_dialog_ = onof;
	const aboutdialog_promise = onof ? null : this.aboutdialog_?.close_dialog();
	this.requestUpdate();
	await Promise.all ([ this.updateComplete, aboutdialog_promise ]);
      });
    }
  }
  usernote (user_note_event)
  {
    App.show_notice (user_note_event.text);
  }
  f1_help (event)
  {
    const zlast = App.zmove_last();
    const el_f1 = Util.find_element_from_point (document, zlast.pageX, zlast.pageY, el => {
      const str = el.getAttribute ('data-f1');
      return str && str[0] === '#'; // treat as anchor into manual
    });
    const data_f1 = el_f1 && el_f1.getAttribute ('data-f1') || '#using-anklang';
    const u = location.origin + '/doc/anklang-manual.html' + data_f1;
    window.open (u, '_blank');
    Util.prevent_event (event);
    return true;
  }
  sidebar_mouse (e)
  {
    const sidebar = this.shadowRoot.querySelector('.b-shell-sidebar'); // FIXME
    // const sidebar = this.$refs.sidebarcontainer; // FIXME
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
  async select_file (opt = {})
  {
    if (this.fs.resolve)
      return undefined;
    Object.assign (this.fs, opt);
    this.fs.existing === false || (this.fs.existing = true);
    let resolve;
    const fileselector_promise = new Promise (r => resolve = r);
    this.fs.resolve = path => { // assignment shows file selector
      this.fs.resolve = null;   // reset hides file selector
      resolve (path);
    };
    return await fileselector_promise;
  }
  // == note_cache ==
  async _update_note_cache (clip)
  {
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
  get_note_cache (clip)
  {
    if (!this.note_cache[clip.$id]) {
      const cache = {
	rgen: { value: 1 }, // TODO: RM, was generational counter, Vue reactive
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
  async note_cache_notes (clip)
  {
    this.get_note_cache (clip);
    const cache = this.note_cache[clip.$id];
    await cache.promise;
    return cache.rgen.value && cache.notes;
  }
  old_cache_notes (clip)
  {
    return this.get_note_cache (clip).notes;
  }
  // == async_modal_dialog ==
  async_modal_dialog = async_modal_dialog;
}
customElements.define ('b-shell', BShell);

// == modal dialog creation ==
let modal_dialog_counter = 1;
function async_modal_dialog (dialog_setup)
{
  const shell = this;
  let resolve;
  const promise = new Promise (r => resolve = r);
  const [get_visible, set_visible] = Signal.createSignal (false);
  const m = {
    dialogid: modal_dialog_counter++,
    div_handler: dialog_setup.div_handler,
    class: dialog_setup.class,
    proplist: dialog_setup.proplist || [],
    get visible() { return get_visible(); },
    set visible (v) { set_visible (v); },
    input (v) {
      if (!this.visible || v)
	return;
      this.visible = false;
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
  setTimeout (_ => m.visible = true, 0); // changing value triggers animation
  return promise;
}
const dialog_emblems = {
  PIANO:	{ mi: "piano",			style: "font-size: 300%; padding-right: 1rem; float: left; color: #ffbbbb" },
  QUESTION:	{ fa: "question-circle",	style: "font-size: 300%; padding-right: 1rem; float: left; color: #538cc1" },
  ERROR:	{ fa: "times-circle",		style: "font-size: 300%; padding-right: 1rem; float: left; color: #cc2f2a" },
  KEYBOARD:	{ mi: "keyboard",		style: "font-size: 300%; padding-right: 1rem; float: left; color: #538cc1" },
};
