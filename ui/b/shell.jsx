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

import { createSignal, createEffect, onMount, onCleanup, For, Show } from 'solid-js'; // createStore

import * as Signal from "../signal.js";
import * as Util from "../util.js";
import * as Ase from '../../ase/gen/api-jsonipc.g.ts';
import * as Dom from "../dom.js";
import DataBubbleIface from '../b/databubble.js';
import spinner_svg from '/assets/spinner.svg'
import { ModalDialogs } from './modals.jsx';
import { AboutDialog } from './aboutdialog.jsx';

// == STYLE ==
Extra_css`
/* global.scss includes @import 'spinner.css'; */

.b-shell {
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

.b-shell {
  .-fullcoverage {
    position: fixed; inset: 0;
    width: 100%; height: 100%;
    display: flex;
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
`;

// == SHELL TEMPLATE ==
export function ShellTemplate (props)
{
  // Shell global
  const t = new BShell ();
  Object.defineProperty (globalThis, 'Shell', { value: t });
  const { r, fs } = t;
  return (
    <div class="b-shell" ref={e => t.setup (e)}>
      {/* Menus and Transport */}
      <b-menubar class="-row1 -col123" project={Data.project}></b-menubar>

      {/* tracks and clips */}
      <b-tracklist class="-row2 -col2" style="overflow: hidden" project={Data.project}></b-tracklist>

      {/* devices */}
      <b-devicepanel class="-row3 -col2" hidden={Data.panel2 !== 'd'} track={App.current_track}></b-devicepanel>

      {/* piano roll */}
      <b-piano-roll class="-row4 -col2" style="overflow: hidden; height:50vh" clip={Data.piano_roll_source}
		    ref={e => t.piano_roll_ = e} hidden={Data.panel2 !== 'p'}></b-piano-roll>

      {/* browser */}
      <div class="b-shell-sidebar vflex -row28 -col1">
	Browser <br />
        <b-treebrowser tree={r.filetree} hidden={Data.panel3 == 'b'}></b-treebrowser>
        <Show when={Data.panel3 !== 'i'}>
          <span><a href="">Info Panel</a></span>
        </Show>
      </div>

      {/* Inspector */}
      <div class="vflex -row28 -col3" style="margin-left: 3em">
	||| <br />
	Editor <br />
	||| <br />
      </div>

      {/* status bar */}
      <b-statusbar class="-row9 -col123"></b-statusbar>

      {/* Other Dialogs */}
      <Show when={r.show_about_dialog_}>
        <AboutDialog onClose={() => r.show_about_dialog_ = false} />
      </Show>

      <b-preferencesdialog shown={Data.show_preferences_dialog} onClose={(ev) => (Data.show_preferences_dialog = false)}></b-preferencesdialog>

      <b-crawlerdialog shown={r.fs_shown} title={fs.title} filters={fs.filters} button={fs.button}
        existing={fs.existing} cwd={fs.cwd} onClose={e => fs.resolve()} onSelect={e => fs.resolve (e.detail?.uri)}></b-crawlerdialog>

      {/* Modal Message Popups */}
      <ModalDialogs ref={e => t.modal_dialogs_ = e} />

      {/* Noticeboard */}
      <b-noticeboard style="z-index: 95"></b-noticeboard>

      {/* Bubbles */}
      <div class="pointer-events-none fixed inset-0 flex h-full w-full" style="z-index: 96" id="b-shell-bubble-layer"></div>

      {/* Spinners (busy indicator) */}
      <Show when={r.show_spinner_count > 0}>
        <div class="pointer-events-none fixed inset-0 flex h-full w-full" style="z-index: 98" id="b-shell-spinner-layer">
          <img src={spinner_svg} />
        </div>
      </Show>
    </div>
  );
}

// == BShell controller ==
class BShell extends Object {
  constructor (input_r = {})
  {
    super();
    this.piano_roll_ = null;
    this.data_bubble = null;
    this.modal_dialogs_ = null;
    this.switch_panel2_ = null;
    this.switch_panel3_ = null;
    this.f1_help_ = null;
    this.fs = { title: 'File Selector', button: 'Select', cwd: '~MUSIC', filters: [] };
    this.r = input_r;
    this.r.fs_shown = false;
    this.r.show_spinner_count = 0;
    this.r.filetree = list_sample_files();
    this.r.show_about_dialog_ = false;
    this.piano_current_clip_tickfn = [null,null];
    this.r = make_reactive (this.r);
    this.usernotehook_ = Ase.server.on ("usernote", user_note_event => this.show_notice (user_note_event.text));
    onCleanup (this.cleanup.bind (this)); // needs constructor() during render()
  }
  /// Called when ShellTemplate is destroyed
  cleanup()
  {
    Util.remove_hotkey ('RawBackquote', this.switch_panel2_);
    Util.remove_hotkey ('I', this.switch_panel3_);
    Util.remove_key_filter (112, this.f1_help_); // F1
    App.shell_unmounted();
    this.usernotehook_();
    this.usernotehook_ = null;
  }
  /// Called when ShellTemplate is instantiated
  setup (shell_element)
  {
    this.shell_element = shell_element;
    this.switch_panel2_ = App.switch_panel2.bind (App);
    Util.add_hotkey ('RawBackquote', this.switch_panel2_);
    this.switch_panel3_ = App.switch_panel3.bind (App);
    Util.add_hotkey ('I', this.switch_panel3_);
    this.f1_help_ = this.f1_help.bind (this);
    Util.add_key_filter (112, this.f1_help_); // F1
    console.assert (!this.data_bubble);
    this.data_bubble = new DataBubbleIface (shell_element);
  }
  /// Helper to refer to the current piano roll clip and tick
  piano_current (clip = undefined, tickfn = undefined)
  {
    // Called several times per second
    if (clip === undefined)
      return this.piano_current_clip_tickfn;
    this.piano_current_clip_tickfn = [clip,tickfn];
  }
  /// Show/hide about dialog
  show_about_dialog (onoff = true)
  {
    this.r.show_about_dialog_ = !!onoff;
  }
  /// Access PianoRoll component
  get piano_roll() { return Shell.piano_roll_; }
  /// Show spinner to indicate "busy" state (increment reason)
  show_spinner()
  {
    this.r.show_spinner_count++;
  }
  /// Hide spinner to indicate "busy" state (decrement reason)
  hide_spinner()
  {
    console.assert (this.r.show_spinner_count > 0);
    this.r.show_spinner_count--;
  }
  /// Show a notification notice, with adequate default timeout
  show_notice (text, timeout = undefined)
  {
    /**@type{any}*/
    const b_noticeboard = this.shell_element.querySelector ('b-noticeboard');
    console.assert (b_noticeboard);
    b_noticeboard.create_note (text, timeout);
  }
  /// Open related help page in another window on F1
  f1_help (event)
  {
    const zlast = App.zmove_last();
    const el_f1 = Util.find_element_from_point (document, zlast.pageX, zlast.pageY, el => {
      const str = el.getAttribute ('data-f1');
      return /(#|\.htm)/.test (str); // check for documentation links / anchors
    });
    const data_f1 = el_f1 && el_f1.getAttribute ('data-f1') || '#using-anklang';
    const u = location.origin + '/anklang/' + data_f1;
    window.open (u, '_blank');
    Util.prevent_event (event);
    return true;
  }
  /// Drag and resize sidebar handle
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
  /// Show file selector dialog
  async select_file (opt = {})
  {
    if (this.r.fs_shown)
      return undefined;
    Object.assign (this.fs, opt);
    this.fs.existing === false || (this.fs.existing = true);
    return new Promise (resolve => {
      this.fs.resolve = path => {
	this.r.fs_shown = false;	// hide file selector
	resolve (path);
      };
      this.r.fs_shown = true;		// show file selector
    });
  }
  /// Create dialog via BModalDialogs.async_modal_dialog()
  async async_modal_dialog (...args)
  {
    return this.modal_dialogs_.async_modal_dialog (...args);
  }
};

/// Crawl to find relevant files for the tree browser
async function list_sample_files() {
  // TODO: const crawler = await Ase.server.resource_crawler();
  const entries = []; // TODO: await crawler.list_files ('wave', 'user-downloads');
  return Object.freeze ({ entries: entries });
}
